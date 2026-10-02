/// @file
/// @brief   ear.objects~: render a mono audio object to loudspeaker signals (signal rate).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "../shared/ear_max.h"
#include "../shared/ear_max_dsp.h"

using namespace earmax;

class ear_objects_tilde : public objects_base<ear_objects_tilde>, public vector_operator<> {
public:
    MIN_DESCRIPTION{ "Render a mono ADM audio object to loudspeaker signals with libear (ITU-R BS.2127). "
                     "One signal outlet per loudspeaker of the BS.2051 layout given as argument. "
                     "Direct and diffuse gains are interpolated; the diffuse part is passed through "
                     "the BS.2127 decorrelation filters and the direct part is delay-compensated." };
    MIN_TAGS{ "spatial audio, ADM, panning, audio" };
    MIN_AUTHOR{ "tsln-lab" };
    MIN_RELATED{ "ear.objects, ear.direct, mc.ear.objects~" };

    inlet<> in_main{ this, "(signal) object audio; (list) azimuth elevation and optional distance, or x y z" };

    ear_objects_tilde(const atoms& args = {})
    {
        const std::string name = layout_from_args(args);
        if (name != m_layout.name() && !apply_layout(name)) {
            apply_layout(k_default_layout);
        }
        layout = symbol(m_layout.name());
        m_layout_locked = true;

        for (const auto& channel : m_layout.channels()) {
            m_outlets.push_back(std::make_unique<outlet<>>(this, "(signal) " + channel.name(), "signal"));
        }

        m_direct.assign(channel_count(), 0.0f);
        m_diffuse.assign(channel_count(), 0.0f);
        compute_gains(m_direct, m_diffuse);
    }

    // ------------------------------------------------------------------
    // attributes and messages
    // ------------------------------------------------------------------

    attribute<number> ramp{ this, "ramp", 10.0,
        description{ "Gain interpolation time in milliseconds when the metadata changes." },
        setter{ MIN_FUNCTION {
            const double ms = std::max(0.0, static_cast<double>(args[0]));
            m_bus.set_ramp_ms(ms);
            return { ms };
        } } };

    attribute<bool> decorrelate{ this, "decorrelate", true,
        description{ "Pass the diffuse part through the BS.2127 decorrelation filters and delay-compensate "
                     "the direct part (adds 255 samples of latency). When off, direct and diffuse gains "
                     "are simply summed." },
        setter{ MIN_FUNCTION {
            m_bus.set_decorrelate(static_cast<bool>(args[0]));
            return args;
        } } };

    message<> list{ this, "list", "Set the position: azimuth elevation and optional distance (polar) or x y z (cartesian).",
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
            m_bus.configure(m_layout, 1, static_cast<size_t>(vector_size()), samplerate());
            m_bus.set_gains_now(0, m_direct, m_diffuse);
            return {};
        } };

    /// called by objects_base after any metadata attribute changed (main thread)
    void metadata_changed()
    {
        if (!compute_gains(m_direct, m_diffuse)) {
            return;
        }
        m_bus.set_targets(0, m_direct, m_diffuse);
    }

    // ------------------------------------------------------------------
    // audio
    // ------------------------------------------------------------------

    void operator()(audio_bundle input, audio_bundle output)
    {
        m_bus.process(input.samples(), static_cast<size_t>(input.channel_count()), output.samples(),
                      static_cast<size_t>(output.channel_count()), static_cast<size_t>(input.frame_count()));
    }

    /// latency in samples introduced by the decorrelation path
    int latency() const
    {
        return m_bus.latency();
    }

private:
    std::vector<std::unique_ptr<outlet<>>> m_outlets;
    std::vector<float> m_direct;
    std::vector<float> m_diffuse;
    bus_renderer m_bus;
};
