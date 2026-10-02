/// @file
/// @brief   Disk writing of a BW64 file with ADM for mc.ear.record~: the audio
///          thread pushes signal vectors into a ring buffer, a writer thread
///          drains it to a libbw64 writer in chunks, and when the recording
///          is finished the writer thread builds the ADM document from the
///          captured programme (objects, bed and HOA scene) and closes the
///          file with its chna and axml chunks (libbw64 writes the axml
///          after the data, so the metadata only has to be known at the end).
///
/// Threads and what they own:
///
/// - the main thread requests the start (path, format) and the finish (the
///   captured objects) and reads the outcome;
/// - the writer thread owns the file: it opens it, drains the ring, builds
///   the document and closes the file;
/// - the audio thread pushes frames (`push`) without blocking, allocating or
///   freeing: when the ring is full the vector is dropped and counted.
///
/// Files over 4 GB are written as RF64 by libbw64.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "ear_max_adm.h"

#include "bw64/bw64.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace earmax::admio {

class bw64_sink {
public:
    static constexpr size_t k_ring_frames = 65536;    ///< ring capacity (1.4 s at 48 kHz)
    static constexpr size_t k_chunk_frames = 4096;    ///< frames written to disk at a time

    /// What a finished recording came to.
    struct outcome {
        bool ok{ false };
        std::string error;
        std::vector<std::string> warnings;    ///< metadata written differently from what was captured
        std::string path;
        uint64_t frames{ 0 };
        size_t channels{ 0 };    ///< tracks with metadata (objects, bed and scene)
    };

    bw64_sink()
    {
        m_thread = std::thread([this] { run(); });
    }

    /// A recording still in progress is finished without metadata.
    ~bw64_sink()
    {
        if (recording()) {
            finish({});
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_running = false;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    bw64_sink(const bw64_sink&) = delete;
    bw64_sink& operator=(const bw64_sink&) = delete;

    // ---- main thread -------------------------------------------------

    /// Called from the writer thread when a recording has finished (its
    /// outcome can then be taken); use it to wake the main thread. It must
    /// not call back into the sink. Setting an empty function waits for a
    /// call in progress, so the owner can clear it before going away.
    void set_notify(std::function<void()> fn)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_notify = std::move(fn);
    }

    /// Start a recording: the file is created by the writer thread, the
    /// audio thread's frames count from now. False when one is in progress.
    bool start(const std::string& path, uint16_t channels, uint32_t samplerate, uint16_t bitdepth)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_recording.load() || m_busy.load()) {
            return false;
        }
        m_channels = channels;
        m_samplerate = samplerate;
        const size_t needed = k_ring_frames * channels;
        if (m_ring.size() != needed) {
            m_ring.assign(needed, 0.0f);    // allocated here, never on the audio thread
        }
        m_read_total.store(0, std::memory_order_relaxed);
        m_write_total.store(0, std::memory_order_relaxed);
        m_overruns.store(0, std::memory_order_relaxed);
        m_pending_start = request{ path, channels, samplerate, bitdepth };
        m_has_pending_start = true;
        m_has_outcome.store(false, std::memory_order_release);
        m_busy.store(true, std::memory_order_release);
        m_recording.store(true, std::memory_order_release);
        m_cv.notify_all();
        return true;
    }

    /// Stop taking audio and finish the file with the captured programme as
    /// its ADM (none when it has no channels); the outcome is available when
    /// `has_outcome()`.
    void finish(captured_programme captured)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_recording.load()) {
            return;
        }
        m_recording.store(false, std::memory_order_release);
        m_finish_programme = std::move(captured);
        m_has_pending_finish = true;
        m_cv.notify_all();
    }

    bool recording() const
    {
        return m_recording.load(std::memory_order_acquire);
    }

    /// The writer thread still has a file open (recording or finishing).
    bool busy() const
    {
        return m_busy.load(std::memory_order_acquire);
    }

    bool has_outcome() const
    {
        return m_has_outcome.load(std::memory_order_acquire);
    }

    outcome take_outcome()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_has_outcome.store(false, std::memory_order_release);
        return m_outcome;
    }

    /// Frames the audio thread has handed over since the start: the
    /// recording's clock for the metadata.
    uint64_t frames_pushed() const
    {
        return m_write_total.load(std::memory_order_acquire);
    }

    /// Vectors dropped because the disk did not keep up, since last taken.
    long take_overruns()
    {
        return m_overruns.exchange(0);
    }

    /// Dropped vectors not yet taken (audio thread: to ask for a report).
    long overruns() const
    {
        return m_overruns.load(std::memory_order_relaxed);
    }

    // ---- audio thread ------------------------------------------------

    /// Interleave `frames` frames of the input channels into the ring (a
    /// missing input channel is silent); drops the vector when the ring is
    /// full. Never blocks, allocates or frees.
    void push(double** ins, long in_channels, long frames)
    {
        if (!m_recording.load(std::memory_order_acquire)) {
            return;
        }
        const size_t channels = m_channels;
        if (channels == 0 || m_ring.empty()) {
            return;
        }
        const uint64_t write_total = m_write_total.load(std::memory_order_relaxed);
        const uint64_t read_total = m_read_total.load(std::memory_order_acquire);
        const uint64_t free_frames = k_ring_frames - (write_total - read_total);
        if (free_frames < static_cast<uint64_t>(frames)) {
            m_overruns.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        for (long i = 0; i < frames; ++i) {
            const size_t index = static_cast<size_t>((write_total + static_cast<uint64_t>(i)) % k_ring_frames);
            float* frame = &m_ring[index * channels];
            for (size_t ch = 0; ch < channels; ++ch) {
                frame[ch] = static_cast<long>(ch) < in_channels ? static_cast<float>(ins[ch][i]) : 0.0f;
            }
        }
        m_write_total.store(write_total + static_cast<uint64_t>(frames), std::memory_order_release);
    }

private:
    struct request {
        std::string path;
        uint16_t channels{ 0 };
        uint32_t samplerate{ 48000 };
        uint16_t bitdepth{ 24 };
    };

    void run()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        for (;;) {
            // requests are served before the stop is honoured, so that a
            // finish requested by the destructor still writes the ADM
            if (m_has_pending_start) {
                const request r = m_pending_start;
                m_has_pending_start = false;
                lock.unlock();
                open_file(r);
                lock.lock();
                continue;
            }
            if (m_has_pending_finish) {
                m_has_pending_finish = false;
                const captured_programme captured = std::move(m_finish_programme);
                m_finish_programme = captured_programme();
                lock.unlock();
                finish_file(captured);
                lock.lock();
                continue;
            }
            if (!m_running) {
                break;
            }
            if (m_writer) {
                lock.unlock();
                const bool wrote = drain_chunk(false);
                lock.lock();
                if (wrote) {
                    continue;
                }
            }
            m_cv.wait_for(lock, std::chrono::milliseconds(m_writer ? 5 : 50));
        }
        if (m_writer) {
            drain_chunk(true);
            try {
                m_writer->close();
            }
            catch (...) {
            }
            m_writer.reset();
        }
    }

    void open_file(const request& r)
    {
        try {
            m_writer = std::make_unique<bw64::Bw64Writer>(r.path.c_str(), r.channels, r.samplerate, r.bitdepth,
                                                           std::vector<std::shared_ptr<bw64::Chunk>>{});
            m_path = r.path;
            m_error.clear();
            m_chunk.assign(k_chunk_frames * r.channels, 0.0f);
        }
        catch (const std::exception& e) {
            m_writer.reset();
            m_path = r.path;
            m_error = e.what();    // kept for a finish that was requested before the failure was seen
            outcome result;
            result.ok = false;
            result.error = m_error;
            result.path = r.path;
            m_recording.store(false, std::memory_order_release);
            publish(result);
        }
    }

    /// write one chunk (or everything left when `all`) from the ring;
    /// false when nothing was written
    bool drain_chunk(bool all)
    {
        if (!m_writer) {
            // no file (the open failed): discard what the audio thread pushed
            m_read_total.store(m_write_total.load(std::memory_order_acquire), std::memory_order_release);
            return false;
        }
        const size_t channels = m_channels;
        const uint64_t read_total = m_read_total.load(std::memory_order_relaxed);
        const uint64_t write_total = m_write_total.load(std::memory_order_acquire);
        const uint64_t available = write_total - read_total;
        if (available == 0 || (!all && available < k_chunk_frames)) {
            return false;
        }
        const uint64_t frames = std::min<uint64_t>(available, k_chunk_frames);
        for (uint64_t i = 0; i < frames; ++i) {
            const size_t index = static_cast<size_t>((read_total + i) % k_ring_frames);
            std::copy(m_ring.begin() + static_cast<long>(index * channels), m_ring.begin() + static_cast<long>((index + 1) * channels),
                      m_chunk.begin() + static_cast<long>(i * channels));
        }
        try {
            m_writer->write(m_chunk.data(), frames);
        }
        catch (const std::exception& e) {
            m_error = e.what();
        }
        m_read_total.store(read_total + frames, std::memory_order_release);
        return true;
    }

    void finish_file(const captured_programme& captured)
    {
        outcome result;
        result.path = m_path;
        result.channels = captured.channels();
        if (!m_writer) {
            result.error = m_error.empty() ? "no file was open" : m_error;
            m_error.clear();
            publish(result);
            return;
        }
        while (drain_chunk(true)) {
        }
        try {
            const uint64_t frames = m_writer->framesWritten();
            const double length = m_samplerate ? static_cast<double>(frames) / m_samplerate : 0.0;
            if (captured.channels() > 0) {
                std::vector<bw64::AudioId> chna_ids;
                auto doc = build_document(captured, length, chna_ids, result.warnings);
                auto chna = std::make_shared<bw64::ChnaChunk>();
                for (const auto& id : chna_ids) {
                    chna->addAudioId(id);
                }
                m_writer->setChnaChunk(chna);
                m_writer->setAxmlChunk(std::make_shared<bw64::AxmlChunk>(to_xml(doc)));
            }
            m_writer->close();
            result.frames = frames;
            result.ok = m_error.empty();
            result.error = m_error;
        }
        catch (const std::exception& e) {
            result.ok = false;
            result.error = e.what();
        }
        m_writer.reset();
        m_error.clear();
        publish(result);
    }

    void publish(const outcome& result)
    {
        // the notify runs under the lock, so that set_notify({}) (the
        // owner going away) waits for a call in flight
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outcome = result;
        m_has_outcome.store(true, std::memory_order_release);
        m_busy.store(false, std::memory_order_release);
        if (m_notify) {
            m_notify();
        }
    }

    // requests and the outcome (mutex)
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_running{ true };
    request m_pending_start;
    bool m_has_pending_start{ false };
    bool m_has_pending_finish{ false };
    captured_programme m_finish_programme;
    outcome m_outcome;
    std::function<void()> m_notify;

    // the file (writer thread)
    std::unique_ptr<bw64::Bw64Writer> m_writer;
    std::string m_path;
    std::string m_error;
    std::vector<float> m_chunk;

    // the ring (positions atomic; the buffer is sized by start(), before
    // the audio thread pushes)
    std::vector<float> m_ring;    ///< interleaved frames
    size_t m_channels{ 0 };
    uint32_t m_samplerate{ 48000 };
    std::atomic<uint64_t> m_read_total{ 0 };
    std::atomic<uint64_t> m_write_total{ 0 };
    std::atomic<long> m_overruns{ 0 };
    std::atomic<bool> m_recording{ false };
    std::atomic<bool> m_busy{ false };
    std::atomic<bool> m_has_outcome{ false };

    std::thread m_thread;
};

} // namespace earmax::admio
