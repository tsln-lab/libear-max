/// @file
/// @brief   ear.objects~: render a mono audio object to loudspeaker signals (signal rate).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include "../shared/ear_max.h"

#include <algorithm>
#include <atomic>
#include <cmath>

using namespace earmax;

class ear_objects_tilde : public objects_base<ear_objects_tilde>, public vector_operator<> {
public:
    MIN_DESCRIPTION{ "Render a mono ADM audio object to loudspeaker signals with libear (ITU-R BS.2127). "
                     "One signal outlet per loudspeaker of the BS.2051 layout given as argument. "
                     "Direct and diffuse gains are interpolated; the diffuse part is passed through "
                     "the BS.2127 decorrelation filters and the direct part is delay-compensated." };
    MIN_TAGS{ "spatial audio, ADM, panning, audio" };
    MIN_AUTHOR{ "tsln-lab" };
    MIN_RELATED{ "ear.objects, ear.direct" };

    inlet<> in_main{ this, "(signal) object audio; (list) azimuth elevation [distance] or x y z" };

    ear_objects_tilde(const atoms& args = {})
    {
        const std::string name = layout_from_args(args);
        if (!apply_layout(name)) {
            apply_layout(k_default_layout);
        }
        layout = symbol(m_layout.name());
        m_layout_locked = true;
        rebuild_position();
        rebuild_channellock();

        m_channels = channel_count();
        for (const auto& channel : m_layout.channels()) {
            m_outlets.push_back(std::make_unique<outlet<>>(this, "(signal) " + channel.name(), "signal"));
        }

        // initial gains, applied without interpolation
        m_target_direct.assign(m_channels, 0.0f);
        m_target_diffuse.assign(m_channels, 0.0f);
        compute_gains(m_target_direct, m_target_diffuse);
        m_current_direct = m_target_direct;
        m_current_diffuse = m_target_diffuse;
        m_step_direct.assign(m_channels, 0.0f);
        m_step_diffuse.assign(m_channels, 0.0f);
    }

    // ------------------------------------------------------------------
    // attributes and messages
    // ------------------------------------------------------------------

    attribute<number> ramp{ this, "ramp", 10.0,
        description{ "Gain interpolation time in milliseconds when the metadata changes." },
        setter{ MIN_FUNCTION {
            const double ms = std::max(0.0, static_cast<double>(args[0]));
            m_ramp_ms = ms;
            return { ms };
        } } };

    attribute<bool> decorrelate{ this, "decorrelate", true,
        description{ "Pass the diffuse part through the BS.2127 decorrelation filters and delay-compensate "
                     "the direct part (adds 255 samples of latency). When off, direct and diffuse gains "
                     "are simply summed." },
        setter{ MIN_FUNCTION {
            m_decorrelate = static_cast<bool>(args[0]);
            return args;
        } } };

    message<> list{ this, "list", "Set the position: azimuth elevation [distance] (polar) or x y z (cartesian).",
        MIN_FUNCTION {
            set_position_from_atoms(args);
            return {};
        } };

    message<> channels{ this, "channels", "Post the channel names of the layout to the Max console.",
        MIN_FUNCTION {
            std::string names;
            for (const auto& name : m_layout.channelNames()) {
                names += name + " ";
            }
            cout << m_layout.name() << ": " << names << endl;
            return {};
        } };

    message<> dspsetup{ this, "dspsetup",
        MIN_FUNCTION {
            rebuild_dsp(static_cast<size_t>(vector_size()));
            return {};
        } };

    /// called by objects_base after any metadata attribute changed (main thread)
    void metadata_changed()
    {
        std::vector<float> direct;
        std::vector<float> diffuse;
        if (!compute_gains(direct, diffuse)) {
            return;
        }
        std::lock_guard<std::mutex> lock(m_gain_mutex);
        m_target_direct = std::move(direct);
        m_target_diffuse = std::move(diffuse);
        m_pending.store(true, std::memory_order_release);
    }

    // ------------------------------------------------------------------
    // audio
    // ------------------------------------------------------------------

    void operator()(audio_bundle input, audio_bundle output)
    {
        const long frames = input.frame_count();
        const size_t n = m_channels;

        if (output.channel_count() < static_cast<long>(n) || frames <= 0) {
            output.clear();
            return;
        }
        if (m_block_size != static_cast<size_t>(frames)) {
            // normally set up by dspsetup; only reached if the vector size changed unexpectedly
            rebuild_dsp(static_cast<size_t>(frames));
        }

        pull_pending_gains();

        const double* in = input.samples(0);

        for (long i = 0; i < frames; ++i) {
            if (m_ramp_remaining > 0) {
                for (size_t ch = 0; ch < n; ++ch) {
                    m_current_direct[ch] += m_step_direct[ch];
                    m_current_diffuse[ch] += m_step_diffuse[ch];
                }
                if (--m_ramp_remaining == 0) {
                    m_current_direct = m_active_direct;
                    m_current_diffuse = m_active_diffuse;
                }
            }
            const float s = static_cast<float>(in[i]);
            for (size_t ch = 0; ch < n; ++ch) {
                m_direct_buf[ch][i] = s * m_current_direct[ch];
                m_diffuse_buf[ch][i] = s * m_current_diffuse[ch];
            }
        }

        if (m_decorrelate && m_delay) {
            m_delay->process(static_cast<size_t>(frames), m_direct_ptrs.data(), m_delayed_ptrs.data());
            for (size_t ch = 0; ch < n; ++ch) {
                m_convolvers[ch]->process(m_diffuse_buf[ch].data(), m_conv_buf[ch].data());
                double* out = output.samples(ch);
                for (long i = 0; i < frames; ++i) {
                    out[i] = static_cast<double>(m_delayed_buf[ch][i]) + static_cast<double>(m_conv_buf[ch][i]);
                }
            }
        }
        else {
            for (size_t ch = 0; ch < n; ++ch) {
                double* out = output.samples(ch);
                for (long i = 0; i < frames; ++i) {
                    out[i] = static_cast<double>(m_direct_buf[ch][i]) + static_cast<double>(m_diffuse_buf[ch][i]);
                }
            }
        }

        // silence any extra outlets Max may hand us
        for (long ch = static_cast<long>(n); ch < output.channel_count(); ++ch) {
            std::fill(output.samples(ch), output.samples(ch) + frames, 0.0);
        }
    }

    /// latency in samples introduced by the decorrelation path
    int latency() const
    {
        return (m_decorrelate && m_delay) ? m_delay->get_delay() : 0;
    }

private:
    /// (Re)allocate block-size dependent DSP state: scratch buffers, delay and decorrelators.
    void rebuild_dsp(size_t block_size)
    {
        if (block_size == 0) {
            return;
        }
        const size_t n = m_channels;

        m_direct_buf.assign(n, std::vector<float>(block_size, 0.0f));
        m_diffuse_buf.assign(n, std::vector<float>(block_size, 0.0f));
        m_delayed_buf.assign(n, std::vector<float>(block_size, 0.0f));
        m_conv_buf.assign(n, std::vector<float>(block_size, 0.0f));
        m_direct_ptrs.resize(n);
        m_delayed_ptrs.resize(n);
        for (size_t ch = 0; ch < n; ++ch) {
            m_direct_ptrs[ch] = m_direct_buf[ch].data();
            m_delayed_ptrs[ch] = m_delayed_buf[ch].data();
        }

        m_delay = std::make_unique<ear::dsp::DelayBuffer>(n, static_cast<size_t>(ear::decorrelatorCompensationDelay()));

        m_convolvers.clear();
        m_filters.clear();
        m_context = std::make_unique<ear::dsp::block_convolver::Context>(block_size, ear::get_fft_kiss<float>());
        const auto filters = ear::designDecorrelators<float>(m_layout);
        for (size_t ch = 0; ch < n; ++ch) {
            m_filters.emplace_back(*m_context, filters[ch].size(), filters[ch].data());
            m_convolvers.push_back(std::make_unique<ear::dsp::block_convolver::BlockConvolver>(*m_context, m_filters.back()));
        }

        m_block_size = block_size;
    }

    /// Adopt newly calculated gains (if any) and start a ramp towards them.
    void pull_pending_gains()
    {
        if (!m_pending.load(std::memory_order_acquire)) {
            return;
        }
        if (!m_gain_mutex.try_lock()) {
            return;    // the main thread is writing; try again next vector
        }
        m_active_direct = m_target_direct;
        m_active_diffuse = m_target_diffuse;
        m_pending.store(false, std::memory_order_release);
        m_gain_mutex.unlock();

        const long ramp_samples = std::max(1L, static_cast<long>(std::lround(m_ramp_ms * samplerate() / 1000.0)));
        for (size_t ch = 0; ch < m_channels; ++ch) {
            m_step_direct[ch] = (m_active_direct[ch] - m_current_direct[ch]) / static_cast<float>(ramp_samples);
            m_step_diffuse[ch] = (m_active_diffuse[ch] - m_current_diffuse[ch]) / static_cast<float>(ramp_samples);
        }
        m_ramp_remaining = ramp_samples;
    }

    size_t m_channels{ 0 };
    std::vector<std::unique_ptr<outlet<>>> m_outlets;

    // gains written by the main thread, read by the audio thread
    std::mutex m_gain_mutex;
    std::atomic<bool> m_pending{ false };
    std::vector<float> m_target_direct;
    std::vector<float> m_target_diffuse;

    // audio-thread gain state
    std::vector<float> m_active_direct;
    std::vector<float> m_active_diffuse;
    std::vector<float> m_current_direct;
    std::vector<float> m_current_diffuse;
    std::vector<float> m_step_direct;
    std::vector<float> m_step_diffuse;
    long m_ramp_remaining{ 0 };
    double m_ramp_ms{ 10.0 };
    std::atomic<bool> m_decorrelate{ true };

    // block-size dependent DSP state
    size_t m_block_size{ 0 };
    std::vector<std::vector<float>> m_direct_buf;
    std::vector<std::vector<float>> m_diffuse_buf;
    std::vector<std::vector<float>> m_delayed_buf;
    std::vector<std::vector<float>> m_conv_buf;
    std::vector<const float*> m_direct_ptrs;
    std::vector<float*> m_delayed_ptrs;
    std::unique_ptr<ear::dsp::DelayBuffer> m_delay;
    std::unique_ptr<ear::dsp::block_convolver::Context> m_context;
    std::vector<ear::dsp::block_convolver::Filter> m_filters;
    std::vector<std::unique_ptr<ear::dsp::block_convolver::BlockConvolver>> m_convolvers;
};

MIN_EXTERNAL(ear_objects_tilde);
