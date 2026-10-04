/// @file
/// @brief   The metadata side of playing an ADM file, shared by ear.adm and
///          mc.ear.play~: holding the loaded file and its rendering items,
///          reporting them, sending the static bed and scene metadata to the
///          renderers, and emitting the Objects blocks active at a time with
///          the EAR's interpolation rules (see ear_max_adm.h for the rules).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "ear_max_adm.h"

#include "c74_min.h"

#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace earmax::admio {

using namespace c74::min;

/// A file name as the patcher gives it (absolute, Max-style
/// "Macintosh HD:/..." or, for reading, a name on Max's search path) as
/// a native absolute path; the name itself when nothing resolves it.
inline std::string resolve_path(const std::string& name, bool for_reading)
{
    if (for_reading) {
        try {
            path p(name);
            if (p) {
                const std::string full = p;
                if (!full.empty()) {
                    return full;
                }
            }
        }
        catch (...) {
            // not on the search path: try it as an absolute path below
        }
    }
    char native[c74::max::MAX_PATH_CHARS] = { 0 };
    if (c74::max::path_nameconform(name.c_str(), native, c74::max::PATH_STYLE_NATIVE, c74::max::PATH_TYPE_ABSOLUTE) == 0
        && native[0] != 0) {
        return native;
    }
    return name;
}

/// Where the player sends its messages: one function per renderer outlet
/// (mc.ear.objects~, mc.ear.direct~, mc.ear.hoa~) and the info outlet.
struct message_sinks {
    std::function<void(const atoms&)> objects;
    std::function<void(const atoms&)> direct;
    std::function<void(const atoms&)> hoa;
    std::function<void(const atoms&)> info;
};

/// The rendering items of a loaded file and the state of their emission.
/// Times are seconds from the start of the file.
class item_player {
public:
    bool loaded() const
    {
        return m_loaded;
    }

    const loaded_file& file() const
    {
        return m_file;
    }

    const file_info& info() const
    {
        return m_file.info;
    }

    const selection& items() const
    {
        return m_items;
    }

    int programme() const
    {
        return m_programme;
    }

    double position() const
    {
        return m_position;
    }

    void set_position(double seconds)
    {
        m_position = std::max(0.0, seconds);
    }

    /// Load a file's ADM and select its items (throws on error: the
    /// previous file is then unloaded). The position returns to 0.
    void load(const std::string& path)
    {
        const int programme = m_programme;
        unload();
        try {
            m_file = load_file(path);
            select(programme);
        }
        catch (...) {
            unload();    // nothing of the new file stays behind
            m_programme = programme;
            throw;
        }
    }

    /// Select the audioProgramme to render (0-based) and resolve the items
    /// again (throws on error).
    void select(int programme_index)
    {
        m_programme = std::max(0, programme_index);
        if (!m_file.document) {
            return;    // no file yet: the programme is remembered for the next load
        }
        m_loaded = false;
        m_items = select_items(m_file, m_programme);
        m_emitted.assign(m_items.objects.size(), k_never);
        m_emitted_direct.assign(m_items.direct.size(), k_never);
        m_loaded = true;
    }

    void unload()
    {
        m_loaded = false;
        m_file = loaded_file();
        m_items = selection();
        m_emitted.clear();
        m_emitted_direct.clear();
        m_position = 0.0;
    }

    /// Report the file, its programmes, rendering items and warnings on
    /// the info sink (track numbers are 1-based, as mc.ear.select~ takes them).
    void report(const message_sinks& out) const
    {
        if (!m_loaded) {
            out.info(atoms{ symbol("file"), symbol("none") });
            return;
        }
        const auto& info = m_file.info;
        out.info(atoms{ symbol("file"), symbol(info.path), static_cast<int>(info.samplerate), static_cast<int>(info.channels),
                        static_cast<double>(info.frames) });
        out.info(atoms{ symbol("programmes"), static_cast<int>(m_items.programmes.size()) });
        for (size_t i = 0; i < m_items.programmes.size(); ++i) {
            out.info(atoms{ symbol("programme"), static_cast<int>(i + 1), symbol(m_items.programmes[i]),
                            static_cast<int>(i) == m_items.programme ? 1 : 0 });
        }
        out.info(atoms{ symbol("objects"), static_cast<int>(m_items.objects.size()) });
        for (size_t i = 0; i < m_items.objects.size(); ++i) {
            const auto& item = m_items.objects[i];
            out.info(atoms{ symbol("object"), static_cast<int>(i + 1), item.track + 1, symbol(item.name), static_cast<int>(item.blocks.size()) });
        }
        out.info(atoms{ symbol("direct"), static_cast<int>(m_items.direct.size()) });
        for (size_t i = 0; i < m_items.direct.size(); ++i) {
            const auto& item = m_items.direct[i];
            atoms a{ symbol("directspeakers"), static_cast<int>(i + 1), item.track + 1, symbol(item.name) };
            for (const auto& label : item.labels) {
                a.push_back(symbol(label));
            }
            out.info(a);
        }
        out.info(atoms{ symbol("hoa"), static_cast<int>(m_items.hoa.size()) });
        for (size_t i = 0; i < m_items.hoa.size(); ++i) {
            const auto& item = m_items.hoa[i];
            atoms a{ symbol("scene"), static_cast<int>(i + 1), symbol(item.name), item.order, symbol(item.normalization) };
            for (const int track : item.tracks) {
                a.push_back(track + 1);
            }
            out.info(a);
        }
        for (const auto& warning : m_items.warnings) {
            out.info(atoms{ symbol("warning"), symbol(warning) });
        }
        if (m_items.hoa.size() > 1) {
            out.info(atoms{ symbol("warning"), symbol("the file has " + std::to_string(m_items.hoa.size())
                                                     + " HOA scenes; only the first is sent to the hoa outlet") });
        }
    }

    /// The static metadata of beds and scenes for mc.ear.direct~ and
    /// mc.ear.hoa~, preceded by the 1-based file track lists for
    /// mc.ear.select~ when `with_tracks` is set (an object that plays the
    /// audio itself routes the tracks and has no use for them).
    void send_static(const message_sinks& out, bool with_tracks) const
    {
        if (with_tracks) {
            atoms tracks{ symbol("tracks") };
            for (const auto& item : m_items.objects) {
                tracks.push_back(item.track + 1);
            }
            out.objects(tracks);

            tracks = atoms{ symbol("tracks") };
            for (const auto& item : m_items.direct) {
                tracks.push_back(item.track + 1);
            }
            out.direct(tracks);
        }
        for (size_t i = 0; i < m_items.direct.size(); ++i) {
            const auto& item = m_items.direct[i];
            const int n = static_cast<int>(i + 1);
            if (!item.timed()) {
                send_direct_block(n, item.blocks.front(), out);    // a timed bed's blocks come from emit()
            }
            out.direct(atoms{ symbol("setvalue"), n, symbol("lfe"), item.lfe ? 1 : 0 });
            out.direct(atoms{ symbol("setvalue"), n, symbol("packformat"), symbol(item.pack_id.empty() ? "none" : item.pack_id) });
        }

        if (!m_items.hoa.empty()) {
            const auto& item = m_items.hoa.front();
            out.hoa(atoms{ symbol("order"), item.order });
            out.hoa(atoms{ symbol("normalization"), symbol(item.normalization) });
            if (with_tracks) {
                atoms tracks{ symbol("tracks") };
                for (const int track : item.tracks) {
                    tracks.push_back(track + 1);
                }
                out.hoa(tracks);
            }
        }
        else if (with_tracks) {
            out.hoa(atoms{ symbol("tracks") });
        }
    }

    /// Forget what was emitted: the next emission sends every active block
    /// again (as a jump).
    void reset_emitted()
    {
        std::fill(m_emitted.begin(), m_emitted.end(), k_never);
        std::fill(m_emitted_direct.begin(), m_emitted_direct.end(), k_never);
    }

    /// Emit the blocks active at `time` for every Objects item whose block
    /// changed since the last emission; `jump` forces an immediate change.
    /// An object is silent outside its blocks (before the first, after one
    /// that ended with a gap before the next, and after the last one with a
    /// duration), as in the EAR: its gain is set to 0 once.
    void emit(double time, bool jump, const message_sinks& out)
    {
        for (size_t i = 0; i < m_items.objects.size(); ++i) {
            const auto& item = m_items.objects[i];
            const int current = block_at(item, time);
            const bool active = current >= 0 && time + 1e-6 < item.blocks[static_cast<size_t>(current)].end;
            if (!active) {
                if (m_emitted[i] != k_silent && m_emitted[i] != k_never) {
                    const int n = static_cast<int>(i + 1);
                    out.objects(atoms{ symbol("setvalue"), n, symbol("ramp"), 0.0 });
                    out.objects(atoms{ symbol("setvalue"), n, symbol("gain"), 0.0 });
                }
                m_emitted[i] = current < 0 ? k_never : k_silent;
                continue;
            }
            if (current == m_emitted[i]) {
                continue;
            }
            const auto& block = item.blocks[static_cast<size_t>(current)];
            const bool was_previous = m_emitted[i] == current - 1;
            const double ramp_seconds = (jump || !was_previous) ? 0.0 : block.interp;
            send_block(static_cast<int>(i + 1), block.state, ramp_seconds, out);
            m_emitted[i] = current;
        }
        // a timed bed: the block active now, when it changed (a bed keeps
        // its last block's parameters after it ends; mc.ear.direct~ ramps
        // the gains over its ramp attribute)
        for (size_t i = 0; i < m_items.direct.size(); ++i) {
            const auto& item = m_items.direct[i];
            if (!item.timed()) {
                continue;
            }
            const int current = direct_block_at(item, time);
            if (current < 0 || current == m_emitted_direct[i]) {
                continue;
            }
            send_direct_block(static_cast<int>(i + 1), item.blocks[static_cast<size_t>(current)], out);
            m_emitted_direct[i] = current;
        }
    }

    /// The next block start or end after `time` over all Objects items, or
    /// the end of the file when nothing is left; +inf when past the end.
    double next_boundary(double time) const
    {
        double next = std::numeric_limits<double>::infinity();
        for (const auto& item : m_items.objects) {
            for (const auto& block : item.blocks) {
                if (block.start > time + 1e-6 && block.start < next) {
                    next = block.start;
                }
                if (block.end > time + 1e-6 && block.end < next) {
                    next = block.end;
                }
            }
        }
        for (const auto& item : m_items.direct) {
            if (!item.timed()) {
                continue;
            }
            for (const auto& block : item.blocks) {
                if (block.start > time + 1e-6 && block.start < next) {
                    next = block.start;
                }
            }
        }
        const double length = end_time();
        if (length > time + 1e-6 && length < next) {
            next = length;
        }
        return next;
    }

    /// The length of the audio in seconds.
    double audio_length() const
    {
        return m_file.info.samplerate ? static_cast<double>(m_file.info.frames) / m_file.info.samplerate : 0.0;
    }

    /// Where the transport ends: the end of the audio, or of the last block
    /// when the metadata outlasts the audio.
    double end_time() const
    {
        double end = audio_length();
        for (const auto& item : m_items.objects) {
            if (!item.blocks.empty() && std::isfinite(item.blocks.back().end)) {
                end = std::max(end, item.blocks.back().end);
            }
        }
        return end;
    }

    static bool same_time(double a, double b)
    {
        return std::abs(a - b) < 1e-6;
    }

private:
    static constexpr int k_never = -1;     ///< nothing emitted yet for the item
    static constexpr int k_silent = -2;    ///< the item was silenced after a block ended

    /// the labels, position and bounds of a DirectSpeakers block for
    /// mc.ear.direct~
    static void send_direct_block(int n, const direct_block& block, const message_sinks& out)
    {
        atoms labels{ symbol("setvalue"), n, symbol("speakerlabel") };
        for (const auto& label : block.labels) {
            labels.push_back(symbol(label));
        }
        out.direct(labels);
        if (block.has_position) {
            out.direct(atoms{ symbol("setvalue"), n, symbol("cartesian"), block.cartesian ? 1 : 0 });
            if (block.cartesian) {
                out.direct(atoms{ symbol("setvalue"), n, symbol("position"), block.x, block.y, block.z });
            }
            else {
                out.direct(atoms{ symbol("setvalue"), n, symbol("position"), block.azimuth, block.elevation, block.distance });
            }
        }
        atoms bounds{ symbol("setvalue"), n, symbol("bounds") };
        for (const double b : block.bounds) {
            bounds.push_back(b);
        }
        out.direct(bounds);
    }

    static void send_block(int n, const object_state& s, double ramp_seconds, const message_sinks& out)
    {
        const symbol setvalue("setvalue");
        out.objects(atoms{ setvalue, n, symbol("ramp"), ramp_seconds * 1000.0 });
        out.objects(atoms{ setvalue, n, symbol("cartesian"), s.cartesian ? 1 : 0 });
        if (s.cartesian) {
            out.objects(atoms{ setvalue, n, symbol("position"), s.x, s.y, s.z });
        }
        else {
            out.objects(atoms{ setvalue, n, symbol("position"), s.azimuth, s.elevation, s.distance });
        }
        out.objects(atoms{ setvalue, n, symbol("width"), s.width });
        out.objects(atoms{ setvalue, n, symbol("height"), s.height });
        out.objects(atoms{ setvalue, n, symbol("depth"), s.depth });
        out.objects(atoms{ setvalue, n, symbol("gain"), s.gain });
        out.objects(atoms{ setvalue, n, symbol("diffuse"), s.diffuse });
        out.objects(atoms{ setvalue, n, symbol("channellock"), s.channellock ? 1 : 0 });
        out.objects(atoms{ setvalue, n, symbol("channellock_distance"), s.channellock_distance });
        out.objects(atoms{ setvalue, n, symbol("divergence"), s.divergence });
        out.objects(atoms{ setvalue, n, symbol("divergence_range"), s.divergence_range });
        out.objects(atoms{ setvalue, n, symbol("screenref"), s.screenref ? 1 : 0 });
        out.objects(atoms{ setvalue, n, symbol("screenedgelock_h"), symbol(s.screenedgelock_h) });
        out.objects(atoms{ setvalue, n, symbol("screenedgelock_v"), symbol(s.screenedgelock_v) });
        // the block's zones replace the previous ones: clear, then add each
        out.objects(atoms{ setvalue, n, symbol("zone"), symbol("clear") });
        for (const exclusion_zone& z : s.zones) {
            atoms zone{ setvalue, n, symbol("zone"), symbol(z.cartesian ? "cartesian" : "polar") };
            for (const double b : z.bounds) {
                zone.push_back(b);
            }
            out.objects(zone);
        }
    }

    loaded_file m_file;
    selection m_items;
    bool m_loaded{ false };
    int m_programme{ 0 };
    std::vector<int> m_emitted;
    std::vector<int> m_emitted_direct;    ///< the block last emitted per DirectSpeakers item (timed beds)    ///< per Objects item: index of the last emitted block, k_never or k_silent
    double m_position{ 0.0 };      ///< transport position in seconds
};

} // namespace earmax::admio
