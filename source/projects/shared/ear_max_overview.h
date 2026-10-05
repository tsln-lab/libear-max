/// @file
/// @brief   A waveform overview of a BW64 file for display: the minimum and
///          maximum sample of every track per bin of time, scanned from disk
///          on a thread of its own and written into a named buffer~, one
///          channel per track and two samples per bin (the minimum, then the
///          maximum). The buffer's sample rate is set to twice the bins per
///          second, so waveform~ draws the file's own timeline: zoomed out it
///          draws each pixel column from the lowest to the highest sample in
///          it, which is the min-to-max envelope of the bins it covers.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "c74_min.h"

#include "bw64/bw64.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace earmax::admio {

/// The overview of a file: `bins` bins per track, each the minimum and the
/// maximum sample of its span of `samplerate / bins_per_second` frames.
struct overview {
    uint32_t samplerate{ 0 };
    uint16_t channels{ 0 };
    double bins_per_second{ 0.0 };
    size_t bins{ 0 };
    /// 2 * bins frames of `channels` samples, interleaved as a buffer~ holds
    /// them: frame 2b carries the minima of bin b, frame 2b + 1 its maxima
    std::vector<float> data;

    /// the sample rate that makes a buffer~ of this data run on the file's timeline
    double buffer_rate() const
    {
        return 2.0 * bins_per_second;
    }

    float minimum(size_t bin, size_t channel) const
    {
        return data[(2 * bin) * channels + channel];
    }

    float maximum(size_t bin, size_t channel) const
    {
        return data[(2 * bin + 1) * channels + channel];
    }
};

/// Scan `reader` from its first frame and return the overview at
/// `bins_per_second`. The reader is left at the end of the file. Stops early
/// with an empty result when `cancel` becomes true.
inline overview compute_overview(bw64::Bw64Reader& reader, double bins_per_second, const std::atomic<bool>* cancel = nullptr)
{
    constexpr uint64_t k_chunk_frames = 4096;
    overview out;
    out.samplerate = reader.sampleRate();
    out.channels = reader.channels();
    out.bins_per_second = bins_per_second;
    const uint64_t frames = reader.numberOfFrames();
    const size_t channels = out.channels;
    if (frames == 0 || channels == 0 || out.samplerate == 0 || !(bins_per_second > 0.0)) {
        return out;
    }
    // at most one bin per frame, and sizes that fit (a request that would
    // overflow gives nothing rather than a wrapped allocation)
    const double frames_per_bin = std::max(1.0, static_cast<double>(out.samplerate) / bins_per_second);
    const double bins = std::ceil(static_cast<double>(frames) / frames_per_bin);
    constexpr double k_max_elements = static_cast<double>(std::numeric_limits<size_t>::max() / sizeof(float) / 4);
    if (!(bins >= 1.0) || bins > static_cast<double>(frames) || bins * static_cast<double>(channels) * 2.0 > k_max_elements) {
        return overview();
    }
    out.bins = static_cast<size_t>(bins);

    std::vector<float> minima(out.bins * channels, std::numeric_limits<float>::infinity());
    std::vector<float> maxima(out.bins * channels, -std::numeric_limits<float>::infinity());
    std::vector<float> chunk(static_cast<size_t>(k_chunk_frames) * channels);
    reader.seek(0);
    uint64_t frame = 0;
    while (frame < frames) {
        if (cancel && cancel->load(std::memory_order_acquire)) {
            return overview();
        }
        const uint64_t got = reader.read(chunk.data(), std::min<uint64_t>(k_chunk_frames, frames - frame));
        if (got == 0) {
            break;
        }
        for (uint64_t f = 0; f < got; ++f, ++frame) {
            const size_t bin = std::min(out.bins - 1, static_cast<size_t>(static_cast<double>(frame) / frames_per_bin));
            const float* samples = chunk.data() + static_cast<size_t>(f) * channels;
            float* lo = minima.data() + bin * channels;
            float* hi = maxima.data() + bin * channels;
            for (size_t c = 0; c < channels; ++c) {
                lo[c] = std::min(lo[c], samples[c]);
                hi[c] = std::max(hi[c], samples[c]);
            }
        }
    }

    out.data.assign(out.bins * 2 * channels, 0.0f);
    for (size_t b = 0; b < out.bins; ++b) {
        for (size_t c = 0; c < channels; ++c) {
            const float lo = minima[b * channels + c];
            const float hi = maxima[b * channels + c];
            if (lo <= hi) {    // a bin no frame fell into stays silent
                out.data[(2 * b) * channels + c] = lo;
                out.data[(2 * b + 1) * channels + c] = hi;
            }
        }
    }
    return out;
}

/// Runs compute_overview() on a thread of its own, opening the file by path
/// so that a playing stream is not disturbed, and keeps the result for the
/// main thread to take. Nothing here waits on the file: cancelling tells
/// the scan to stop and sets its thread aside, set-aside threads are
/// joined once their scan has finished, and when the scanner goes a
/// thread still reading is disarmed (its completion callback cleared
/// under a lock, so it can never call into a destroyed object) and let
/// finish on its own after a short grace period.
class overview_scanner {
public:
    ~overview_scanner()
    {
        cancel();
        for (auto& retired : m_retired) {
            release(retired.first, retired.second);
        }
        m_retired.clear();
    }

    /// the current scan is in progress
    bool running() const
    {
        return m_scan && !m_scan->done.load(std::memory_order_acquire);
    }

    /// the result of the current scan waits to be taken
    bool ready() const
    {
        return m_scan && !m_scan->taken && m_scan->done.load(std::memory_order_acquire);
    }

    /// Start a scan of `path`; `done` is called from the scan thread when
    /// the result is ready (wake the main thread from it). False while a
    /// scan is running.
    bool start(const std::string& path, double bins_per_second, const std::string& buffer, std::function<void()> done)
    {
        if (running()) {
            return false;
        }
        retire();
        auto scan = std::make_shared<state>();
        scan->path = path;
        scan->buffer = buffer;
        scan->on_done = std::move(done);
        m_scan = scan;
        m_thread = std::thread([scan, bins_per_second] {
            try {
                auto reader = bw64::readFile(scan->path);
                scan->result = compute_overview(*reader, bins_per_second, &scan->cancel);
            }
            catch (const std::exception& e) {
                scan->error = e.what();
                scan->result = overview();
            }
            scan->done.store(true, std::memory_order_release);
            std::lock_guard<std::mutex> lock(scan->callback_mutex);
            if (scan->on_done) {
                scan->on_done();
            }
        });
        return true;
    }

    /// The result of the finished scan (main thread, once ready()); `error`
    /// says why the scan produced nothing when it did not.
    overview take(std::string& error)
    {
        if (!ready()) {
            error.clear();
            return overview();
        }
        m_scan->taken = true;
        if (m_thread.joinable()) {
            m_thread.join();    // done: returns at once
        }
        error = m_scan->error;
        return std::move(m_scan->result);
    }

    /// the buffer~ the current scan is for
    const std::string& buffer() const
    {
        static const std::string none;
        return m_scan ? m_scan->buffer : none;
    }

    /// the file the current scan is of
    const std::string& path() const
    {
        static const std::string none;
        return m_scan ? m_scan->path : none;
    }

    /// Drop the current scan: a running one is told to stop (it ends after
    /// the chunk it is reading) and its thread set aside; nothing is ready
    /// afterwards. Never waits.
    void cancel()
    {
        if (m_scan) {
            m_scan->cancel.store(true, std::memory_order_release);
            m_scan->taken = true;
        }
        retire();
        m_scan.reset();
    }

private:
    struct state {
        std::atomic<bool> cancel{ false };
        std::atomic<bool> done{ false };
        bool taken{ false };    // main thread only
        overview result;
        std::string error;
        std::string path;
        std::string buffer;
        std::mutex callback_mutex;
        std::function<void()> on_done;    // cleared when the owner goes
    };

    /// set the current thread aside and join the set-aside threads that have finished
    void retire()
    {
        if (m_thread.joinable()) {
            m_retired.emplace_back(std::move(m_thread), m_scan);
        }
        for (auto it = m_retired.begin(); it != m_retired.end();) {
            if (!it->second || it->second->done.load(std::memory_order_acquire)) {
                if (it->first.joinable()) {
                    it->first.join();
                }
                it = m_retired.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    /// Let go of a set-aside thread without waiting on its reads: its
    /// callback is cleared so it cannot reach the owner any more, then it
    /// is joined if it finishes within a short grace period (one chunk at
    /// most, normally) and detached otherwise to finish on its own.
    static void release(std::thread& thread, const std::shared_ptr<state>& scan)
    {
        if (scan) {
            scan->cancel.store(true, std::memory_order_release);
            std::lock_guard<std::mutex> lock(scan->callback_mutex);
            scan->on_done = nullptr;
        }
        if (!thread.joinable()) {
            return;
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        while (scan && !scan->done.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!scan || scan->done.load(std::memory_order_acquire)) {
            thread.join();
        }
        else {
            thread.detach();
        }
    }

    std::thread m_thread;
    std::shared_ptr<state> m_scan;
    std::vector<std::pair<std::thread, std::shared_ptr<state>>> m_retired;
};

/// Writes an overview into a named buffer~ (main thread): the buffer is
/// resized to 2 * bins frames of one channel per track and its sample rate
/// set to twice the bins per second.
class overview_buffer {
public:
    explicit overview_buffer(c74::min::object_base* owner)
        : m_owner(owner)
    {
    }

    ~overview_buffer()
    {
        if (m_ref) {
            c74::max::object_free(m_ref);
        }
    }

    overview_buffer(const overview_buffer&) = delete;
    overview_buffer& operator=(const overview_buffer&) = delete;

    bool write(const std::string& name, const overview& data, std::string& error)
    {
        using namespace c74::max;
        t_symbol* sym = gensym(name.c_str());
        if (!m_ref) {
            m_ref = buffer_ref_new(m_owner->maxobj(), sym);
        }
        else {
            buffer_ref_set(m_ref, sym);
        }
        t_buffer_obj* buffer = m_ref ? buffer_ref_getobject(m_ref) : nullptr;
        if (!buffer) {
            error = "no buffer~ named " + name;
            return false;
        }
        const long frames = static_cast<long>(2 * data.bins);
        const long channels = static_cast<long>(data.channels);
        t_atom av[2];
        atom_setfloat(&av[0], data.buffer_rate());
        object_method_typed(buffer, gensym("sr"), 1, av, nullptr);
        atom_setlong(&av[0], frames);
        atom_setlong(&av[1], channels);
        object_method_typed(buffer, gensym("sizeinsamps"), 2, av, nullptr);

        t_buffer_info info{};
        buffer_edit_begin(buffer);
        buffer_getinfo(buffer, &info);
        const bool sized = info.b_samples && info.b_frames == frames && info.b_nchans == channels;
        if (sized) {
            std::copy(data.data.begin(), data.data.end(), info.b_samples);
        }
        buffer_edit_end(buffer, 1);
        if (!sized) {
            error = "could not size buffer~ " + name + " to " + std::to_string(frames) + " samples of " + std::to_string(channels) + " channels";
            return false;
        }
        buffer_setdirty(buffer);
        return true;
    }

    /// forward the owner's notify message (buffer~ binding and changes)
    c74::max::t_max_err notify(c74::max::t_symbol* registration, c74::max::t_symbol* name, void* sender, void* data)
    {
        return m_ref ? c74::max::buffer_ref_notify(m_ref, registration, name, sender, data) : 0;
    }

private:
    c74::min::object_base* m_owner;
    c74::max::t_buffer_ref* m_ref{ nullptr };
};

} // namespace earmax::admio
