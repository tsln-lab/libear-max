/// @file
/// @brief   Capturing object metadata for writing, shared by ear.adm and
///          mc.ear.record~: a set of object slots that take the same messages
///          as mc.ear.objects~ (setvalue, applyvalues, parameters, lists) and
///          record every change as a timed block, with the ramp in force at
///          that moment as the block's interpolation. The caller supplies the
///          time of each change (ear.adm from Max's scheduler, mc.ear.record~
///          from the frames recorded so far), so the capture itself knows no
///          clock. A channel bed (bed_capture, the messages of mc.ear.direct~)
///          and an HOA scene (scene_capture, the messages of mc.ear.hoa~) are
///          static metadata, captured without a clock.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "ear_max_adm.h"
#include "ear_max_direct.h"

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
            return apply_zone(s, values, error);
        }
        error = "unknown parameter: " + name;
        return false;
    }

    /// 'zone clear', 'zone polar minAz maxAz minEl maxEl' or 'zone cartesian
    /// minX maxX minY maxY minZ maxZ', as the renderers take them; the bounds
    /// must be within the ranges an ADM file can hold.
    static bool apply_zone(object_state& s, const atoms& values, std::string& error)
    {
        if (values.empty() || !atom_is_symbol(values[0])) {
            error = "zone needs 'polar', 'cartesian' or 'clear' as first argument";
            return false;
        }
        const std::string kind = values[0];
        if (kind == "clear") {
            if (values.size() != 1) {
                error = "zone clear takes no bounds";
                return false;
            }
            s.zones.clear();
            return true;
        }
        exclusion_zone zone;
        zone.cartesian = kind == "cartesian";
        const size_t count = zone.cartesian ? 6 : 4;
        if ((kind != "polar" && kind != "cartesian") || values.size() != count + 1) {
            error = "zone: use 'zone polar minAz maxAz minEl maxEl', "
                    "'zone cartesian minX maxX minY maxY minZ maxZ' or 'zone clear'";
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            double v = 0.0;
            if (!number_at(values, i + 1, v, error, "zone")) return false;
            // libadm validates the ranges of BS.2076: azimuths within +-180,
            // elevations within +-90, Cartesian bounds within +-1
            const double limit = zone.cartesian ? 1.0 : (i < 2 ? 180.0 : 90.0);
            if (v < -limit || v > limit) {
                error = zone.cartesian ? "zone cartesian bounds must be within -1..1"
                                       : "zone polar azimuths must be within -180..180 and elevations within -90..90";
                return false;
            }
            zone.bounds.push_back(v);
        }
        // a lower bound above its upper bound is an empty box; a polar zone's
        // azimuths may wrap around the back (minAz 150 maxAz -150), so only
        // the elevations are checked there
        const bool inverted = zone.cartesian
            ? (zone.bounds[0] > zone.bounds[1] || zone.bounds[2] > zone.bounds[3] || zone.bounds[4] > zone.bounds[5])
            : zone.bounds[2] > zone.bounds[3];
        if (inverted) {
            error = zone.cartesian ? "zone cartesian: each min must not exceed its max"
                                   : "zone polar: minEl must not exceed maxEl";
            return false;
        }
        s.zones.push_back(std::move(zone));
        return true;
    }

    std::vector<slot> m_slots;
    bool m_capturing{ false };
    double m_last{ 0.0 };
    double m_default_ramp_ms{ 10.0 };
};

/// The DirectSpeakers channels written after the objects, taking the
/// messages of mc.ear.direct~ ('setvalue N speakerlabel ...', 'inputlayout
/// 0+5+0', 'applyvalues ...', or a parameter for all channels). Between
/// begin() and end(), a change of a channel's labels, position or bounds
/// is recorded as a timed block at the caller's time (lfe and packformat
/// are the channel's for the whole file).
class bed_capture {
public:
    size_t size() const
    {
        return m_channels.size();
    }

    void resize(size_t n)
    {
        const size_t old = m_channels.size();
        m_channels.resize(n);
        if (m_capturing || !m_blocks.empty()) {
            m_blocks.resize(n);
        }
        if (m_capturing) {
            // a channel added while capturing starts with its state as its
            // first block, like the others did at begin()
            for (size_t i = old; i < n; ++i) {
                m_blocks[i].assign(1, block_of(m_channels[i], 0.0));
            }
        }
    }

    const std::string& name() const
    {
        return m_name;
    }

    void set_name(const std::string& name)
    {
        m_name = name;
    }

    const direct_metadata& at(size_t i) const
    {
        return m_channels[i];
    }

    const std::vector<captured_direct_block>& blocks(size_t i) const
    {
        return m_blocks[i];
    }

    void clear()
    {
        m_capturing = false;
        for (auto& ch : m_channels) {
            ch = direct_metadata();
        }
        for (auto& b : m_blocks) {
            b.clear();
        }
    }

    /// Start capturing at time 0: every channel's current labels and
    /// position become its first block.
    void begin()
    {
        m_capturing = true;
        m_blocks.assign(m_channels.size(), {});
        for (size_t i = 0; i < m_channels.size(); ++i) {
            m_blocks[i].push_back(block_of(m_channels[i], 0.0));
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

    /// Apply an mc.ear.direct~ channel parameter to one channel (0-based)
    /// at `time` seconds; while capturing, a change of the labels, position
    /// or bounds becomes a block.
    bool apply_one(size_t i, const std::string& name, const atoms& values, double time, std::string& error)
    {
        if (i >= m_channels.size()) {
            error = "channel number out of range 1.." + std::to_string(m_channels.size());
            return false;
        }
        if (!m_channels[i].apply(name, values, [&](const std::string& m) { error = m; })) {
            return false;
        }
        if (name != "lfe" && name != "packformat") {
            record_change(i, time);
        }
        return true;
    }

    bool apply_all(const std::string& name, const atoms& values, double time, std::string& error)
    {
        for (size_t i = 0; i < m_channels.size(); ++i) {
            if (!apply_one(i, name, values, time, error)) {
                return false;
            }
        }
        return true;
    }

    /// Label the channels after a BS.2051 layout, as mc.ear.direct~'s
    /// 'inputlayout': labels, nominal positions and LFE; when the layout is
    /// a common definitions one its audioPackFormatID is set as well, so
    /// the bed is written as a reference to it. Channels beyond the layout
    /// are cleared.
    bool apply_layout(const std::string& layout_name, double time, std::string& error)
    {
        ear::Layout layout;
        try {
            layout = ear::getLayout(layout_name);
        }
        catch (const std::exception& e) {
            error = e.what();
            return false;
        }
        const auto& channels = layout.channels();
        if (channels.size() > m_channels.size()) {
            error = "inputlayout " + layout.name() + " has " + std::to_string(channels.size()) + " channels but only "
                    + std::to_string(m_channels.size()) + " are written (see the directchans attribute)";
            return false;
        }
        const auto& packs = adm::audioPackFormatLookupTable();
        const auto pack = packs.find(layout.name());
        for (size_t i = 0; i < m_channels.size(); ++i) {
            direct_metadata& meta = m_channels[i];
            meta = direct_metadata();
            if (i < channels.size()) {
                meta.dstm.speakerLabels = { channels[i].name() };
                meta.dstm.channelFrequency.lowPass = channels[i].isLfe() ? boost::optional<double>(120.0) : boost::none;
                const auto pos = channels[i].polarPositionNominal();
                meta.position = ear::PolarSpeakerPosition(pos.azimuth, pos.elevation, pos.distance);
                meta.rebuild_position();
                if (pack != packs.end()) {
                    meta.dstm.audioPackFormatID = adm::formatId(pack->second);
                }
            }
            record_change(i, time);
        }
        return true;
    }

    /// The bed as captured, for build_document. Every channel is written
    /// with its position (the default is 0 0 1, as in mc.ear.direct~): ADM
    /// asks for one, and libadm writes one in any case.
    captured_bed bed() const
    {
        captured_bed out;
        out.name = m_name;
        for (size_t i = 0; i < m_channels.size(); ++i) {
            const direct_metadata& meta = m_channels[i];
            captured_direct_channel ch;
            ch.labels = meta.dstm.speakerLabels;
            ch.has_position = true;
            ch.cartesian = meta.cartesian;
            ch.azimuth = meta.position.azimuth;
            ch.elevation = meta.position.elevation;
            ch.distance = meta.position.distance;
            ch.x = meta.cartesian_position.X;
            ch.y = meta.cartesian_position.Y;
            ch.z = meta.cartesian_position.Z;
            ch.bounds = meta.bounds;
            ch.lfe = static_cast<bool>(meta.dstm.channelFrequency.lowPass);
            ch.pack_id = meta.dstm.audioPackFormatID ? *meta.dstm.audioPackFormatID : std::string();
            if (i < m_blocks.size()) {
                ch.blocks = m_blocks[i];
            }
            out.channels.push_back(std::move(ch));
        }
        return out;
    }

private:
    static captured_direct_block block_of(const direct_metadata& meta, double time)
    {
        captured_direct_block b;
        b.time = time;
        b.labels = meta.dstm.speakerLabels;
        b.has_position = true;
        b.cartesian = meta.cartesian;
        b.azimuth = meta.position.azimuth;
        b.elevation = meta.position.elevation;
        b.distance = meta.position.distance;
        b.x = meta.cartesian_position.X;
        b.y = meta.cartesian_position.Y;
        b.z = meta.cartesian_position.Z;
        b.bounds = meta.bounds;
        return b;
    }

    static bool same_values(const captured_direct_block& a, const captured_direct_block& b)
    {
        return a.labels == b.labels && a.has_position == b.has_position && a.cartesian == b.cartesian && a.azimuth == b.azimuth
               && a.elevation == b.elevation && a.distance == b.distance && a.x == b.x && a.y == b.y && a.z == b.z
               && a.bounds == b.bounds;
    }

    void record_change(size_t i, double time)
    {
        if (!m_capturing || i >= m_blocks.size()) {
            return;
        }
        auto& blocks = m_blocks[i];
        const double t = std::max(0.0, time);
        captured_direct_block b = block_of(m_channels[i], t);
        if (!blocks.empty() && std::abs(blocks.back().time - t) < 1e-6) {
            blocks.back() = b;    // several parameters at the same time: one block
            return;
        }
        if (!blocks.empty() && same_values(blocks.back(), b)) {
            return;    // the same metadata sent again (patches repeat it): not a change
        }
        blocks.push_back(std::move(b));
    }

    std::string m_name{ "bed" };
    std::vector<direct_metadata> m_channels;
    std::vector<std::vector<captured_direct_block>> m_blocks;    ///< per channel, while and after capturing
    bool m_capturing{ false };
};

/// The HOA scene written after the bed: the order and normalization of
/// mc.ear.hoa~ ('order N', 'normalization SN3D'), and a name.
class scene_capture {
public:
    const captured_scene& scene() const
    {
        return m_scene;
    }

    bool set_order(int order, std::string& error)
    {
        if (order > hoa::k_max_order) {
            error = "order must be between 0 and " + std::to_string(hoa::k_max_order) + " (negative: no scene)";
            return false;
        }
        m_scene.order = std::max(-1, order);
        return true;
    }

    bool set_normalization(const std::string& name, std::string& error)
    {
        if (name != "SN3D" && name != "N3D" && name != "FuMa") {
            error = "normalization must be SN3D, N3D or FuMa";
            return false;
        }
        m_scene.normalization = name;
        return true;
    }

    void set_name(const std::string& name)
    {
        m_scene.name = name;
    }

private:
    captured_scene m_scene;
};

/// Apply a message sent to the 'direct' inlet (or with 'direct' prepended)
/// of a capturing object at `time` seconds: the mc.ear.direct~ messages
/// 'setvalue N parameter values...', 'applyvalues parameter v1 v2 ...',
/// 'inputlayout name', plus 'name symbol' for the bed, and any channel
/// parameter for all channels. The 'tracks' message of ear.adm and
/// mc.ear.play~ is ignored, so their direct outlet can be fed straight
/// back. Returns false with `error` set.
inline bool apply_direct_message(bed_capture& bed, const atoms& args, double time, std::string& error)
{
    if (args.empty() || !atom_is_symbol(args[0])) {
        error = "direct needs a message: setvalue, applyvalues, inputlayout, name or a channel parameter";
        return false;
    }
    const std::string selector = args[0];
    const atoms rest(args.begin() + 1, args.end());
    if (selector == "tracks") {
        return true;
    }
    if (selector == "name") {
        if (rest.empty() || !atom_is_symbol(rest[0])) {
            error = "direct name needs a symbol";
            return false;
        }
        bed.set_name(std::string(rest[0]));
        return true;
    }
    if (selector == "inputlayout") {
        if (rest.empty() || !atom_is_symbol(rest[0])) {
            error = "direct inputlayout needs a layout name";
            return false;
        }
        return bed.apply_layout(std::string(rest[0]), time, error);
    }
    if (selector == "setvalue") {
        if (rest.size() < 2 || !atom_is_numeric(rest[0]) || !atom_is_symbol(rest[1])) {
            error = "direct setvalue needs a channel number (1-based, 0 for all) and a parameter name";
            return false;
        }
        const long index = static_cast<long>(static_cast<double>(rest[0]));
        if (index < 0 || static_cast<size_t>(index) > bed.size()) {
            error = "direct setvalue: channel number out of range 0.." + std::to_string(bed.size());
            return false;
        }
        const std::string parameter = rest[1];
        const atoms values(rest.begin() + 2, rest.end());
        return index == 0 ? bed.apply_all(parameter, values, time, error)
                          : bed.apply_one(static_cast<size_t>(index - 1), parameter, values, time, error);
    }
    if (selector == "applyvalues") {
        if (rest.empty() || !atom_is_symbol(rest[0])) {
            error = "direct applyvalues needs a parameter name followed by one value per channel";
            return false;
        }
        const std::string parameter = rest[0];
        const size_t n = std::min(rest.size() - 1, bed.size());
        for (size_t i = 0; i < n; ++i) {
            if (!bed.apply_one(i, parameter, atoms{ rest[i + 1] }, time, error)) {
                return false;
            }
        }
        return true;
    }
    return bed.apply_all(selector, rest, time, error);
}

/// Apply a message sent to the 'hoa' inlet (or with 'hoa' prepended): the
/// mc.ear.hoa~ settings 'order N' and 'normalization name', plus 'name
/// symbol' for the scene; 'tracks' is ignored as above.
inline bool apply_hoa_message(scene_capture& scene, const atoms& args, std::string& error)
{
    if (args.empty() || !atom_is_symbol(args[0])) {
        error = "hoa needs a message: order, normalization or name";
        return false;
    }
    const std::string selector = args[0];
    if (selector == "tracks") {
        return true;
    }
    if (selector == "order") {
        if (args.size() < 2 || !atom_is_numeric(args[1])) {
            error = "hoa order needs a number";
            return false;
        }
        return scene.set_order(static_cast<int>(static_cast<double>(args[1])), error);
    }
    if (selector == "normalization" || selector == "name") {
        if (args.size() < 2 || !atom_is_symbol(args[1])) {
            error = "hoa " + selector + " needs a symbol";
            return false;
        }
        if (selector == "name") {
            scene.set_name(std::string(args[1]));
            return true;
        }
        return scene.set_normalization(std::string(args[1]), error);
    }
    error = "unknown hoa message: " + selector + " (order, normalization or name)";
    return false;
}

} // namespace earmax::admio
