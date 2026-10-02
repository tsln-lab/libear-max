/// @file
/// @brief   Capturing object metadata for writing, shared by ear.adm and
///          mc.ear.record~: a set of object slots that take the same messages
///          as mc.ear.objects~ (setvalue, applyvalues, parameters, lists) and
///          record every change as a timed block, with the ramp in force at
///          that moment as the block's interpolation. The caller supplies the
///          time of each change (ear.adm from Max's scheduler, mc.ear.record~
///          from the frames recorded so far), so the capture itself knows no
///          clock.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "ear_max_adm.h"

#include "c74_min.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace earmax::admio {

using namespace c74::min;

class object_capture {
public:
    struct slot {
        std::string name;
        object_state state;
        double ramp_ms{ -1.0 };    ///< per-object interpolation time; negative = the default ramp
        std::vector<captured_block> blocks;
    };

    size_t size() const
    {
        return m_slots.size();
    }

    const slot& at(size_t i) const
    {
        return m_slots[i];
    }

    const std::vector<captured_block>& blocks(size_t i) const
    {
        return m_slots[i].blocks;
    }

    /// Number of objects; new ones are named "object N".
    void resize(size_t n)
    {
        const size_t old = m_slots.size();
        m_slots.resize(n);
        for (size_t i = old; i < n; ++i) {
            m_slots[i].name = "object " + std::to_string(i + 1);
        }
    }

    bool set_name(size_t i, const std::string& name)
    {
        if (i >= m_slots.size()) {
            return false;
        }
        m_slots[i].name = name;
        return true;
    }

    /// The interpolation written for a change of an object without a ramp
    /// of its own (the renderers' ramp attribute).
    void set_default_ramp(double ms)
    {
        m_default_ramp_ms = std::max(0.0, ms);
    }

    double default_ramp() const
    {
        return m_default_ramp_ms;
    }

    /// Start capturing at time 0: the current state of every object becomes
    /// its first block.
    void begin()
    {
        m_last = 0.0;
        m_capturing = true;
        for (auto& s : m_slots) {
            s.blocks.clear();
            s.blocks.push_back({ 0.0, 0.0, s.state });
        }
    }

    void end()
    {
        m_capturing = false;
    }

    bool capturing() const
    {
        return m_capturing;
    }

    /// The time of the last captured change, in seconds.
    double last_time() const
    {
        return m_last;
    }

    /// Discard the captured timeline and reset every object's parameters.
    void clear()
    {
        m_capturing = false;
        m_last = 0.0;
        for (auto& s : m_slots) {
            s.blocks.clear();
            s.state = object_state();
            s.ramp_ms = -1.0;
        }
    }

    /// Apply an mc.ear.objects~ parameter to one object (0-based) at `time`
    /// seconds; a change while capturing becomes a block. Returns false with
    /// `error` set when the parameter or its values are invalid.
    bool apply_one(size_t i, const std::string& name, const atoms& values, double time, std::string& error)
    {
        if (i >= m_slots.size()) {
            error = "object number out of range 1.." + std::to_string(m_slots.size());
            return false;
        }
        if (!apply_parameter(m_slots[i], name, values, error)) {
            return false;
        }
        if (name != "ramp") {
            record_change(i, time);
        }
        return true;
    }

    /// Apply a parameter to all objects; stops at the first invalid one (the
    /// same error would repeat for every object).
    bool apply_all(const std::string& name, const atoms& values, double time, std::string& error)
    {
        for (size_t i = 0; i < m_slots.size(); ++i) {
            if (!apply_one(i, name, values, time, error)) {
                return false;
            }
        }
        return true;
    }

    /// The first `count` objects as captured timelines, for build_document;
    /// an object never captured is a static one.
    std::vector<captured_object> objects(size_t count) const
    {
        std::vector<captured_object> out;
        for (size_t i = 0; i < count && i < m_slots.size(); ++i) {
            captured_object o;
            o.name = m_slots[i].name;
            o.blocks = m_slots[i].blocks;
            if (o.blocks.empty()) {
                o.blocks.push_back({ 0.0, 0.0, m_slots[i].state });
            }
            out.push_back(std::move(o));
        }
        return out;
    }

private:
    static bool same_time(double a, double b)
    {
        return std::abs(a - b) < 1e-6;
    }

    void record_change(size_t i, double time)
    {
        if (!m_capturing) {
            return;
        }
        slot& s = m_slots[i];
        const double t = std::max(0.0, time);
        const double ramp_seconds = (s.ramp_ms < 0.0 ? m_default_ramp_ms : s.ramp_ms) / 1000.0;
        m_last = std::max(m_last, t);
        if (!s.blocks.empty() && same_time(s.blocks.back().time, t)) {
            s.blocks.back().state = s.state;    // several parameters at the same time: one block
            s.blocks.back().ramp = ramp_seconds;
            return;
        }
        s.blocks.push_back({ t, ramp_seconds, s.state });
    }

    static bool number_at(const atoms& values, size_t i, double& out, std::string& error, const char* what)
    {
        if (values.size() <= i || !atom_is_numeric(values[i]) || !std::isfinite(static_cast<double>(values[i]))) {
            error = std::string(what) + " needs a finite number";
            return false;
        }
        out = static_cast<double>(values[i]);
        return true;
    }

    /// apply one mc.ear.objects~ parameter to an object's state
    static bool apply_parameter(slot& s_, const std::string& name, const atoms& values, std::string& error)
    {
        object_state& s = s_.state;
        double v = 0.0;
        if (name == "azimuth") return number_at(values, 0, s.azimuth, error, "azimuth");
        if (name == "elevation") return number_at(values, 0, s.elevation, error, "elevation");
        if (name == "distance") return number_at(values, 0, s.distance, error, "distance");
        if (name == "x") return number_at(values, 0, s.x, error, "x");
        if (name == "y") return number_at(values, 0, s.y, error, "y");
        if (name == "z") return number_at(values, 0, s.z, error, "z");
        if (name == "width") return number_at(values, 0, s.width, error, "width");
        if (name == "height") return number_at(values, 0, s.height, error, "height");
        if (name == "depth") return number_at(values, 0, s.depth, error, "depth");
        if (name == "gain") return number_at(values, 0, s.gain, error, "gain");
        if (name == "diffuse") return number_at(values, 0, s.diffuse, error, "diffuse");
        if (name == "channellock_distance") return number_at(values, 0, s.channellock_distance, error, "channellock_distance");
        if (name == "divergence") return number_at(values, 0, s.divergence, error, "divergence");
        if (name == "divergence_range") return number_at(values, 0, s.divergence_range, error, "divergence_range");
        if (name == "ramp") {
            if (!number_at(values, 0, v, error, "ramp")) return false;
            s_.ramp_ms = v < 0.0 ? -1.0 : v;    // negative: back to the default ramp, like the renderers
            return true;
        }
        if (name == "cartesian" || name == "channellock" || name == "screenref") {
            if (!number_at(values, 0, v, error, name.c_str())) return false;
            const bool flag = v != 0.0;
            if (name == "cartesian") s.cartesian = flag;
            else if (name == "channellock") s.channellock = flag;
            else s.screenref = flag;
            return true;
        }
        if (name == "screenedgelock_h" || name == "screenedgelock_v") {
            if (values.empty() || !atom_is_symbol(values[0])) {
                error = name + " needs a symbol";
                return false;
            }
            const std::string edge = values[0];
            if (name == "screenedgelock_h") {
                if (edge != "none" && edge != "left" && edge != "right") { error = "screenedgelock_h must be none, left or right"; return false; }
                s.screenedgelock_h = edge;
            }
            else {
                if (edge != "none" && edge != "top" && edge != "bottom") { error = "screenedgelock_v must be none, top or bottom"; return false; }
                s.screenedgelock_v = edge;
            }
            return true;
        }
        if (name == "position" || name == "list") {
            if (values.size() < 2) {
                error = "position needs at least 2 numbers: azimuth elevation [distance] or x y z";
                return false;
            }
            double a = 0.0, b = 0.0, c = s.cartesian ? s.z : s.distance;
            if (!number_at(values, 0, a, error, "position") || !number_at(values, 1, b, error, "position")) return false;
            if (values.size() > 2 && !number_at(values, 2, c, error, "position")) return false;
            if (s.cartesian) { s.x = a; s.y = b; s.z = c; }
            else { s.azimuth = a; s.elevation = b; s.distance = c; }
            return true;
        }
        if (name == "zone") {
            error = "zone exclusion cannot be written (not supported by libadm); ignored";
            return false;
        }
        error = "unknown parameter: " + name;
        return false;
    }

    std::vector<slot> m_slots;
    bool m_capturing{ false };
    double m_last{ 0.0 };
    double m_default_ramp_ms{ 10.0 };
};

} // namespace earmax::admio
