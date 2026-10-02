/// @file
/// @brief   Disk streaming of a BW64 file's audio for mc.ear.play~: a reader
///          thread keeps a ring buffer filled from a libbw64 reader, the audio
///          thread pulls signal vectors out of it through a routing table.
///
/// Threads and what they own:
///
/// - the main thread hands over an opened reader (`open`) and requests seeks,
///   loop changes and the routing of output channels to file tracks;
/// - the reader thread owns the file: it performs the hand-over, the seeks
///   and the ring resets, and refills the ring in chunks when space frees;
/// - the audio thread pulls frames (`pull`) without blocking: it try_locks
///   the ring mutex, which the reader only holds while it resets the ring,
///   and never allocates or frees. When the lock is busy or the ring is not
///   ready it delivers nothing and the caller outputs silence.
///
/// Positions are frame counters: `m_write_total` / `m_read_total` count
/// frames written to and read from the ring since the last reset (the ring
/// index is the counter modulo the ring size), `m_play_frame` is the frame
/// of the file the next pulled frame comes from, wrapping at the end when
/// looping. No sample-rate conversion is done.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "bw64/bw64.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace earmax::admio {

class bw64_stream {
public:
    static constexpr size_t k_ring_frames = 32768;    ///< ring capacity (0.68 s at 48 kHz)
    static constexpr size_t k_chunk_frames = 4096;    ///< frames read from disk at a time
    static constexpr uint64_t k_no_frame = std::numeric_limits<uint64_t>::max();

    bw64_stream()
    {
        m_thread = std::thread([this] { run(); });
    }

    ~bw64_stream()
    {
        {
            std::lock_guard<std::mutex> lock(m_io_mutex);
            m_running = false;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    bw64_stream(const bw64_stream&) = delete;
    bw64_stream& operator=(const bw64_stream&) = delete;

    // ---- main thread -------------------------------------------------

    /// Hand over an opened file: the reader thread resets the ring and
    /// prefills it from frame 0 (`ready()` turns true when it is done).
    void open(std::unique_ptr<bw64::Bw64Reader> reader)
    {
        m_channels.store(reader ? reader->channels() : 0);
        m_frames.store(reader ? reader->numberOfFrames() : 0);
        m_samplerate.store(reader ? reader->sampleRate() : 0);
        {
            std::lock_guard<std::mutex> lock(m_io_mutex);
            // cleared before the request is visible: the reader thread cannot
            // finish the reset and set them again before these stores land
            m_ready.store(false, std::memory_order_release);
            m_ended.store(false, std::memory_order_release);
            m_pending_reader = std::move(reader);
            m_has_pending_reader = true;
            m_pending_seek = k_no_frame;
        }
        m_cv.notify_all();
    }

    void close()
    {
        open(nullptr);
    }

    /// Request a seek to `frame` (clamped to the file); the ring is reset
    /// and refilled from there.
    void seek(uint64_t frame)
    {
        {
            std::lock_guard<std::mutex> lock(m_io_mutex);
            m_ready.store(false, std::memory_order_release);
            m_ended.store(false, std::memory_order_release);
            m_pending_seek = std::min(frame, m_frames.load());
        }
        m_cv.notify_all();
    }

    void set_loop(bool loop)
    {
        m_loop.store(loop);
        m_cv.notify_all();
    }

    bool loop() const
    {
        return m_loop.load();
    }

    /// Output channel -> file track (-1 for silence), in the order of the
    /// output channels of the pull. Replaces the previous table; the audio
    /// thread never sees a half-written one.
    void set_routing(std::vector<int> routing)
    {
        std::lock_guard<std::mutex> lock(m_ring_mutex);
        m_routing.swap(routing);    // the old table is freed here, after the lock
    }

    uint16_t channels() const
    {
        return m_channels.load();
    }

    uint64_t frames() const
    {
        return m_frames.load();
    }

    uint32_t samplerate() const
    {
        return m_samplerate.load();
    }

    /// The ring was reset and prefilled after the last open or seek.
    bool ready() const
    {
        return m_ready.load(std::memory_order_acquire);
    }

    bool has_file() const
    {
        return m_frames.load() > 0;
    }

    /// The file frame the next pulled frame comes from.
    uint64_t play_frame() const
    {
        return m_play_frame.load(std::memory_order_acquire);
    }

    /// The playback reached the end of the file (without looping).
    bool ended() const
    {
        return m_ended.load(std::memory_order_acquire);
    }

    /// The playback wrapped to the start since the flag was last taken.
    bool take_wrapped()
    {
        return m_wrapped.exchange(false);
    }

    /// Vectors that could not be filled because the disk did not keep up,
    /// since the flag was last taken.
    long take_underruns()
    {
        return m_underruns.exchange(0);
    }

    // ---- audio thread ------------------------------------------------

    /// Copy up to `frames` frames into the output channels through the
    /// routing table and return how many were delivered (the caller fills
    /// the rest with silence). Never blocks, allocates or frees.
    long pull(double** outs, long out_channels, long frames)
    {
        if (!m_ready.load(std::memory_order_acquire) || !m_ring_mutex.try_lock()) {
            return 0;
        }
        const long delivered = pull_locked(outs, out_channels, frames);
        m_ring_mutex.unlock();
        return delivered;
    }

private:
    long pull_locked(double** outs, long out_channels, long frames)
    {
        const uint64_t read_total = m_read_total.load(std::memory_order_relaxed);
        const uint64_t write_total = m_write_total.load(std::memory_order_acquire);
        const uint64_t eof_total = m_eof_total.load(std::memory_order_acquire);
        uint64_t available = write_total - read_total;
        const bool at_eof = eof_total != k_no_frame;
        if (at_eof) {
            available = std::min(available, eof_total - read_total);
        }
        const long n = static_cast<long>(std::min<uint64_t>(available, static_cast<uint64_t>(frames)));
        const size_t channels = m_ring_channels;
        const size_t ring_frames = m_ring.size() / std::max<size_t>(1, channels);

        for (long ch = 0; ch < out_channels; ++ch) {
            const int track = ch < static_cast<long>(m_routing.size()) ? m_routing[static_cast<size_t>(ch)] : -1;
            double* out = outs[ch];
            if (track < 0 || static_cast<size_t>(track) >= channels || n == 0) {
                std::fill(out, out + frames, 0.0);
                continue;
            }
            for (long i = 0; i < n; ++i) {
                const size_t index = static_cast<size_t>((read_total + static_cast<uint64_t>(i)) % ring_frames);
                out[i] = static_cast<double>(m_ring[index * channels + static_cast<size_t>(track)]);
            }
        }

        if (n < frames && !at_eof) {
            m_underruns.fetch_add(1, std::memory_order_relaxed);
        }
        if (n > 0) {
            m_read_total.store(read_total + static_cast<uint64_t>(n), std::memory_order_release);
            uint64_t play = m_play_frame.load(std::memory_order_relaxed) + static_cast<uint64_t>(n);
            const uint64_t total = m_frames.load();
            if (total > 0 && play >= total) {
                if (m_loop.load()) {
                    play %= total;    // the ring holds the start of the file again
                    m_wrapped.store(true, std::memory_order_release);
                }
                else {
                    play = total;
                }
            }
            m_play_frame.store(play, std::memory_order_release);
        }
        if (at_eof && read_total + static_cast<uint64_t>(n) >= eof_total) {
            m_ended.store(true, std::memory_order_release);
        }
        return n;
    }

    // ---- reader thread -----------------------------------------------

    /// The io mutex is held only to take requests and to wait: the disk
    /// reads run without it, so open, seek and close never wait on them
    /// (the file and the chunk buffer are touched by this thread only).
    void run()
    {
        std::unique_lock<std::mutex> lock(m_io_mutex);
        while (m_running) {
            if (m_has_pending_reader) {
                m_reader = std::move(m_pending_reader);
                m_has_pending_reader = false;
                lock.unlock();
                reset_ring(0);
                lock.lock();
                continue;
            }
            if (m_pending_seek != k_no_frame) {
                const uint64_t frame = m_pending_seek;
                m_pending_seek = k_no_frame;
                lock.unlock();
                reset_ring(frame);
                lock.lock();
                continue;
            }
            const bool have_file = m_reader != nullptr;
            if (have_file) {
                lock.unlock();
                const bool filled = fill_chunk();
                lock.lock();
                if (filled) {
                    continue;    // more space may be free: keep filling
                }
            }
            // the ring is full, the file is at its end, or there is no file:
            // wait for a request, or look again shortly for freed space
            m_cv.wait_for(lock, std::chrono::milliseconds(have_file ? 5 : 50));
        }
    }

    /// Reset the ring around `frame`: the audio thread is kept out while the
    /// positions change, then the ring is prefilled.
    void reset_ring(uint64_t frame)
    {
        {
            std::lock_guard<std::mutex> ring_lock(m_ring_mutex);
            m_ready.store(false, std::memory_order_release);
            m_ring_channels = m_reader ? m_reader->channels() : 0;
            const size_t needed = k_ring_frames * m_ring_channels;
            if (m_ring.size() != needed) {
                m_ring.assign(needed, 0.0f);
            }
            m_chunk.resize(k_chunk_frames * std::max<size_t>(1, m_ring_channels));
            m_read_total.store(0, std::memory_order_relaxed);
            m_write_total.store(0, std::memory_order_relaxed);
            m_eof_total.store(k_no_frame, std::memory_order_relaxed);
            m_ended.store(false, std::memory_order_relaxed);
            m_wrapped.store(false, std::memory_order_relaxed);
            m_play_frame.store(std::min(frame, m_frames.load()), std::memory_order_release);
        }
        if (!m_reader) {
            return;
        }
        try {
            m_reader->seek(static_cast<int32_t>(std::min<uint64_t>(frame, static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))));
            m_file_frame = m_reader->tell();
        }
        catch (...) {
            m_reader.reset();
            return;
        }
        while (!superseded() && fill_chunk()) {
        }
        // readiness is published in the same critical section as the check
        // for a newer request: a request that arrived during this reset has
        // cleared the flag, and the audio must stay silent until the reader
        // has performed that one (otherwise the old file or position would
        // play with the new routing or metadata)
        std::lock_guard<std::mutex> lock(m_io_mutex);
        if (m_running && !m_has_pending_reader && m_pending_seek == k_no_frame) {
            m_ready.store(true, std::memory_order_release);
        }
    }

    /// A newer request (or the shutdown) is waiting: the current reset or
    /// prefill should stop.
    bool superseded()
    {
        std::lock_guard<std::mutex> lock(m_io_mutex);
        return !m_running || m_has_pending_reader || m_pending_seek != k_no_frame;
    }

    /// Read one chunk from the file into the ring when there is room for it.
    /// Returns false when nothing was written (ring full, end of file, error).
    bool fill_chunk()
    {
        const size_t channels = m_ring_channels;
        if (channels == 0) {
            return false;
        }
        const uint64_t read_total = m_read_total.load(std::memory_order_acquire);
        const uint64_t write_total = m_write_total.load(std::memory_order_relaxed);
        const uint64_t free_frames = k_ring_frames - (write_total - read_total);
        if (free_frames < k_chunk_frames) {
            return false;
        }
        if (m_eof_total.load(std::memory_order_relaxed) != k_no_frame) {
            if (!m_loop.load()) {
                return false;    // the end was reached and looping is off
            }
            m_eof_total.store(k_no_frame, std::memory_order_release);    // looping was turned on at the end: go on from the start
        }
        uint64_t frames = 0;
        try {
            if (m_reader->eof()) {
                if (!m_loop.load()) {
                    m_eof_total.store(write_total, std::memory_order_release);
                    return false;
                }
                m_reader->seek(0);
                m_file_frame = 0;
            }
            frames = m_reader->read(m_chunk.data(), k_chunk_frames);
            m_file_frame += frames;
        }
        catch (...) {
            m_eof_total.store(write_total, std::memory_order_release);    // treat a read error as the end
            return false;
        }
        if (frames == 0) {
            if (!m_loop.load()) {
                m_eof_total.store(write_total, std::memory_order_release);
            }
            return false;
        }
        for (uint64_t i = 0; i < frames; ++i) {
            const size_t index = static_cast<size_t>((write_total + i) % k_ring_frames);
            std::copy(m_chunk.begin() + static_cast<long>(i * channels), m_chunk.begin() + static_cast<long>((i + 1) * channels),
                      m_ring.begin() + static_cast<long>(index * channels));
        }
        m_write_total.store(write_total + frames, std::memory_order_release);
        return true;
    }

    // requests and the file (io mutex)
    std::mutex m_io_mutex;
    std::condition_variable m_cv;
    bool m_running{ true };
    std::unique_ptr<bw64::Bw64Reader> m_reader;
    std::unique_ptr<bw64::Bw64Reader> m_pending_reader;
    bool m_has_pending_reader{ false };
    uint64_t m_pending_seek{ k_no_frame };
    uint64_t m_file_frame{ 0 };
    std::vector<float> m_chunk;

    // the ring (ring mutex for resets, atomics for the positions)
    std::mutex m_ring_mutex;
    std::vector<float> m_ring;    ///< interleaved frames
    size_t m_ring_channels{ 0 };
    std::vector<int> m_routing;
    std::atomic<uint64_t> m_read_total{ 0 };
    std::atomic<uint64_t> m_write_total{ 0 };
    std::atomic<uint64_t> m_eof_total{ k_no_frame };    ///< write position at which the file ended
    std::atomic<uint64_t> m_play_frame{ 0 };
    std::atomic<bool> m_ready{ false };
    std::atomic<bool> m_ended{ false };
    std::atomic<bool> m_wrapped{ false };
    std::atomic<bool> m_loop{ false };
    std::atomic<long> m_underruns{ 0 };

    // the file's properties, for the main thread
    std::atomic<uint16_t> m_channels{ 0 };
    std::atomic<uint64_t> m_frames{ 0 };
    std::atomic<uint32_t> m_samplerate{ 0 };

    std::thread m_thread;
};

} // namespace earmax::admio
