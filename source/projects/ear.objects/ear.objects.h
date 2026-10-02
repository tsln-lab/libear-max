/// @file
/// @brief   ear.objects: loudspeaker gains for an ADM audio object (control rate).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "../shared/ear_max.h"

using namespace earmax;

class ear_objects : public objects_base<ear_objects> {
public:
    MIN_DESCRIPTION{ "Calculate loudspeaker gains for an ADM audio object with libear (ITU-R BS.2127). "
                     "Outputs one direct and one diffuse gain per loudspeaker of a BS.2051 layout." };
    MIN_TAGS{ "spatial audio, ADM, panning" };
    MIN_AUTHOR{ "tsln-lab" };
    MIN_RELATED{ "ear.objects~, ear.direct" };

    inlet<> in_main{ this, "(list) azimuth elevation and optional distance, or x y z; (bang) recalculate" };
    outlet<> out_direct{ this, "(list) direct gains, one per loudspeaker" };
    outlet<> out_diffuse{ this, "(list) diffuse gains, one per loudspeaker" };
    outlet<> out_info{ this, "(anything) channels, positions, layouts" };

    ear_objects(const atoms& args = {})
    {
        const std::string name = layout_from_args(args);
        if (name != m_layout.name() && !apply_layout(name)) {
            apply_layout(k_default_layout);
        }
        layout = symbol(m_layout.name());
    }

    attribute<bool> autocalc{ this, "autocalc", true,
        description{ "Recalculate and output the gains whenever the metadata changes. "
                     "When off, send a bang to calculate." } };

    message<> bang{ this, "bang", "Calculate and output the gains.",
        MIN_FUNCTION {
            calculate();
            return {};
        } };

    message<> list{ this, "list", "Set the position: azimuth elevation and optional distance (polar) or x y z (cartesian).",
        MIN_FUNCTION {
            set_position_from_atoms(args);
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

    /// called by objects_base after any metadata attribute changed
    void metadata_changed()
    {
        if (autocalc) {
            calculate();
        }
    }

    void calculate()
    {
        if (!compute_gains(m_direct, m_diffuse)) {
            return;
        }
        out_diffuse.send(floats_to_atoms(m_diffuse));
        out_direct.send(floats_to_atoms(m_direct));
    }

private:
    std::vector<float> m_direct;
    std::vector<float> m_diffuse;
};
