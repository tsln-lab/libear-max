/// @file
/// @brief  Shared helpers for the libear-max externals: layout handling,
///         atom conversion and the Objects-type metadata attribute set.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "c74_min.h"

#include "ear/ear.hpp"
#include "ear/decorrelate.hpp"
#include "ear/dsp/dsp.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace earmax {

using namespace c74::min;

/// Layout used when no argument is given.
constexpr const char* k_default_layout = "0+5+0";

/// Names of all ITU-R BS.2051 layouts known to libear.
inline std::vector<std::string> layout_names()
{
    std::vector<std::string> names;
    for (const auto& layout : ear::loadLayouts()) {
        names.push_back(layout.name());
    }
    return names;
}

inline std::string layout_names_joined()
{
    std::string result;
    for (const auto& name : layout_names()) {
        if (!result.empty()) {
            result += " ";
        }
        result += name;
    }
    return result;
}

inline atoms strings_to_atoms(const std::vector<std::string>& values)
{
    atoms result;
    result.reserve(values.size());
    for (const auto& value : values) {
        result.push_back(symbol(value));
    }
    return result;
}

inline atoms floats_to_atoms(const std::vector<float>& values)
{
    atoms result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(static_cast<double>(value));
    }
    return result;
}

inline bool is_symbol(const atom& a)
{
    return a.a_type == c74::max::A_SYM;
}

inline bool is_numeric(const atom& a)
{
    return a.a_type == c74::max::A_FLOAT || a.a_type == c74::max::A_LONG;
}

/// The first (non-attribute) argument as a layout name, or the default.
inline std::string layout_from_args(const atoms& args)
{
    if (!args.empty() && is_symbol(args[0])) {
        return std::string(args[0]);
    }
    return k_default_layout;
}

/// Send `selector arg1 arg2 ...` from an outlet.
inline void send_message(outlet<>& out, const char* selector, const atoms& args = {})
{
    atoms message;
    message.reserve(args.size() + 1);
    message.push_back(symbol(selector));
    message.insert(message.end(), args.begin(), args.end());
    out.send(message);
}

/// Send the channel names of a layout as `channels M+030 M-030 ...`.
inline void send_channels(outlet<>& out, const ear::Layout& layout)
{
    send_message(out, "channels", strings_to_atoms(layout.channelNames()));
}

/// Send one `position <name> <azimuth> <elevation> <distance>` message per channel.
inline void send_positions(outlet<>& out, const ear::Layout& layout)
{
    for (const auto& channel : layout.channels()) {
        const auto position = channel.polarPositionNominal();
        send_message(out, "position",
                     { symbol(channel.name()), position.azimuth, position.elevation, position.distance });
    }
}

/// Send `layouts 0+2+0 0+5+0 ...`.
inline void send_layouts(outlet<>& out)
{
    send_message(out, "layouts", strings_to_atoms(layout_names()));
}

/// Base class for externals that render ADM "Objects" type metadata.
///
/// Holds the loudspeaker layout, the libear gain calculator and the current
/// ObjectsTypeMetadata, and exposes every metadata field as a Max attribute.
/// The derived class implements `metadata_changed()`, which is called on the
/// main thread after any attribute has changed (only once the object is
/// fully constructed).
template <class derived_t>
class objects_base : public object<derived_t> {
public:
    // ------------------------------------------------------------------
    // renderer state (declared before the attributes so that it is
    // constructed before any attribute setter can run)
    // ------------------------------------------------------------------

    ear::Layout m_layout;
    std::unique_ptr<ear::GainCalculatorObjects> m_calc;
    ear::ObjectsTypeMetadata m_otm;
    std::mutex m_calc_mutex;

    /// when true, the `layout` attribute refuses changes (signal objects, whose outlet count is fixed)
    bool m_layout_locked{ false };

    /// when true, attribute setters do not call metadata_changed() (used to batch updates)
    bool m_suppress_notify{ false };

    // metadata mirrors, updated by the attribute setters
    double m_azimuth{ 0.0 };
    double m_elevation{ 0.0 };
    double m_distance{ 1.0 };
    double m_x{ 0.0 };
    double m_y{ 1.0 };
    double m_z{ 0.0 };
    bool m_cartesian{ false };
    double m_divergence{ 0.0 };
    double m_divergence_range{ 45.0 };
    bool m_channellock{ false };
    double m_channellock_distance{ 0.0 };

    size_t channel_count() const
    {
        return m_layout.channels().size();
    }

    /// Replace the layout and gain calculator. Returns false (and posts an
    /// error) if the name is not a BS.2051 layout known to libear.
    bool apply_layout(const std::string& name)
    {
        try {
            ear::Layout new_layout = ear::getLayout(name);
            std::lock_guard<std::mutex> lock(m_calc_mutex);
            m_layout = std::move(new_layout);
            m_calc = std::make_unique<ear::GainCalculatorObjects>(m_layout);
            return true;
        }
        catch (const std::exception& e) {
            this->cerr << e.what() << "; known layouts: " << layout_names_joined() << endl;
            return false;
        }
    }

    /// Calculate direct and diffuse gains for the current metadata.
    /// Returns false (and posts an error) if libear rejects the metadata.
    bool compute_gains(std::vector<float>& direct, std::vector<float>& diffuse)
    {
        std::lock_guard<std::mutex> lock(m_calc_mutex);
        if (!m_calc) {
            return false;
        }
        direct.resize(channel_count());
        diffuse.resize(channel_count());
        try {
            m_calc->calculate(m_otm, direct, diffuse, [this](const ear::Warning& warning) {
                this->cerr << "warning: " << warning.message << endl;
            });
            return true;
        }
        catch (const std::exception& e) {
            this->cerr << e.what() << endl;
            return false;
        }
    }

    /// Rebuild the position / divergence variants from the mirrors.
    void rebuild_position()
    {
        m_otm.cartesian = m_cartesian;
        if (m_cartesian) {
            m_otm.position = ear::CartesianPosition(m_x, m_y, m_z);
            m_otm.objectDivergence = ear::CartesianObjectDivergence(m_divergence, m_divergence_range);
        }
        else {
            m_otm.position = ear::PolarPosition(m_azimuth, m_elevation, m_distance);
            m_otm.objectDivergence = ear::PolarObjectDivergence(m_divergence, m_divergence_range);
        }
    }

    void rebuild_channellock()
    {
        boost::optional<double> max_distance;
        if (m_channellock_distance > 0.0) {
            max_distance = m_channellock_distance;
        }
        m_otm.channelLock = ear::ChannelLock(m_channellock, max_distance);
    }

    /// Notify the derived class, unless suppressed or still constructing.
    void changed()
    {
        if (m_suppress_notify || !this->initialized()) {
            return;
        }
        static_cast<derived_t*>(this)->metadata_changed();
    }

    /// Set the position from a list: `azimuth elevation [distance]` in polar
    /// mode, `x y z` in cartesian mode. Updates the attributes and notifies once.
    void set_position_from_atoms(const atoms& args)
    {
        if (args.size() < 2) {
            this->cerr << "position needs at least 2 numbers: azimuth elevation [distance] or x y z" << endl;
            return;
        }
        for (const auto& a : args) {
            if (!is_numeric(a)) {
                this->cerr << "position values must be numbers" << endl;
                return;
            }
        }

        m_suppress_notify = true;
        if (m_cartesian) {
            x = static_cast<double>(args[0]);
            y = static_cast<double>(args[1]);
            if (args.size() > 2) {
                z = static_cast<double>(args[2]);
            }
        }
        else {
            azimuth = static_cast<double>(args[0]);
            elevation = static_cast<double>(args[1]);
            if (args.size() > 2) {
                distance = static_cast<double>(args[2]);
            }
        }
        m_suppress_notify = false;
        changed();
    }

    // ------------------------------------------------------------------
    // attributes
    // ------------------------------------------------------------------

    attribute<symbol> layout{ this, "layout", k_default_layout,
        description{ "ITU-R BS.2051 loudspeaker layout name (e.g. 0+2+0, 0+5+0, 2+5+0, 4+5+0, 4+9+0, 9+10+3). "
                     "Send 'layouts' to list all names. Signal objects fix the layout at creation time." },
        setter{ MIN_FUNCTION {
            const std::string name = args[0];
            if (m_layout_locked) {
                if (name != m_layout.name()) {
                    this->cerr << "layout cannot be changed after creation; create a new object with the layout as argument" << endl;
                }
                return { symbol(m_layout.name()) };
            }
            if (!apply_layout(name)) {
                return { symbol(m_layout.name()) };
            }
            changed();
            return args;
        } } };

    attribute<bool> cartesian{ this, "cartesian", false,
        description{ "Use cartesian coordinates (x y z) instead of polar (azimuth elevation distance). "
                     "This also selects which divergence parameters are used. "
                     "NOT YET SUPPORTED by libear: enabling it reports an error and keeps the previous gains." },
        setter{ MIN_FUNCTION {
            m_cartesian = static_cast<bool>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> azimuth{ this, "azimuth", 0.0,
        description{ "Polar azimuth in degrees; 0 is front, positive values turn anticlockwise (to the left)." },
        setter{ MIN_FUNCTION {
            m_azimuth = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> elevation{ this, "elevation", 0.0,
        description{ "Polar elevation in degrees; positive values are above the listener." },
        setter{ MIN_FUNCTION {
            m_elevation = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> distance{ this, "distance", 1.0,
        description{ "Polar distance (0 to 1; 1 is at the loudspeakers)." },
        setter{ MIN_FUNCTION {
            m_distance = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> x{ this, "x", 0.0,
        description{ "Cartesian X (-1 left to 1 right)." },
        setter{ MIN_FUNCTION {
            m_x = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> y{ this, "y", 1.0,
        description{ "Cartesian Y (-1 back to 1 front)." },
        setter{ MIN_FUNCTION {
            m_y = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> z{ this, "z", 0.0,
        description{ "Cartesian Z (-1 below to 1 above)." },
        setter{ MIN_FUNCTION {
            m_z = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> width{ this, "width", 0.0,
        description{ "Object extent: width in degrees (polar) or units (cartesian)." },
        setter{ MIN_FUNCTION {
            m_otm.width = static_cast<double>(args[0]);
            changed();
            return args;
        } } };

    attribute<number> height{ this, "height", 0.0,
        description{ "Object extent: height in degrees (polar) or units (cartesian)." },
        setter{ MIN_FUNCTION {
            m_otm.height = static_cast<double>(args[0]);
            changed();
            return args;
        } } };

    attribute<number> depth{ this, "depth", 0.0,
        description{ "Object extent: depth (distance units)." },
        setter{ MIN_FUNCTION {
            m_otm.depth = static_cast<double>(args[0]);
            changed();
            return args;
        } } };

    attribute<number> gain{ this, "gain", 1.0,
        description{ "Linear gain applied to the object." },
        setter{ MIN_FUNCTION {
            m_otm.gain = static_cast<double>(args[0]);
            changed();
            return args;
        } } };

    attribute<number> diffuse{ this, "diffuse", 0.0,
        description{ "Diffuseness from 0 (all direct) to 1 (all diffuse, sent through decorrelation filters)." },
        range{ 0.0, 1.0 },
        setter{ MIN_FUNCTION {
            m_otm.diffuse = static_cast<double>(args[0]);
            changed();
            return args;
        } } };

    attribute<bool> channellock{ this, "channellock", false,
        description{ "Snap the object to the nearest loudspeaker (see channellock_distance)." },
        setter{ MIN_FUNCTION {
            m_channellock = static_cast<bool>(args[0]);
            rebuild_channellock();
            changed();
            return args;
        } } };

    attribute<number> channellock_distance{ this, "channellock_distance", 0.0,
        description{ "Maximum distance to a loudspeaker for channel lock to apply; 0 means unlimited." },
        setter{ MIN_FUNCTION {
            m_channellock_distance = static_cast<double>(args[0]);
            rebuild_channellock();
            changed();
            return args;
        } } };

    attribute<number> divergence{ this, "divergence", 0.0,
        description{ "Object divergence from 0 (none) to 1 (fully split into two virtual sources)." },
        range{ 0.0, 1.0 },
        setter{ MIN_FUNCTION {
            m_divergence = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<number> divergence_range{ this, "divergence_range", 45.0,
        description{ "Divergence spread: azimuth range in degrees (polar) or position range (cartesian)." },
        setter{ MIN_FUNCTION {
            m_divergence_range = static_cast<double>(args[0]);
            rebuild_position();
            changed();
            return args;
        } } };

    attribute<bool> screenref{ this, "screenref", false,
        description{ "Apply screen scaling relative to the default reference screen. "
                     "NOT YET SUPPORTED by libear: enabling it reports an error and keeps the previous gains." },
        setter{ MIN_FUNCTION {
            m_otm.screenRef = static_cast<bool>(args[0]);
            changed();
            return args;
        } } };
};

} // namespace earmax
