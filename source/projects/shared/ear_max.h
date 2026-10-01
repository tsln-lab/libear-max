/// @file
/// @brief  Shared helpers for the libear-max externals: layout handling,
///         atom conversion and the Objects-type metadata attribute set.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "c74_min.h"

#include "ear/ear.hpp"
#include "ear/decorrelate.hpp"
#include "ear/dsp/dsp.hpp"

#include <cmath>
#include <limits>
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

inline bool atom_is_symbol(const atom& a)
{
    return a.a_type == c74::max::A_SYM;
}

inline bool atom_is_numeric(const atom& a)
{
    return a.a_type == c74::max::A_FLOAT || a.a_type == c74::max::A_LONG;
}

/// The first (non-attribute) argument as a layout name, or the default.
inline std::string layout_from_args(const atoms& args)
{
    if (!args.empty() && atom_is_symbol(args[0])) {
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

/// ADM Objects metadata plus the attribute mirrors needed to rebuild the
/// position and divergence variants; parameters are applied by name so that
/// both the attribute-based externals and the multichannel (mc) externals,
/// which address objects by index, share one implementation.
struct object_metadata {
    ear::ObjectsTypeMetadata otm;

    double azimuth{ 0.0 };
    double elevation{ 0.0 };
    double distance{ 1.0 };
    double x{ 0.0 };
    double y{ 1.0 };
    double z{ 0.0 };
    bool cartesian{ false };
    double divergence{ 0.0 };
    double divergence_range{ 45.0 };
    bool channellock{ false };
    double channellock_distance{ 0.0 };

    object_metadata()
    {
        rebuild_position();
        rebuild_channellock();
    }

    void rebuild_position()
    {
        otm.cartesian = cartesian;
        if (cartesian) {
            otm.position = ear::CartesianPosition(x, y, z);
            otm.objectDivergence = ear::CartesianObjectDivergence(divergence, divergence_range);
        }
        else {
            otm.position = ear::PolarPosition(azimuth, elevation, distance);
            otm.objectDivergence = ear::PolarObjectDivergence(divergence, divergence_range);
        }
    }

    void rebuild_channellock()
    {
        boost::optional<double> max_distance;
        if (channellock_distance > 0.0) {
            max_distance = channellock_distance;
        }
        otm.channelLock = ear::ChannelLock(channellock, max_distance);
    }

    /// Names of all parameters accepted by apply(), for documentation.
    static const char* parameter_names()
    {
        return "azimuth elevation distance x y z cartesian width height depth gain diffuse "
               "channellock channellock_distance divergence divergence_range screenref "
               "screenedgelock_h screenedgelock_v position zone";
    }

    /// Apply a parameter by name. `args` are the parameter's arguments (without
    /// the name). Returns false, after calling `error` with a message, if the
    /// name is unknown or the arguments are invalid; the metadata is unchanged
    /// in that case.
    template <class error_fn>
    bool apply(const std::string& name, const atoms& args, error_fn&& error)
    {
        auto need_number = [&](double& out) {
            if (args.empty() || !atom_is_numeric(args[0])) {
                error(name + " needs a number");
                return false;
            }
            out = static_cast<double>(args[0]);
            return true;
        };
        auto need_bool = [&](bool& out) {
            if (args.empty() || !atom_is_numeric(args[0])) {
                error(name + " needs 0 or 1");
                return false;
            }
            out = static_cast<double>(args[0]) != 0.0;
            return true;
        };

        if (name == "azimuth") {
            if (!need_number(azimuth)) return false;
            rebuild_position();
        }
        else if (name == "elevation") {
            if (!need_number(elevation)) return false;
            rebuild_position();
        }
        else if (name == "distance") {
            if (!need_number(distance)) return false;
            rebuild_position();
        }
        else if (name == "x") {
            if (!need_number(x)) return false;
            rebuild_position();
        }
        else if (name == "y") {
            if (!need_number(y)) return false;
            rebuild_position();
        }
        else if (name == "z") {
            if (!need_number(z)) return false;
            rebuild_position();
        }
        else if (name == "cartesian") {
            if (!need_bool(cartesian)) return false;
            rebuild_position();
        }
        else if (name == "width") {
            return need_number(otm.width);
        }
        else if (name == "height") {
            return need_number(otm.height);
        }
        else if (name == "depth") {
            return need_number(otm.depth);
        }
        else if (name == "gain") {
            return need_number(otm.gain);
        }
        else if (name == "diffuse") {
            return need_number(otm.diffuse);
        }
        else if (name == "channellock") {
            if (!need_bool(channellock)) return false;
            rebuild_channellock();
        }
        else if (name == "channellock_distance") {
            if (!need_number(channellock_distance)) return false;
            rebuild_channellock();
        }
        else if (name == "divergence") {
            if (!need_number(divergence)) return false;
            rebuild_position();
        }
        else if (name == "divergence_range") {
            if (!need_number(divergence_range)) return false;
            rebuild_position();
        }
        else if (name == "screenref") {
            return need_bool(otm.screenRef);
        }
        else if (name == "screenedgelock_h" || name == "screenedgelock_v") {
            const bool horizontal = name == "screenedgelock_h";
            const std::string value = args.empty() ? std::string("none") : std::string(args[0]);
            const bool valid = value == "none" || value.empty() ||
                               (horizontal ? (value == "left" || value == "right")
                                           : (value == "top" || value == "bottom"));
            if (!valid) {
                error(name + (horizontal ? " must be none, left or right" : " must be none, top or bottom"));
                return false;
            }
            boost::optional<std::string>& field = horizontal ? otm.screenEdgeLock.horizontal : otm.screenEdgeLock.vertical;
            if (value == "none" || value.empty()) {
                field = boost::none;
            }
            else {
                field = value;
            }
        }
        else if (name == "position" || name == "list") {
            if (args.size() < 2) {
                error("position needs at least 2 numbers: azimuth elevation [distance] or x y z");
                return false;
            }
            for (const auto& a : args) {
                if (!atom_is_numeric(a)) {
                    error("position values must be numbers");
                    return false;
                }
            }
            if (cartesian) {
                x = static_cast<double>(args[0]);
                y = static_cast<double>(args[1]);
                if (args.size() > 2) z = static_cast<double>(args[2]);
            }
            else {
                azimuth = static_cast<double>(args[0]);
                elevation = static_cast<double>(args[1]);
                if (args.size() > 2) distance = static_cast<double>(args[2]);
            }
            rebuild_position();
        }
        else if (name == "zone") {
            if (args.empty() || !atom_is_symbol(args[0])) {
                error("zone needs 'polar', 'cartesian' or 'clear' as first argument");
                return false;
            }
            const std::string kind = args[0];
            std::vector<float> values;
            for (size_t i = 1; i < args.size(); ++i) {
                if (!atom_is_numeric(args[i])) {
                    error("zone values must be numbers");
                    return false;
                }
                // libear stores zone bounds as float: reject values that would
                // not survive the narrowing (NaN, infinities, out of range)
                const double v = static_cast<double>(args[i]);
                const double max_float = static_cast<double>(std::numeric_limits<float>::max());
                if (!std::isfinite(v) || v < -max_float || v > max_float) {
                    error("zone values must be finite numbers");
                    return false;
                }
                values.push_back(static_cast<float>(v));
            }
            if (kind == "clear") {
                otm.zoneExclusion.zones.clear();
            }
            else if (kind == "polar" && values.size() == 4) {
                otm.zoneExclusion.zones.push_back(
                    ear::PolarExclusionZone{ values[0], values[1], values[2], values[3], 0.0f, 0.0f, "" });
            }
            else if (kind == "cartesian" && values.size() == 6) {
                otm.zoneExclusion.zones.push_back(ear::CartesianExclusionZone{
                    values[0], values[1], values[2], values[3], values[4], values[5], "" });
            }
            else {
                error("zone: use 'zone polar minAz maxAz minEl maxEl', "
                      "'zone cartesian minX maxX minY maxY minZ maxZ' or 'zone clear'");
                return false;
            }
        }
        else {
            error("unknown object parameter '" + name + "'; parameters are: " + parameter_names());
            return false;
        }
        return true;
    }
};

/// Calculate gains for one object with a shared calculator; errors and
/// libear warnings are reported through `report`.
template <class report_fn>
bool compute_object_gains(ear::GainCalculatorObjects& calc, const ear::ObjectsTypeMetadata& otm,
                          std::vector<float>& direct, std::vector<float>& diffuse, report_fn&& report)
{
    try {
        calc.calculate(otm, direct, diffuse, [&](const ear::Warning& warning) {
            report("warning: " + warning.message);
        });
        return true;
    }
    catch (const std::exception& e) {
        report(e.what());
        return false;
    }
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
    object_metadata m_meta;
    std::mutex m_calc_mutex;

    /// when true, the `layout` attribute refuses changes (signal objects, whose outlet count is fixed)
    bool m_layout_locked{ false };

    /// when true, attribute setters do not call metadata_changed() (used to batch updates)
    bool m_suppress_notify{ false };

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
        return compute_object_gains(*m_calc, m_meta.otm, direct, diffuse,
                                    [this](const std::string& message) { this->cerr << message << endl; });
    }

    /// Apply a parameter to the metadata by name (see object_metadata::apply)
    /// and notify the derived class.
    bool apply_parameter(const std::string& name, const atoms& args)
    {
        if (!m_meta.apply(name, args, [this](const std::string& message) { this->cerr << message << endl; })) {
            return false;
        }
        changed();
        return true;
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
        if (!m_meta.apply("position", args, [this](const std::string& message) { this->cerr << message << endl; })) {
            return;
        }
        // reflect the new values in the attributes without re-notifying
        m_suppress_notify = true;
        if (m_meta.cartesian) {
            x = m_meta.x;
            y = m_meta.y;
            z = m_meta.z;
        }
        else {
            azimuth = m_meta.azimuth;
            elevation = m_meta.elevation;
            distance = m_meta.distance;
        }
        m_suppress_notify = false;
        changed();
    }

    // ------------------------------------------------------------------
    // zone exclusion messages
    // ------------------------------------------------------------------

    message<> zone{ this, "zone",
        "Add an exclusion zone: 'zone polar minAzimuth maxAzimuth minElevation maxElevation' "
        "or 'zone cartesian minX maxX minY maxY minZ maxZ'. Loudspeakers inside the zones are not used; "
        "'zone clear' removes all zones.",
        MIN_FUNCTION {
            apply_parameter("zone", args);
            return {};
        } };

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
                     "This also selects which divergence parameters are used." },
        setter{ MIN_FUNCTION {
            apply_parameter("cartesian", args);
            return args;
        } } };

    attribute<number> azimuth{ this, "azimuth", 0.0,
        description{ "Polar azimuth in degrees; 0 is front, positive values turn anticlockwise (to the left)." },
        setter{ MIN_FUNCTION {
            apply_parameter("azimuth", args);
            return args;
        } } };

    attribute<number> elevation{ this, "elevation", 0.0,
        description{ "Polar elevation in degrees; positive values are above the listener." },
        setter{ MIN_FUNCTION {
            apply_parameter("elevation", args);
            return args;
        } } };

    attribute<number> distance{ this, "distance", 1.0,
        description{ "Polar distance (0 to 1; 1 is at the loudspeakers)." },
        setter{ MIN_FUNCTION {
            apply_parameter("distance", args);
            return args;
        } } };

    attribute<number> x{ this, "x", 0.0,
        description{ "Cartesian X (-1 left to 1 right)." },
        setter{ MIN_FUNCTION {
            apply_parameter("x", args);
            return args;
        } } };

    attribute<number> y{ this, "y", 1.0,
        description{ "Cartesian Y (-1 back to 1 front)." },
        setter{ MIN_FUNCTION {
            apply_parameter("y", args);
            return args;
        } } };

    attribute<number> z{ this, "z", 0.0,
        description{ "Cartesian Z (-1 below to 1 above)." },
        setter{ MIN_FUNCTION {
            apply_parameter("z", args);
            return args;
        } } };

    attribute<number> width{ this, "width", 0.0,
        description{ "Object extent: width in degrees (polar) or units (cartesian)." },
        setter{ MIN_FUNCTION {
            apply_parameter("width", args);
            return args;
        } } };

    attribute<number> height{ this, "height", 0.0,
        description{ "Object extent: height in degrees (polar) or units (cartesian)." },
        setter{ MIN_FUNCTION {
            apply_parameter("height", args);
            return args;
        } } };

    attribute<number> depth{ this, "depth", 0.0,
        description{ "Object extent: depth (distance units)." },
        setter{ MIN_FUNCTION {
            apply_parameter("depth", args);
            return args;
        } } };

    attribute<number> gain{ this, "gain", 1.0,
        description{ "Linear gain applied to the object." },
        setter{ MIN_FUNCTION {
            apply_parameter("gain", args);
            return args;
        } } };

    attribute<number> diffuse{ this, "diffuse", 0.0,
        description{ "Diffuseness from 0 (all direct) to 1 (all diffuse, sent through decorrelation filters)." },
        range{ 0.0, 1.0 },
        setter{ MIN_FUNCTION {
            apply_parameter("diffuse", args);
            return args;
        } } };

    attribute<bool> channellock{ this, "channellock", false,
        description{ "Snap the object to the nearest loudspeaker (see channellock_distance)." },
        setter{ MIN_FUNCTION {
            apply_parameter("channellock", args);
            return args;
        } } };

    attribute<number> channellock_distance{ this, "channellock_distance", 0.0,
        description{ "Maximum distance to a loudspeaker for channel lock to apply; 0 means unlimited." },
        setter{ MIN_FUNCTION {
            apply_parameter("channellock_distance", args);
            return args;
        } } };

    attribute<number> divergence{ this, "divergence", 0.0,
        description{ "Object divergence from 0 (none) to 1 (fully split into two virtual sources)." },
        range{ 0.0, 1.0 },
        setter{ MIN_FUNCTION {
            apply_parameter("divergence", args);
            return args;
        } } };

    attribute<number> divergence_range{ this, "divergence_range", 45.0,
        description{ "Divergence spread: azimuth range in degrees (polar) or position range (cartesian)." },
        setter{ MIN_FUNCTION {
            apply_parameter("divergence_range", args);
            return args;
        } } };

    attribute<symbol> screenedgelock_h{ this, "screenedgelock_h", "none",
        description{ "Lock the horizontal position to a screen edge: none, left or right." },
        setter{ MIN_FUNCTION {
            const std::string value = args.empty() ? std::string("none") : std::string(args[0]);
            if (!apply_parameter("screenedgelock_h", args)) {
                const auto& field = m_meta.otm.screenEdgeLock.horizontal;
                return { symbol(field ? *field : "none") };
            }
            return { symbol(value.empty() ? "none" : value) };
        } } };

    attribute<symbol> screenedgelock_v{ this, "screenedgelock_v", "none",
        description{ "Lock the vertical position to a screen edge: none, top or bottom." },
        setter{ MIN_FUNCTION {
            const std::string value = args.empty() ? std::string("none") : std::string(args[0]);
            if (!apply_parameter("screenedgelock_v", args)) {
                const auto& field = m_meta.otm.screenEdgeLock.vertical;
                return { symbol(field ? *field : "none") };
            }
            return { symbol(value.empty() ? "none" : value) };
        } } };

    attribute<bool> screenref{ this, "screenref", false,
        description{ "Apply screen scaling relative to the reference screen (the default ADM screen)." },
        setter{ MIN_FUNCTION {
            apply_parameter("screenref", args);
            return args;
        } } };
};

} // namespace earmax
