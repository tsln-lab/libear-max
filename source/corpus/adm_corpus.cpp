/// @file
/// @brief   The ADM corpus checker: reads every BW64 file of a corpus (the
///          EBU's ADM test materials), selects every programme as ear.adm
///          does, feeds the metadata to libear's gain calculators as the
///          renderers do, writes the selection back with the document
///          builder and reads it again, and records or checks a manifest
///          of what it found. See README "Testing against the EBU's ADM
///          test files".
/// @license Use of this source code is governed by the MIT License found in the License.md file.
///
/// Usage:
///   adm_corpus [--record|--check manifest] [--layout name] [files or directory]
///
/// The corpus is the files given, or every .wav of the directory given, or
/// of the directory named by EARMAX_ADM_CORPUS. Without one the checker
/// exits with 77, which ctest reports as skipped. `--record` writes the
/// manifest from what it finds; `--check` (the default with a manifest
/// path) compares and exits with 1 on any difference. Without a manifest
/// the findings are printed.

#include "ear_max_adm.h"

#include "ear/ear.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

using namespace earmax;

namespace {

constexpr int k_skipped = 77;

// ---- files (std::filesystem needs macOS 10.15, the externals target 10.11) --

bool is_directory(const std::string& path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool is_file(const std::string& path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
#endif
}

bool ends_with(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string basename_of(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

/// the .wav files of a directory, as paths
std::vector<std::string> wav_files(const std::string& dir)
{
    std::vector<std::string> out;
#ifdef _WIN32
    WIN32_FIND_DATAA entry;
    HANDLE handle = FindFirstFileA((dir + "\\*.wav").c_str(), &entry);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                out.push_back(dir + "\\" + entry.cFileName);
            }
        } while (FindNextFileA(handle, &entry));
        FindClose(handle);
    }
#else
    if (DIR* d = opendir(dir.c_str())) {
        while (const dirent* entry = readdir(d)) {
            const std::string name = entry->d_name;
            const std::string path = dir + "/" + name;
            if (ends_with(name, ".wav") && is_file(path)) {
                out.push_back(path);
            }
        }
        closedir(d);
    }
#endif
    return out;
}

std::string num(double v)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.6g", v);
    return buffer;
}

bool close(double a, double b, double tolerance = 1e-4)
{
    return std::abs(a - b) <= tolerance * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}

// ---- the metadata as the renderers take it ---------------------------

ear::ObjectsTypeMetadata to_otm(const admio::object_state& s)
{
    ear::ObjectsTypeMetadata otm;
    otm.cartesian = s.cartesian;
    if (s.cartesian) {
        otm.position = ear::CartesianPosition(s.x, s.y, s.z);
        otm.objectDivergence = ear::CartesianObjectDivergence(s.divergence, s.divergence_range);
    }
    else {
        otm.position = ear::PolarPosition(s.azimuth, s.elevation, s.distance);
        otm.objectDivergence = ear::PolarObjectDivergence(s.divergence, s.divergence_range);
    }
    otm.width = s.width;
    otm.height = s.height;
    otm.depth = s.depth;
    otm.gain = s.gain;
    otm.diffuse = s.diffuse;
    otm.channelLock = ear::ChannelLock(s.channellock, s.channellock_distance > 0.0 ? boost::optional<double>(s.channellock_distance)
                                                                                     : boost::none);
    otm.screenRef = s.screenref;
    if (s.screenedgelock_h != "none") otm.screenEdgeLock.horizontal = s.screenedgelock_h;
    if (s.screenedgelock_v != "none") otm.screenEdgeLock.vertical = s.screenedgelock_v;
    for (const auto& z : s.zones) {
        const auto f = [&z](size_t i) { return static_cast<float>(z.bounds[i]); };
        if (z.cartesian) {
            otm.zoneExclusion.zones.push_back(ear::CartesianExclusionZone{ f(0), f(1), f(2), f(3), f(4), f(5), z.label });
        }
        else {
            otm.zoneExclusion.zones.push_back(ear::PolarExclusionZone{ f(0), f(1), f(2), f(3), 0.0f, 1.0f, z.label });
        }
    }
    return otm;
}

ear::DirectSpeakersTypeMetadata to_dstm(const admio::direct_item& d)
{
    ear::DirectSpeakersTypeMetadata dstm;
    dstm.speakerLabels = d.labels;
    if (d.cartesian) {
        ear::CartesianSpeakerPosition p(d.x, d.y, d.z);
        if (d.bounds.size() >= 6) {
            p.XMin = d.bounds[0];
            p.XMax = d.bounds[1];
            p.YMin = d.bounds[2];
            p.YMax = d.bounds[3];
            p.ZMin = d.bounds[4];
            p.ZMax = d.bounds[5];
        }
        dstm.position = p;
    }
    else {
        ear::PolarSpeakerPosition p(d.azimuth, d.elevation, d.distance);
        if (d.bounds.size() >= 4) {
            p.azimuthMin = d.bounds[0];
            p.azimuthMax = d.bounds[1];
            p.elevationMin = d.bounds[2];
            p.elevationMax = d.bounds[3];
        }
        if (d.bounds.size() >= 6) {
            p.distanceMin = d.bounds[4];
            p.distanceMax = d.bounds[5];
        }
        dstm.position = p;
    }
    if (d.lfe) {
        dstm.channelFrequency.lowPass = 120.0;
    }
    if (!d.pack_id.empty()) {
        dstm.audioPackFormatID = d.pack_id;
    }
    return dstm;
}

// ---- the selection as the capture would hold it -----------------------

admio::captured_programme to_captured(const admio::selection& sel, const std::string& name)
{
    admio::captured_programme p;
    p.name = name;
    for (const auto& o : sel.objects) {
        admio::captured_object object;
        object.name = o.name;
        for (const auto& b : o.blocks) {
            object.blocks.push_back({ b.start, b.interp, b.state });
        }
        p.objects.push_back(object);
    }
    if (!sel.direct.empty()) {
        p.bed.name = sel.direct.front().name;
        for (const auto& d : sel.direct) {
            admio::captured_direct_channel ch;
            ch.labels = d.labels;
            ch.has_position = d.has_position;
            ch.cartesian = d.cartesian;
            ch.azimuth = d.azimuth;
            ch.elevation = d.elevation;
            ch.distance = d.distance;
            ch.x = d.x;
            ch.y = d.y;
            ch.z = d.z;
            ch.bounds = d.bounds;
            ch.lfe = d.lfe;
            ch.pack_id = d.pack_id;
            if (d.timed()) {
                for (const auto& b : d.blocks) {
                    admio::captured_direct_block cb;
                    cb.time = b.start;
                    cb.labels = b.labels;
                    cb.has_position = b.has_position;
                    cb.cartesian = b.cartesian;
                    cb.azimuth = b.azimuth;
                    cb.elevation = b.elevation;
                    cb.distance = b.distance;
                    cb.x = b.x;
                    cb.y = b.y;
                    cb.z = b.z;
                    cb.bounds = b.bounds;
                    ch.blocks.push_back(cb);
                }
            }
            p.bed.channels.push_back(ch);
        }
    }
    if (sel.hoa.size() == 1 && sel.hoa.front().complete) {
        p.scene.name = sel.hoa.front().name;
        p.scene.order = sel.hoa.front().order;
        p.scene.normalization = sel.hoa.front().normalization;
    }
    return p;
}

// ---- comparing a selection with its round trip ------------------------

void compare_state(const std::string& where, const admio::object_state& a, const admio::object_state& b, std::vector<std::string>& out)
{
    const auto differ = [&](const char* what, double x, double y) {
        if (!close(x, y)) out.push_back(where + " " + what + " " + num(x) + " -> " + num(y));
    };
    if (a.cartesian != b.cartesian) {
        out.push_back(where + " cartesian " + std::to_string(a.cartesian) + " -> " + std::to_string(b.cartesian));
        return;
    }
    if (a.cartesian) {
        differ("x", a.x, b.x);
        differ("y", a.y, b.y);
        differ("z", a.z, b.z);
    }
    else {
        differ("azimuth", a.azimuth, b.azimuth);
        differ("elevation", a.elevation, b.elevation);
        differ("distance", a.distance, b.distance);
    }
    differ("width", a.width, b.width);
    differ("height", a.height, b.height);
    differ("depth", a.depth, b.depth);
    differ("gain", a.gain, b.gain);
    differ("diffuse", a.diffuse, b.diffuse);
    differ("divergence", a.divergence, b.divergence);
    if (a.divergence > 0.0) differ("divergence_range", a.divergence_range, b.divergence_range);
    if (a.channellock != b.channellock) out.push_back(where + " channellock differs");
    if (a.channellock) differ("channellock_distance", a.channellock_distance, b.channellock_distance);
    if (a.screenref != b.screenref) out.push_back(where + " screenref differs");
    if (a.screenedgelock_h != b.screenedgelock_h || a.screenedgelock_v != b.screenedgelock_v) out.push_back(where + " screenedgelock differs");
    if (a.zones.size() != b.zones.size()) {
        out.push_back(where + " zones " + std::to_string(a.zones.size()) + " -> " + std::to_string(b.zones.size()));
    }
    else {
        for (size_t i = 0; i < a.zones.size(); ++i) {
            if (a.zones[i].cartesian != b.zones[i].cartesian || a.zones[i].bounds.size() != b.zones[i].bounds.size()
                || a.zones[i].label != b.zones[i].label) {
                out.push_back(where + " zone " + std::to_string(i + 1) + " differs");
                continue;
            }
            for (size_t k = 0; k < a.zones[i].bounds.size(); ++k) {
                if (!close(a.zones[i].bounds[k], b.zones[i].bounds[k])) {
                    out.push_back(where + " zone " + std::to_string(i + 1) + " bound " + std::to_string(k + 1) + " differs");
                    break;
                }
            }
        }
    }
}

std::vector<std::string> compare(const admio::selection& a, const admio::selection& b, double length)
{
    std::vector<std::string> out;
    size_t renumbered = 0;    // items whose track moved: the writer lays the programme's tracks out afresh
    // a block that lasts to the end of its object (end = infinity when the
    // object has no duration) comes back lasting to the end of the file:
    // the same span of audio
    const auto same_end = [length](double x, double y) {
        const double ex = std::isfinite(x) ? x : length;
        const double ey = std::isfinite(y) ? y : length;
        return close(ex, ey, 1e-4);    // a sample or so
    };
    if (a.objects.size() != b.objects.size()) {
        out.push_back("objects " + std::to_string(a.objects.size()) + " -> " + std::to_string(b.objects.size()));
    }
    for (size_t i = 0; i < std::min(a.objects.size(), b.objects.size()); ++i) {
        const std::string where = "object " + std::to_string(i + 1);
        const auto& x = a.objects[i];
        const auto& y = b.objects[i];
        if (x.track != y.track) ++renumbered;
        if (x.blocks.size() != y.blocks.size()) {
            out.push_back(where + " blocks " + std::to_string(x.blocks.size()) + " -> " + std::to_string(y.blocks.size()));
            continue;
        }
        for (size_t k = 0; k < x.blocks.size(); ++k) {
            const std::string block = where + " block " + std::to_string(k + 1);
            if (!close(x.blocks[k].start, y.blocks[k].start, 1e-4)) {
                out.push_back(block + " start " + num(x.blocks[k].start) + " -> " + num(y.blocks[k].start));
            }
            if (!same_end(x.blocks[k].end, y.blocks[k].end)) {
                out.push_back(block + " end " + num(x.blocks[k].end) + " -> " + num(y.blocks[k].end));
            }
            if (!close(x.blocks[k].interp, y.blocks[k].interp, 1e-4)) {
                out.push_back(block + " interp " + num(x.blocks[k].interp) + " -> " + num(y.blocks[k].interp));
            }
            compare_state(block, x.blocks[k].state, y.blocks[k].state, out);
        }
    }
    if (a.direct.size() != b.direct.size()) {
        out.push_back("direct " + std::to_string(a.direct.size()) + " -> " + std::to_string(b.direct.size()));
    }
    for (size_t i = 0; i < std::min(a.direct.size(), b.direct.size()); ++i) {
        const std::string where = "direct " + std::to_string(i + 1);
        const auto& x = a.direct[i];
        const auto& y = b.direct[i];
        if (x.track != y.track) ++renumbered;
        std::vector<std::string> lx, ly;
        for (const auto& l : x.labels) lx.push_back(admio::nominal_label(l));
        for (const auto& l : y.labels) ly.push_back(admio::nominal_label(l));
        if (lx != ly) out.push_back(where + " labels differ");
        if (x.has_position != y.has_position || x.cartesian != y.cartesian) {
            out.push_back(where + " position kind differs");
        }
        else if (x.has_position) {
            if (x.cartesian) {
                if (!close(x.x, y.x) || !close(x.y, y.y) || !close(x.z, y.z)) out.push_back(where + " position differs");
            }
            else if (!close(x.azimuth, y.azimuth) || !close(x.elevation, y.elevation) || !close(x.distance, y.distance)) {
                out.push_back(where + " position " + num(x.azimuth) + " " + num(x.elevation) + " " + num(x.distance) + " -> " + num(y.azimuth)
                              + " " + num(y.elevation) + " " + num(y.distance));
            }
        }
        if (x.bounds.size() != y.bounds.size()) out.push_back(where + " bounds differ");
        if (x.lfe != y.lfe) out.push_back(where + " lfe differs");
        if (x.blocks.size() != y.blocks.size()) {
            out.push_back(where + " blocks " + std::to_string(x.blocks.size()) + " -> " + std::to_string(y.blocks.size()));
        }
    }
    if (a.hoa.size() != b.hoa.size()) {
        out.push_back("hoa " + std::to_string(a.hoa.size()) + " -> " + std::to_string(b.hoa.size()));
    }
    for (size_t i = 0; i < std::min(a.hoa.size(), b.hoa.size()); ++i) {
        const std::string where = "hoa " + std::to_string(i + 1);
        if (a.hoa[i].order != b.hoa[i].order) out.push_back(where + " order differs");
        if (a.hoa[i].normalization != b.hoa[i].normalization) out.push_back(where + " normalization differs");
        if (a.hoa[i].tracks != b.hoa[i].tracks) out.push_back(where + " tracks differ");
    }
    if (renumbered) {
        out.push_back(std::to_string(renumbered)
                      + " items on other tracks (the writer lays the programme's tracks out afresh: objects first, then the bed)");
    }
    return out;
}

// ---- one file -----------------------------------------------------------

/// What the checker found in one file: the manifest's lines for it.
std::vector<std::string> examine(const std::string& path, const std::string& layout_name)
{
    std::vector<std::string> lines;
    admio::loaded_file file;
    try {
        file = admio::load_file(path);
    }
    catch (const std::exception& e) {
        lines.push_back("load-error: " + std::string(e.what()));
        return lines;
    }
    lines.push_back("audio: " + std::to_string(file.info.channels) + " tracks " + std::to_string(file.info.samplerate) + " Hz "
                    + std::to_string(file.info.bitdepth) + " bit " + std::to_string(file.info.frames) + " frames");
    // the file's chunks (whether a Dolby master carries dbmd, for one)
    try {
        auto reader = bw64::readFile(path);
        std::string chunks;
        for (const auto& header : reader->chunks()) {
            std::string id;
            for (int shift = 0; shift < 32; shift += 8) {
                id += static_cast<char>((header.id >> shift) & 0xff);
            }
            while (!id.empty() && id.back() == ' ') id.pop_back();
            chunks += (chunks.empty() ? "" : " ") + id + (id == "data" ? "" : " (" + std::to_string(header.size) + ")");
        }
        lines.push_back("chunks: " + chunks);
    }
    catch (const std::exception&) {
    }
    const double length = file.info.samplerate ? static_cast<double>(file.info.frames) / file.info.samplerate : 0.0;

    admio::selection first;
    try {
        first = admio::select_items(file);
    }
    catch (const std::exception& e) {
        lines.push_back("select-error: " + std::string(e.what()));
        return lines;
    }
    lines.push_back("programmes: " + std::to_string(first.programmes.size()));

    const ear::Layout layout = ear::getLayout(layout_name);
    ear::GainCalculatorObjects objects_calc(layout);
    ear::GainCalculatorDirectSpeakers direct_calc(layout);
    ear::GainCalculatorHOA hoa_calc(layout);

    const size_t count = std::max<size_t>(1, first.programmes.size());
    for (size_t p = 0; p < count; ++p) {
        const std::string tag = "programme " + std::to_string(p + 1) + ": ";
        admio::selection sel;
        try {
            sel = admio::select_items(file, static_cast<int>(p));
        }
        catch (const std::exception& e) {
            lines.push_back(tag + "select-error: " + e.what());
            continue;
        }
        const std::string name = p < first.programmes.size() ? first.programmes[p] : std::string("(none)");
        size_t blocks = 0;
        for (const auto& o : sel.objects) blocks += o.blocks.size();
        size_t timed = 0;
        for (const auto& d : sel.direct) timed += d.timed() ? 1 : 0;
        std::ostringstream items;
        items << tag << "'" << name << "' objects " << sel.objects.size() << " (" << blocks << " blocks) direct " << sel.direct.size();
        if (timed) items << " (" << timed << " timed)";
        items << " hoa " << sel.hoa.size();
        for (const auto& h : sel.hoa) {
            items << " [order " << h.order << " " << h.normalization << (h.complete ? "" : " incomplete") << "]";
        }
        lines.push_back(items.str());
        for (const auto& w : sel.warnings) {
            lines.push_back(tag + "warning: " + w);
        }

        // the renderers' calculators over every block
        std::map<std::string, size_t> notes;
        size_t errors = 0;
        const auto report = [&](const std::string& what) { ++notes[what]; };
        std::vector<float> direct_gains(layout.channels().size()), diffuse_gains(layout.channels().size());
        for (const auto& o : sel.objects) {
            for (const auto& b : o.blocks) {
                try {
                    objects_calc.calculate(to_otm(b.state), direct_gains, diffuse_gains,
                                           [&](const ear::Warning& w) { report("warning: " + w.message); });
                }
                catch (const std::exception& e) {
                    ++errors;
                    report(std::string("error: ") + e.what());
                }
            }
        }
        std::vector<float> gains(layout.channels().size());
        for (const auto& d : sel.direct) {
            try {
                direct_calc.calculate(to_dstm(d), gains, [&](const ear::Warning& w) { report("warning: " + w.message); });
            }
            catch (const std::exception& e) {
                ++errors;
                report(std::string("error: ") + e.what());
            }
        }
        for (const auto& h : sel.hoa) {
            ear::HOATypeMetadata hoa;
            for (size_t i = 0; i < h.tracks.size(); ++i) {
                int n = 0, m = 0;
                hoa::from_acn(static_cast<int>(i), n, m);
                hoa.orders.push_back(n);
                hoa.degrees.push_back(m);
            }
            hoa.normalization = h.normalization;
            std::vector<std::vector<float>> matrix(h.tracks.size(), std::vector<float>(layout.channels().size()));
            try {
                hoa_calc.calculate(hoa, matrix, [&](const ear::Warning& w) { report("warning: " + w.message); });
            }
            catch (const std::exception& e) {
                ++errors;
                report(std::string("error: ") + e.what());
            }
        }
        lines.push_back(tag + "render " + layout_name + ": " + std::to_string(errors) + " errors");
        for (const auto& n : notes) {
            lines.push_back(tag + "render " + n.first + " (" + std::to_string(n.second) + ")");
        }

        // the round trip through the document builder and the reader
        try {
            admio::captured_programme captured = to_captured(sel, name);
            std::vector<bw64::AudioId> chna_ids;
            std::vector<std::string> warnings;
            auto doc = admio::build_document(captured, length, chna_ids, warnings,
                                             admio::audio_format{ file.info.samplerate, file.info.bitdepth });
            for (const auto& w : warnings) {
                lines.push_back(tag + "roundtrip warning: " + w);
            }
            admio::loaded_file again;
            again.info = file.info;
            std::stringstream xml(admio::to_xml(doc));
            again.document = adm::parseXml(xml);
            adm::addCommonDefinitionsTo(again.document);
            for (const auto& id : chna_ids) {
                again.track_of_uid[admio::upper(id.uid())] = static_cast<int>(id.trackIndex()) - 1;
            }
            const admio::selection back = admio::select_items(again);
            for (const auto& w : back.warnings) {
                lines.push_back(tag + "roundtrip read warning: " + w);
            }
            const auto differences = compare(sel, back, length);
            if (differences.empty()) {
                lines.push_back(tag + "roundtrip: same");
            }
            for (const auto& d : differences) {
                lines.push_back(tag + "roundtrip: " + d);
            }
        }
        catch (const std::exception& e) {
            lines.push_back(tag + "roundtrip error: " + e.what());
        }
    }
    return lines;
}

// ---- the manifest -----------------------------------------------------

using manifest = std::map<std::string, std::vector<std::string>>;

manifest read_manifest(const std::string& path)
{
    manifest m;
    std::ifstream in(path);
    std::string line, current;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("file: ", 0) == 0) {
            current = line.substr(6);
            m[current];
        }
        else if (!current.empty() && line.rfind("  ", 0) == 0) {
            m[current].push_back(line.substr(2));
        }
    }
    return m;
}

void write_manifest(const std::string& path, const manifest& m, const std::string& layout_name)
{
    std::ofstream out(path);
    out << "# What adm_corpus found in each file of the corpus, recorded with\n"
           "# 'adm_corpus --record' and checked by the adm_corpus test (layout "
        << layout_name << ").\n"
           "# A difference is a change of behaviour to look at, not necessarily a bug.\n";
    for (const auto& entry : m) {
        out << "\nfile: " << entry.first << "\n";
        for (const auto& line : entry.second) {
            out << "  " << line << "\n";
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::string manifest_path;
    bool record = false;
    std::string layout_name = "4+5+0";
    std::vector<std::string> inputs;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--record" || arg == "--check") && i + 1 < argc) {
            record = arg == "--record";
            manifest_path = argv[++i];
        }
        else if (arg == "--layout" && i + 1 < argc) {
            layout_name = argv[++i];
        }
        else {
            inputs.push_back(arg);
        }
    }
    if (inputs.empty()) {
        const char* env = std::getenv("EARMAX_ADM_CORPUS");
        if (!env || !*env) {
            std::cout << "adm_corpus: no corpus (EARMAX_ADM_CORPUS is not set); skipped\n";
            return k_skipped;
        }
        inputs.push_back(env);
    }

    std::vector<std::string> files;
    for (const auto& input : inputs) {
        if (is_directory(input)) {
            const auto found = wav_files(input);
            files.insert(files.end(), found.begin(), found.end());
        }
        else if (is_file(input)) {
            files.push_back(input);
        }
        else {
            std::cerr << "adm_corpus: " << input << " is neither a file nor a directory\n";
            return 2;
        }
    }
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) { return basename_of(a) < basename_of(b); });
    if (files.empty()) {
        std::cout << "adm_corpus: the corpus has no .wav files; skipped\n";
        return k_skipped;
    }

    manifest found;
    for (const auto& file : files) {
        const std::string name = basename_of(file);
        std::cout << name << "\n";
        const auto lines = examine(file, layout_name);
        for (const auto& line : lines) {
            std::cout << "  " << line << "\n";
        }
        found[name] = lines;
    }

    if (manifest_path.empty()) {
        return 0;
    }
    if (record) {
        write_manifest(manifest_path, found, layout_name);
        std::cout << "recorded " << found.size() << " files in " << manifest_path << "\n";
        return 0;
    }
    const manifest expected = read_manifest(manifest_path);
    size_t problems = 0;
    for (const auto& entry : found) {
        const auto it = expected.find(entry.first);
        if (it == expected.end()) {
            std::cout << "NOT IN MANIFEST: " << entry.first << "\n";
            ++problems;
            continue;
        }
        if (it->second != entry.second) {
            std::cout << "DIFFERS: " << entry.first << "\n";
            for (const auto& line : it->second) {
                if (std::find(entry.second.begin(), entry.second.end(), line) == entry.second.end()) std::cout << "  - " << line << "\n";
            }
            for (const auto& line : entry.second) {
                if (std::find(it->second.begin(), it->second.end(), line) == it->second.end()) std::cout << "  + " << line << "\n";
            }
            ++problems;
        }
    }
    for (const auto& entry : expected) {
        if (!found.count(entry.first)) {
            std::cout << "MISSING FROM CORPUS: " << entry.first << "\n";
            ++problems;
        }
    }
    if (problems) {
        std::cout << problems << " file(s) differ from " << manifest_path << "\n";
        return 1;
    }
    std::cout << found.size() << " files as recorded in " << manifest_path << "\n";
    return 0;
}
