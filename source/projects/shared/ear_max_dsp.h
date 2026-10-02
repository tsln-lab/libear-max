/// @file
/// @brief   Signal-rate core shared by the renderer externals: N input
///          channels (objects or bed channels) are mixed onto L loudspeaker
///          channels with interpolated direct/diffuse gains, the diffuse bus
///          runs through the BS.2127 decorrelators and the direct bus is
///          delay-compensated.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "ear/ear.hpp"
#include "ear/decorrelate.hpp"
#include "ear/dsp/dsp.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <vector>

namespace earmax {

/// Gain matrix renderer with per-input linear gain ramps.
///
/// Threading: `configure`, `set_targets` and `set_gains_now` are called from
/// Max's main thread; `process` from the audio thread. New target gains are
/// handed over under a mutex that the audio thread only tries to lock, so it
/// never blocks; it picks the gains up on the next vector if the lock is
/// contended.
class bus_renderer {
public:
    /// Allocate state for `layout` with up to `capacity` inputs at the given
    /// block size and sample rate. Previous gain state is discarded; callers
    /// re-publish their current gains with set_gains_now() afterwards.
    /// `with_decorrelation` allocates the decorrelation filters and the
    /// compensation delay; `with_delay` allocates the compensation delay
    /// alone, for renderers without a diffuse path that must stay aligned
    /// with a decorrelating one (see set_delay_compensation()).
    void configure(const ear::Layout& layout, size_t capacity, size_t block_size, double samplerate,
                   bool with_decorrelation = true, bool with_delay = false)
    {
        allocate(layout.channels().size(), capacity, block_size, samplerate);
        m_with_decorrelation = with_decorrelation;
        if ((with_decorrelation || with_delay) && block_size > 0) {
            m_delay = std::make_unique<ear::dsp::DelayBuffer>(
                m_outputs, static_cast<size_t>(ear::decorrelatorCompensationDelay()));
        }
        if (with_decorrelation && block_size > 0) {
            m_context = std::make_unique<ear::dsp::block_convolver::Context>(block_size, ear::get_fft_kiss<float>());
            const auto filters = ear::designDecorrelators<float>(layout);
            for (size_t ch = 0; ch < m_outputs; ++ch) {
                m_filters.emplace_back(*m_context, filters[ch].size(), filters[ch].data());
                m_convolvers.push_back(
                    std::make_unique<ear::dsp::block_convolver::BlockConvolver>(*m_context, m_filters.back()));
            }
        }
        m_layout = layout;
    }

    /// Allocate a plain gain matrix with `outputs` output channels and neither
    /// decorrelation nor delay, for renderers whose outputs are not
    /// loudspeakers (an ambisonic encoder).
    void configure(size_t outputs, size_t capacity, size_t block_size, double samplerate)
    {
        allocate(outputs, capacity, block_size, samplerate);
        m_with_decorrelation = false;
        m_layout = ear::Layout();
    }

private:
    /// gain state and buffers for `outputs` channels; discards decorrelators and delay
    void allocate(size_t outputs, size_t capacity, size_t block_size, double samplerate)
    {
        m_outputs = outputs;
        m_capacity = std::max<size_t>(1, capacity);
        m_block_size = block_size;
        m_samplerate = samplerate;

        m_objects.clear();
        for (size_t i = 0; i < m_capacity; ++i) {
            m_objects.push_back(std::make_unique<object_gains>(m_outputs));
        }

        m_direct_bus.assign(m_outputs, std::vector<float>(block_size, 0.0f));
        m_diffuse_bus.assign(m_outputs, std::vector<float>(block_size, 0.0f));
        m_delayed.assign(m_outputs, std::vector<float>(block_size, 0.0f));
        m_convolved.assign(m_outputs, std::vector<float>(block_size, 0.0f));
        m_direct_ptrs.resize(m_outputs);
        m_delayed_ptrs.resize(m_outputs);
        for (size_t ch = 0; ch < m_outputs; ++ch) {
            m_direct_ptrs[ch] = m_direct_bus[ch].data();
            m_delayed_ptrs[ch] = m_delayed[ch].data();
        }

        m_convolvers.clear();
        m_filters.clear();
        m_context.reset();
        m_delay.reset();
    }

public:
    bool configured() const
    {
        return m_block_size > 0 && !m_objects.empty();
    }

    size_t outputs() const
    {
        return m_outputs;
    }

    size_t capacity() const
    {
        return m_capacity;
    }

    void set_ramp_ms(double ms)
    {
        m_ramp_ms.store(std::max(0.0, ms));
    }

    void set_decorrelate(bool on)
    {
        m_decorrelate.store(on);
    }

    bool decorrelating() const
    {
        return m_decorrelate.load() && !m_convolvers.empty();
    }

    /// Delay the output by the decorrelator compensation delay without
    /// decorrelating anything, so that this renderer stays time-aligned with
    /// a decorrelating one (the EAR reference aligns all item types this way).
    /// Needs configure() with `with_delay`.
    void set_delay_compensation(bool on)
    {
        m_delay_compensation.store(on);
    }

    bool delay_compensating() const
    {
        return m_delay_compensation.load() && m_delay != nullptr;
    }

    /// latency in samples introduced by the decorrelation path or the delay compensation
    int latency() const
    {
        return (decorrelating() || delay_compensating()) ? m_delay->get_delay() : 0;
    }

    /// New target gains for input `i`; the audio thread ramps to them over
    /// `ramp_ms`, or over the renderer's ramp time when `ramp_ms` is negative.
    void set_targets(size_t i, const std::vector<float>& direct, const std::vector<float>& diffuse, double ramp_ms = -1.0)
    {
        if (i >= m_objects.size()) {
            return;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        copy_gains(direct, m_objects[i]->target_direct);
        copy_gains(diffuse, m_objects[i]->target_diffuse);
        m_objects[i]->target_ramp_ms = ramp_ms;
        m_objects[i]->pending.store(true, std::memory_order_release);
        m_any_pending.store(true, std::memory_order_release);
    }

    /// Gains for input `i` that take effect without a ramp (for initial
    /// values; only call when audio is not running, e.g. from dspsetup).
    void set_gains_now(size_t i, const std::vector<float>& direct, const std::vector<float>& diffuse)
    {
        if (i >= m_objects.size()) {
            return;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        object_gains& o = *m_objects[i];
        copy_gains(direct, o.target_direct);
        copy_gains(diffuse, o.target_diffuse);
        o.active_direct = o.target_direct;
        o.active_diffuse = o.target_diffuse;
        o.current_direct = o.target_direct;
        o.current_diffuse = o.target_diffuse;
        std::fill(o.step_direct.begin(), o.step_direct.end(), 0.0f);
        std::fill(o.step_diffuse.begin(), o.step_diffuse.end(), 0.0f);
        o.ramp_remaining = 0;
        o.pending.store(false, std::memory_order_release);
    }

    /// Audio thread. Inputs beyond the capacity are ignored; outputs beyond
    /// the layout are silenced. A vector size other than the configured block
    /// size produces silence (the DSP chain is expected to call configure()
    /// through dspsetup before the size changes).
    void process(const double* const* ins, size_t num_ins, double* const* outs, size_t num_outs, size_t frames)
    {
        const size_t n_out = std::min(num_outs, m_outputs);
        if (!configured() || frames == 0) {
            for (size_t ch = 0; ch < num_outs; ++ch) {
                std::fill(outs[ch], outs[ch] + frames, 0.0);
            }
            return;
        }
        if (frames != m_block_size) {
            // normally prevented by dspsetup; never reallocate or reset the
            // gain state on the audio thread: output silence until the next
            // dspsetup configures the buffers for the new vector size
            for (size_t ch = 0; ch < num_outs; ++ch) {
                std::fill(outs[ch], outs[ch] + frames, 0.0);
            }
            return;
        }

        pull_pending();

        for (size_t ch = 0; ch < m_outputs; ++ch) {
            std::fill(m_direct_bus[ch].begin(), m_direct_bus[ch].end(), 0.0f);
            std::fill(m_diffuse_bus[ch].begin(), m_diffuse_bus[ch].end(), 0.0f);
        }

        const size_t n_in = std::min(num_ins, m_objects.size());
        for (size_t i = 0; i < n_in; ++i) {
            object_gains& o = *m_objects[i];
            const double* in = ins[i];
            for (size_t n = 0; n < frames; ++n) {
                if (o.ramp_remaining > 0) {
                    for (size_t ch = 0; ch < m_outputs; ++ch) {
                        o.current_direct[ch] += o.step_direct[ch];
                        o.current_diffuse[ch] += o.step_diffuse[ch];
                    }
                    if (--o.ramp_remaining == 0) {
                        o.current_direct = o.active_direct;
                        o.current_diffuse = o.active_diffuse;
                    }
                }
                const float s = static_cast<float>(in[n]);
                if (s == 0.0f && o.ramp_remaining == 0) {
                    continue;
                }
                for (size_t ch = 0; ch < m_outputs; ++ch) {
                    m_direct_bus[ch][n] += s * o.current_direct[ch];
                    m_diffuse_bus[ch][n] += s * o.current_diffuse[ch];
                }
            }
        }

        const bool decorrelate = decorrelating();
        const bool delay = decorrelate || delay_compensating();
        if (m_delay) {
            // keep the delay line running while it is bypassed, so that
            // switching it back on does not replay audio from before the bypass
            m_delay->process(frames, m_direct_ptrs.data(), m_delayed_ptrs.data());
        }
        for (size_t ch = 0; ch < n_out; ++ch) {
            const std::vector<float>& direct = delay ? m_delayed[ch] : m_direct_bus[ch];
            const std::vector<float>* diffuse = &m_diffuse_bus[ch];
            if (decorrelate) {
                m_convolvers[ch]->process(m_diffuse_bus[ch].data(), m_convolved[ch].data());
                diffuse = &m_convolved[ch];
            }
            double* out = outs[ch];
            for (size_t n = 0; n < frames; ++n) {
                out[n] = static_cast<double>(direct[n]) + static_cast<double>((*diffuse)[n]);
            }
        }

        for (size_t ch = n_out; ch < num_outs; ++ch) {
            std::fill(outs[ch], outs[ch] + frames, 0.0);
        }
    }

private:
    struct object_gains {
        explicit object_gains(size_t outputs)
            : target_direct(outputs, 0.0f), target_diffuse(outputs, 0.0f), active_direct(outputs, 0.0f),
              active_diffuse(outputs, 0.0f), current_direct(outputs, 0.0f), current_diffuse(outputs, 0.0f),
              step_direct(outputs, 0.0f), step_diffuse(outputs, 0.0f)
        {
        }

        // written by the main thread under the mutex
        std::vector<float> target_direct;
        std::vector<float> target_diffuse;
        double target_ramp_ms{ -1.0 };    // negative: use the renderer's ramp
        std::atomic<bool> pending{ false };

        // audio-thread state
        std::vector<float> active_direct;
        std::vector<float> active_diffuse;
        std::vector<float> current_direct;
        std::vector<float> current_diffuse;
        std::vector<float> step_direct;
        std::vector<float> step_diffuse;
        long ramp_remaining{ 0 };
    };

    static void copy_gains(const std::vector<float>& from, std::vector<float>& to)
    {
        const size_t n = std::min(from.size(), to.size());
        std::copy(from.begin(), from.begin() + n, to.begin());
        std::fill(to.begin() + n, to.end(), 0.0f);
    }

    void pull_pending()
    {
        if (!m_any_pending.load(std::memory_order_acquire)) {
            return;
        }
        if (!m_mutex.try_lock()) {
            return;    // the main thread is writing; try again next vector
        }
        const double default_ramp_ms = m_ramp_ms.load();
        for (auto& op : m_objects) {
            object_gains& o = *op;
            if (!o.pending.load(std::memory_order_acquire)) {
                continue;
            }
            const double ramp_ms = o.target_ramp_ms >= 0.0 ? o.target_ramp_ms : default_ramp_ms;
            const long ramp_samples = std::max(1L, static_cast<long>(std::lround(ramp_ms * m_samplerate / 1000.0)));
            o.active_direct = o.target_direct;
            o.active_diffuse = o.target_diffuse;
            o.pending.store(false, std::memory_order_release);
            for (size_t ch = 0; ch < m_outputs; ++ch) {
                o.step_direct[ch] = (o.active_direct[ch] - o.current_direct[ch]) / static_cast<float>(ramp_samples);
                o.step_diffuse[ch] = (o.active_diffuse[ch] - o.current_diffuse[ch]) / static_cast<float>(ramp_samples);
            }
            o.ramp_remaining = ramp_samples;
        }
        m_any_pending.store(false, std::memory_order_release);
        m_mutex.unlock();
    }

    ear::Layout m_layout;
    size_t m_outputs{ 0 };
    size_t m_capacity{ 0 };
    size_t m_block_size{ 0 };
    double m_samplerate{ 44100.0 };
    bool m_with_decorrelation{ true };

    std::atomic<double> m_ramp_ms{ 10.0 };
    std::atomic<bool> m_decorrelate{ true };
    std::atomic<bool> m_delay_compensation{ false };

    std::mutex m_mutex;
    std::atomic<bool> m_any_pending{ false };
    std::vector<std::unique_ptr<object_gains>> m_objects;

    std::vector<std::vector<float>> m_direct_bus;
    std::vector<std::vector<float>> m_diffuse_bus;
    std::vector<std::vector<float>> m_delayed;
    std::vector<std::vector<float>> m_convolved;
    std::vector<const float*> m_direct_ptrs;
    std::vector<float*> m_delayed_ptrs;
    std::unique_ptr<ear::dsp::DelayBuffer> m_delay;
    std::unique_ptr<ear::dsp::block_convolver::Context> m_context;
    std::vector<ear::dsp::block_convolver::Filter> m_filters;
    std::vector<std::unique_ptr<ear::dsp::block_convolver::BlockConvolver>> m_convolvers;
};

} // namespace earmax
