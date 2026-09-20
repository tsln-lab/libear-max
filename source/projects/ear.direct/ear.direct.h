/// @file
/// @brief   ear.direct: loudspeaker gains for an ADM DirectSpeakers channel (control rate).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "../shared/ear_max.h"

using namespace earmax;

class ear_direct : public object<ear_direct> {
public:
    MIN_DESCRIPTION{ "Calculate loudspeaker gains for an ADM DirectSpeakers channel with libear (ITU-R BS.2127). "
                     "Maps a channel by speaker label, or by nominal position and bounds, onto a BS.2051 layout." };
    MIN_TAGS{ "spatial audio, ADM, panning" };
    MIN_AUTHOR{ "tsln-lab" };
    MIN_RELATED{ "ear.objects, ear.objects~" };

    // ------------------------------------------------------------------
    // renderer state (declared before the attributes: Min runs the attribute
    // setters while constructing the attributes, so this must exist first)
    // ------------------------------------------------------------------

    ear::Layout m_layout;
    std::unique_ptr<ear::GainCalculatorDirectSpeakers> m_calc;
    ear::DirectSpeakersTypeMetadata m_metadata;
    ear::PolarSpeakerPosition m_position;
    std::vector<double> m_bounds;
    std::vector<float> m_gains;
    bool m_suppress_notify{ false };

    inlet<> in_main{ this, "(list) azimuth elevation [distance]; (speakerlabel) labels; (bang) recalculate" };
    outlet<> out_gains{ this, "(list) gains, one per loudspeaker" };
    outlet<> out_info{ this, "(anything) channels, positions, layouts" };

    ear_direct(const atoms& args = {})
    {
        const std::string name = layout_from_args(args);
        if (name != m_layout.name() && !apply_layout(name)) {
            apply_layout(k_default_layout);
        }
        layout = symbol(m_layout.name());
        rebuild_position();
    }

    // ------------------------------------------------------------------
    // attributes
    // ------------------------------------------------------------------

    attribute<symbol> layout{ this, "layout", k_default_layout,
        description{ "ITU-R BS.2051 loudspeaker layout name (e.g. 0+2+0, 0+5+0, 2+5+0, 4+5+0, 4+9+0, 9+10+3). "
                     "Send 'layouts' to list all names." },
        setter{ MIN_FUNCTION {
            const std::string name = args[0];
            if (!apply_layout(name)) {
                return { symbol(m_layout.name()) };
            }
            changed();
            return args;
        } } };

    attribute<number> azimuth{ this, "azimuth", 0.0,
        description{ "Nominal azimuth of the channel in degrees; 0 is front, positive is anticlockwise." },
        setter{ MIN_FUNCTION {
            m_position.azimuth = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> elevation{ this, "elevation", 0.0,
        description{ "Nominal elevation of the channel in degrees." },
        setter{ MIN_FUNCTION {
            m_position.elevation = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> distance{ this, "distance", 1.0,
        description{ "Nominal distance of the channel (normally 1)." },
        setter{ MIN_FUNCTION {
            m_position.distance = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<bool> lfe{ this, "lfe", false,
        description{ "Mark the channel as LFE (sets a 120 Hz low-pass frequency element), "
                     "so it is routed to an LFE loudspeaker only." },
        setter{ MIN_FUNCTION {
            if (static_cast<bool>(args[0])) {
                m_metadata.channelFrequency.lowPass = 120.0;
            }
            else {
                m_metadata.channelFrequency.lowPass = boost::none;
            }
            changed();
            return args;
        } } };

    attribute<symbol> packformat{ this, "packformat", "",
        description{ "Optional audioPackFormatID of the pack this channel belongs to (e.g. AP_00010003); "
                     "enables pack-specific mapping rules." },
        setter{ MIN_FUNCTION {
            const std::string id = args[0];
            if (id.empty()) {
                m_metadata.audioPackFormatID = boost::none;
            }
            else {
                m_metadata.audioPackFormatID = id;
            }
            changed();
            return args;
        } } };

    attribute<bool> autocalc{ this, "autocalc", true,
        description{ "Recalculate and output the gains whenever the metadata changes. "
                     "When off, send a bang to calculate." } };

    // ------------------------------------------------------------------
    // messages
    // ------------------------------------------------------------------

    message<> bang{ this, "bang", "Calculate and output the gains.",
        MIN_FUNCTION {
            calculate();
            return {};
        } };

    message<> list{ this, "list", "Set the nominal position: azimuth elevation [distance].",
        MIN_FUNCTION {
            if (args.size() < 2) {
                cerr << "position needs at least 2 numbers: azimuth elevation [distance]" << endl;
                return {};
            }
            for (const auto& a : args) {
                if (!is_numeric(a)) {
                    cerr << "position values must be numbers" << endl;
                    return {};
                }
            }
            m_suppress_notify = true;
            azimuth = static_cast<double>(args[0]);
            elevation = static_cast<double>(args[1]);
            if (args.size() > 2) {
                distance = static_cast<double>(args[2]);
            }
            m_suppress_notify = false;
            changed();
            return {};
        } };

    message<> speakerlabel{ this, "speakerlabel",
        "Set the speaker labels of the channel (e.g. M+030, or urn:itu:bs:2051:0:speaker:M+030). "
        "Labels take precedence over the position. Send without arguments to clear.",
        MIN_FUNCTION {
            m_metadata.speakerLabels.clear();
            for (const auto& a : args) {
                m_metadata.speakerLabels.push_back(std::string(a));
            }
            changed();
            return {};
        } };

    message<> bounds{ this, "bounds",
        "Set the position bounds: azimuthMin azimuthMax elevationMin elevationMax [distanceMin distanceMax]. "
        "A loudspeaker within the bounds is used directly. Send without arguments to clear.",
        MIN_FUNCTION {
            if (args.empty()) {
                m_bounds.clear();
                rebuild_position();
                changed();
                return {};
            }
            if (args.size() != 4 && args.size() != 6) {
                cerr << "bounds needs 4 or 6 numbers: azimuthMin azimuthMax elevationMin elevationMax [distanceMin distanceMax]" << endl;
                return {};
            }
            std::vector<double> values;
            for (const auto& a : args) {
                if (!is_numeric(a)) {
                    cerr << "bounds values must be numbers" << endl;
                    return {};
                }
                values.push_back(static_cast<double>(a));
            }
            m_bounds = values;
            rebuild_position();
            changed();
            return {};
        } };

    message<> channels{ this, "channels", "Output the channel names of the layout from the right outlet.",
        MIN_FUNCTION {
            send_channels(out_info, m_layout);
            return {};
        } };

    message<> positions{ this, "positions", "Output the nominal loudspeaker positions from the right outlet.",
        MIN_FUNCTION {
            send_positions(out_info, m_layout);
            return {};
        } };

    message<> layouts{ this, "layouts", "Output the names of all known layouts from the right outlet.",
        MIN_FUNCTION {
            send_layouts(out_info);
            return {};
        } };

    // ------------------------------------------------------------------
    // implementation
    // ------------------------------------------------------------------

    size_t channel_count() const
    {
        return m_layout.channels().size();
    }

    bool apply_layout(const std::string& name)
    {
        try {
            ear::Layout new_layout = ear::getLayout(name);
            m_layout = std::move(new_layout);
            m_calc = std::make_unique<ear::GainCalculatorDirectSpeakers>(m_layout);
            return true;
        }
        catch (const std::exception& e) {
            cerr << e.what() << "; known layouts: " << layout_names_joined() << endl;
            return false;
        }
    }

    bool compute_gains(std::vector<float>& gains)
    {
        if (!m_calc) {
            return false;
        }
        gains.resize(channel_count());
        try {
            m_calc->calculate(m_metadata, gains, [this](const ear::Warning& warning) {
                cerr << "warning: " << warning.message << endl;
            });
            return true;
        }
        catch (const std::exception& e) {
            cerr << e.what() << endl;
            return false;
        }
    }

    void calculate()
    {
        if (!compute_gains(m_gains)) {
            return;
        }
        out_gains.send(floats_to_atoms(m_gains));
    }

    void changed()
    {
        if (m_suppress_notify || !initialized()) {
            return;
        }
        if (autocalc) {
            calculate();
        }
    }

    void rebuild_position()
    {
        ear::PolarSpeakerPosition position(m_position.azimuth, m_position.elevation, m_position.distance);
        if (m_bounds.size() >= 4) {
            position.azimuthMin = m_bounds[0];
            position.azimuthMax = m_bounds[1];
            position.elevationMin = m_bounds[2];
            position.elevationMax = m_bounds[3];
        }
        if (m_bounds.size() >= 6) {
            position.distanceMin = m_bounds[4];
            position.distanceMax = m_bounds[5];
        }
        m_metadata.position = position;
    }

    const ear::Layout& current_layout() const
    {
        return m_layout;
    }

};
