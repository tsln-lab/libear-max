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
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <thread>
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
    const double frames_per_bin = static_cast<double>(out.samplerate) / bins_per_second;
    out.bins = static_cast<size_t>(std::ceil(static_cast<double>(frames) / frames_per_bin));

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
/// main thread to take.
class overview_scanner {
public:
    ~overview_scanner()
    {
        cancel();
    }

    /// a scan is in progress
    bool running() const
    {
        return m_running.load(std::memory_order_acquire);
    }

    /// the result of the last scan waits to be taken
    bool ready() const
    {
        return m_ready.load(std::memory_order_acquire);
    }

    /// Start a scan of `path`; `done` is called from the scan thread when
    /// the result is ready (wake the main thread from it). False while a
    /// scan is running.
    bool start(const std::string& path, double bins_per_second, const std::string& buffer, std::function<void()> done)
    {
        if (running()) {
            return false;
        }
        join();
        m_cancel.store(false, std::memory_order_release);
        m_ready.store(false, std::memory_order_release);
        m_running.store(true, std::memory_order_release);
        m_buffer = buffer;
        m_error.clear();
        m_thread = std::thread([this, path, bins_per_second, done] {
            try {
                auto reader = bw64::readFile(path);
                m_result = compute_overview(*reader, bins_per_second, &m_cancel);
            }
            catch (const std::exception& e) {
                m_error = e.what();
                m_result = overview();
            }
            m_ready.store(true, std::memory_order_release);
            if (done) {
                done();
            }
            m_running.store(false, std::memory_order_release);
        });
        return true;
    }

    /// The result of the finished scan (main thread, once ready()); `error`
    /// says why the scan produced nothing when it did not.
    overview take(std::string& error)
    {
        join();
        m_ready.store(false, std::memory_order_release);
        error = m_error;
        return std::move(m_result);
    }

    /// the buffer~ the running or last scan is for
    const std::string& buffer() const
    {
        return m_buffer;
    }

    /// stop a running scan and wait for its thread
    void cancel()
    {
        m_cancel.store(true, std::memory_order_release);
        join();
    }

private:
    void join()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    std::thread m_thread;
    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_ready{ false };
    std::atomic<bool> m_cancel{ false };
    overview m_result;
    std::string m_error;
    std::string m_buffer;
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
