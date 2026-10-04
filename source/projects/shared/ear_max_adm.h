/// @file
/// @brief   ADM / BW64 file support for ear.adm: reading a file's ADM metadata
///          into rendering items the way the EAR selects them, and writing a
///          captured object timeline back to an ADM file.
///
/// The ADM structure is handled by libadm and the BW64 container by libbw64
/// (both EBU, Apache-2.0). What neither does, and what the EAR's select_items
/// and timing rules do in Python, is here: resolving which track carries
/// which item of which type, and turning the timed audioBlockFormats of an
/// Objects channel into renderer updates with the reference's interpolation
/// rules (BS.2127 section 7.2 / EAR InterpretObjectMetadata):
///
/// - a block interpolates from the previous block's gains over the whole
///   block when jumpPosition is not set, over interpolationLength when
///   jumpPosition is set with a length, and not at all (jump) when
///   jumpPosition is set without a length;
/// - interpolation only happens when the block starts exactly where the
///   previous block ended; otherwise the new values apply immediately.
///
/// Our renderers ramp gains linearly like the EAR, so emitting each block
/// with 'setvalue N ramp <interpolation ms>' followed by its parameters
/// reproduces the reference behaviour.
///
/// Known limitations of this phase: audioObject importance and
/// complementary object groups are not interpreted (every object is
/// rendered); muted objects, silent tracks and unsupported types are skipped
/// with a warning; nested audioObjects contribute their innermost
/// start/duration.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "adm/adm.hpp"
#include "adm/common_definitions.hpp"
#include "adm/parse.hpp"
#include "adm/utilities/id_assignment.hpp"
#include "adm/utilities/object_creation.hpp"
#include "adm/write.hpp"
#include "bw64/bw64.hpp"
#include "ear/bs2051.hpp"
#include "ear/layout.hpp"

#include "ear_max_hoa.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace earmax::admio {

// ----------------------------------------------------------------------
// time helpers
// ----------------------------------------------------------------------

inline double seconds(const adm::Time& time)
{
    if (time.isFractional()) {
        const auto f = time.asFractional();
        return static_cast<double>(f.numerator()) / static_cast<double>(f.denominator());
    }
    return static_cast<double>(time.asNanoseconds().count()) / 1e9;
}

inline std::chrono::nanoseconds to_nanoseconds(double seconds_value)
{
    return std::chrono::nanoseconds(static_cast<int64_t>(std::llround(seconds_value * 1e9)));
}

inline adm::Time to_time(double seconds_value)
{
    return adm::Time(std::chrono::nanoseconds(static_cast<int64_t>(std::llround(seconds_value * 1e9))));
}

inline bool same_time(double a, double b)
{
    return std::abs(a - b) < 1e-9;
}

inline std::string upper(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

// ----------------------------------------------------------------------
// rendering items
// ----------------------------------------------------------------------

/// The parameters of one audioBlockFormatObjects, in the terms of our
/// renderers (object_metadata in ear_max.h).
struct exclusion_zone {
    bool cartesian{ false };
    /// the bounds in the order of the renderers' 'zone' message: minAz maxAz
    /// minEl maxEl for a polar zone, minX maxX minY maxY minZ maxZ for a
    /// Cartesian one
    std::vector<double> bounds;
    std::string label;    ///< the zone element's value in the file, if any (the 'zone' messages carry none)
};

struct object_state {
    bool cartesian{ false };
    double azimuth{ 0.0 };
    double elevation{ 0.0 };
    double distance{ 1.0 };
    double x{ 0.0 };
    double y{ 1.0 };
    double z{ 0.0 };
    double width{ 0.0 };
    double height{ 0.0 };
    double depth{ 0.0 };
    double gain{ 1.0 };
    double diffuse{ 0.0 };
    bool channellock{ false };
    double channellock_distance{ 0.0 };
    double divergence{ 0.0 };
    double divergence_range{ 45.0 };
    bool screenref{ false };
    std::string screenedgelock_h{ "none" };
    std::string screenedgelock_v{ "none" };
    std::vector<exclusion_zone> zones;    ///< the block's zoneExclusion, in file order
};

struct object_block {
    double start{ 0.0 };
    double end{ std::numeric_limits<double>::infinity() };
    double interp{ 0.0 };    ///< seconds to interpolate from the previous block, 0 for a jump
    object_state state;
};

/// A speaker label without the BS.2051 URN prefix the common definitions
/// use ("urn:itu:bs:2051:0:speaker:M+030" -> "M+030"), with the LFE
/// spellings of the common definitions (LFE, LFEL, LFER) mapped to the
/// layouts' (LFE1, LFE2) as libear does.
inline std::string nominal_label(const std::string& label)
{
    std::string out = label;
    const std::string prefix = "urn:itu:bs:2051:";
    if (out.compare(0, prefix.size(), prefix) == 0) {
        const size_t colon = out.find(":speaker:", prefix.size());
        if (colon != std::string::npos) {
            out = out.substr(colon + 9);
        }
    }
    if (out == "LFE" || out == "LFEL") return "LFE1";
    if (out == "LFER") return "LFE2";
    return out;
}

/// The speaker labels of a Dolby Atmos master's bed (Dolby Atmos Master ADM
/// Profile, table 2-14) translated to the BS.2051 labels of the loudspeakers
/// at the same places, so that a layout with the loudspeaker takes the
/// channel directly and the LFE is known as such; the profile's Cartesian
/// positions coincide with the EAR's allocentric positions of these
/// loudspeakers, so a layout without them places the channel as the EAR
/// does, by position. Other labels are returned unchanged.
inline std::string dolby_speaker_label(const std::string& label)
{
    static const std::pair<const char*, const char*> table[] = {
        { "RC_L", "M+030" },   { "RC_R", "M-030" },   { "RC_C", "M+000" },   { "RC_LFE", "LFE1" },
        { "RC_Lss", "M+090" }, { "RC_Rss", "M-090" }, { "RC_Lrs", "M+135" }, { "RC_Rrs", "M-135" },
        { "RC_Lts", "U+090" }, { "RC_Rts", "U-090" }, { "RC_Ls", "M+110" },  { "RC_Rs", "M-110" },
    };
    for (const auto& entry : table) {
        if (label == entry.first) {
            return entry.second;
        }
    }
    return label;
}

struct objects_item {
    std::string name;
    int track{ 0 };    ///< 0-based file track
    std::vector<object_block> blocks;
};

/// One audioBlockFormatDirectSpeakers: the channel's labels and nominal
/// position for a span of time (most beds have one block for the whole
/// file).
struct direct_block {
    double start{ 0.0 };
    double end{ std::numeric_limits<double>::infinity() };
    std::vector<std::string> labels;
    bool has_position{ false };
    bool cartesian{ false };    ///< the position is x y z (Dolby Atmos beds) instead of polar
    double azimuth{ 0.0 };
    double elevation{ 0.0 };
    double distance{ 1.0 };
    double x{ 0.0 };
    double y{ 1.0 };
    double z{ 0.0 };
    std::vector<double> bounds;    ///< azimuthMin azimuthMax elevationMin elevationMax [distanceMin distanceMax], or XMin XMax YMin YMax ZMin ZMax
};

struct direct_item {
    std::string name;
    int track{ 0 };
    // the first block's parameters (the whole channel's for a static bed)
    std::vector<std::string> labels;
    bool has_position{ false };
    bool cartesian{ false };    ///< the position is x y z (Dolby Atmos beds) instead of polar
    double azimuth{ 0.0 };
    double elevation{ 0.0 };
    double distance{ 1.0 };
    double x{ 0.0 };
    double y{ 1.0 };
    double z{ 0.0 };
    std::vector<double> bounds;    ///< azimuthMin azimuthMax elevationMin elevationMax [distanceMin distanceMax], or XMin XMax YMin YMax ZMin ZMax
    bool lfe{ false };
    std::string pack_id;
    std::vector<direct_block> blocks;    ///< every block, in time order

    bool timed() const
    {
        return blocks.size() > 1;
    }
};

struct hoa_item {
    std::string name;
    std::vector<int> tracks;    ///< 0-based file tracks in ACN order
    int order{ 0 };
    std::string normalization{ "SN3D" };
    bool complete{ true };    ///< every component from 0 to (order+1)^2-1 present exactly once
};

struct selection {
    std::vector<std::string> programmes;
    int programme{ -1 };
    std::vector<objects_item> objects;
    std::vector<direct_item> direct;
    std::vector<hoa_item> hoa;
    std::vector<std::string> warnings;
};

struct file_info {
    std::string path;
    uint32_t samplerate{ 48000 };
    uint16_t channels{ 0 };
    uint16_t bitdepth{ 24 };
    uint64_t frames{ 0 };
};

struct loaded_file {
    file_info info;
    std::shared_ptr<adm::Document> document;
    std::map<std::string, int> track_of_uid;    ///< audioTrackUID id -> 0-based track
};

// ----------------------------------------------------------------------
// reading
// ----------------------------------------------------------------------

/// Open a BW64 file and parse its ADM metadata. Throws std::runtime_error
/// with a message for the Max console when the file cannot be used.
inline loaded_file load_file(const std::string& path)
{
    loaded_file result;
    auto reader = bw64::readFile(path);
    result.info.path = path;
    result.info.samplerate = reader->sampleRate();
    result.info.channels = reader->channels();
    result.info.bitdepth = reader->bitDepth();
    result.info.frames = reader->numberOfFrames();

    auto axml = reader->axmlChunk();
    if (!axml) {
        throw std::runtime_error("the file has no axml chunk (no ADM metadata)");
    }
    std::stringstream xml;
    axml->write(xml);
    result.document = adm::parseXml(xml);
    adm::addCommonDefinitionsTo(result.document);

    auto chna = reader->chnaChunk();
    if (!chna) {
        throw std::runtime_error("the file has no chna chunk (no track to audioTrackUID mapping)");
    }
    for (const auto& id : chna->audioIds()) {
        std::string uid = id.uid();
        while (!uid.empty() && (uid.back() == '\0' || uid.back() == ' ')) {
            uid.pop_back();
        }
        if (id.trackIndex() > 0) {
            result.track_of_uid[upper(uid)] = static_cast<int>(id.trackIndex()) - 1;
        }
    }
    return result;
}

namespace detail {

inline std::shared_ptr<const adm::AudioChannelFormat> channel_format_of(const std::shared_ptr<const adm::AudioTrackUid>& uid)
{
    if (auto cf = uid->getReference<adm::AudioChannelFormat>()) {
        return cf;
    }
    if (auto tf = uid->getReference<adm::AudioTrackFormat>()) {
        if (auto sf = tf->getReference<adm::AudioStreamFormat>()) {
            return sf->getReference<adm::AudioChannelFormat>();
        }
    }
    return nullptr;
}

template <class element>
std::string name_of(const element& e)
{
    return e.template has<adm::AudioObjectName>() ? e.template get<adm::AudioObjectName>().get() : std::string();
}

inline object_state state_of(const adm::AudioBlockFormatObjects& b)
{
    object_state s;
    s.cartesian = b.has<adm::Cartesian>() ? b.get<adm::Cartesian>().get() : false;
    if (b.has<adm::CartesianPosition>()) {
        const auto p = b.get<adm::CartesianPosition>();
        s.x = p.get<adm::X>().get();
        s.y = p.get<adm::Y>().get();
        s.z = p.has<adm::Z>() ? p.get<adm::Z>().get() : 0.0;
        s.cartesian = true;
        if (p.has<adm::ScreenEdgeLock>()) {
            const auto lock = p.get<adm::ScreenEdgeLock>();
            if (lock.has<adm::HorizontalEdge>()) s.screenedgelock_h = lock.get<adm::HorizontalEdge>().get();
            if (lock.has<adm::VerticalEdge>()) s.screenedgelock_v = lock.get<adm::VerticalEdge>().get();
        }
    }
    else if (b.has<adm::SphericalPosition>()) {
        const auto p = b.get<adm::SphericalPosition>();
        s.azimuth = p.get<adm::Azimuth>().get();
        s.elevation = p.get<adm::Elevation>().get();
        s.distance = p.has<adm::Distance>() ? p.get<adm::Distance>().get() : 1.0;
        if (p.has<adm::ScreenEdgeLock>()) {
            const auto lock = p.get<adm::ScreenEdgeLock>();
            if (lock.has<adm::HorizontalEdge>()) s.screenedgelock_h = lock.get<adm::HorizontalEdge>().get();
            if (lock.has<adm::VerticalEdge>()) s.screenedgelock_v = lock.get<adm::VerticalEdge>().get();
        }
    }
    s.width = b.get<adm::Width>().get();
    s.height = b.get<adm::Height>().get();
    s.depth = b.get<adm::Depth>().get();
    s.gain = b.get<adm::Gain>().asLinear();
    s.diffuse = b.get<adm::Diffuse>().get();
    if (b.has<adm::ChannelLock>()) {
        const auto lock = b.get<adm::ChannelLock>();
        s.channellock = lock.get<adm::ChannelLockFlag>().get();
        s.channellock_distance = lock.has<adm::MaxDistance>() ? lock.get<adm::MaxDistance>().get() : 0.0;
    }
    if (b.has<adm::ObjectDivergence>()) {
        const auto div = b.get<adm::ObjectDivergence>();
        s.divergence = div.get<adm::Divergence>().get();
        if (s.cartesian) {
            s.divergence_range = div.has<adm::PositionRange>() ? div.get<adm::PositionRange>().get() : 0.0;
        }
        else {
            s.divergence_range = div.has<adm::AzimuthRange>() ? div.get<adm::AzimuthRange>().get() : 45.0;
        }
    }
    s.screenref = b.get<adm::ScreenRef>().get();
    for (const adm::Zone& zone : b.get<adm::ZoneExclusion>().get<adm::Zones>()) {
        exclusion_zone z;
        if (adm::isCartesian(zone)) {
            const auto c = boost::get<adm::CartesianZone>(zone);
            z.cartesian = true;
            z.bounds = { c.get<adm::MinX>().get(), c.get<adm::MaxX>().get(), c.get<adm::MinY>().get(),
                         c.get<adm::MaxY>().get(), c.get<adm::MinZ>().get(), c.get<adm::MaxZ>().get() };
            if (c.has<adm::ZoneLabel>()) z.label = c.get<adm::ZoneLabel>().get();
        }
        else {
            const auto p = boost::get<adm::PolarZone>(zone);
            z.bounds = { p.get<adm::MinAzimuth>().get(), p.get<adm::MaxAzimuth>().get(),
                         p.get<adm::MinElevation>().get(), p.get<adm::MaxElevation>().get() };
            if (p.has<adm::ZoneLabel>()) z.label = p.get<adm::ZoneLabel>().get();
        }
        s.zones.push_back(std::move(z));
    }
    return s;
}

/// The EAR's interpolation length for a block of the given duration.
inline double interp_length(const adm::AudioBlockFormatObjects& b, double duration)
{
    if (b.has<adm::JumpPosition>()) {
        const auto jump = b.get<adm::JumpPosition>();
        if (jump.get<adm::JumpPositionFlag>().get()) {
            return jump.has<adm::InterpolationLength>() ? seconds(jump.get<adm::InterpolationLength>().get()) : 0.0;
        }
    }
    return duration;
}

struct object_context {
    std::shared_ptr<const adm::AudioObject> object;
    double start{ 0.0 };
    double end{ std::numeric_limits<double>::infinity() };
    double gain{ 1.0 };
};

inline void collect_objects(const std::shared_ptr<const adm::AudioObject>& object, std::vector<object_context>& out,
                            std::set<std::string>& seen, selection& result)
{
    if (!object) {
        return;
    }
    const std::string id = adm::formatId(object->get<adm::AudioObjectId>());
    if (!seen.insert(id).second) {
        return;
    }
    object_context ctx;
    ctx.object = object;
    if (object->has<adm::Start>()) {
        ctx.start = seconds(object->get<adm::Start>().get());
    }
    if (object->has<adm::Duration>()) {
        ctx.end = ctx.start + seconds(object->get<adm::Duration>().get());
    }
    if (object->has<adm::Gain>()) {
        ctx.gain = object->get<adm::Gain>().asLinear();
    }
    if (object->has<adm::Mute>() && object->get<adm::Mute>().get()) {
        result.warnings.push_back("audioObject '" + name_of(*object) + "' is muted and was skipped");
        return;
    }
    out.push_back(ctx);
    for (const auto& nested : object->getReferences<adm::AudioObject>()) {
        collect_objects(nested, out, seen, result);
    }
}

} // namespace detail

/// Resolve the rendering items of a document the way the EAR does for its
/// renderer: the selected audioProgramme (by index, -1 for the first), its
/// contents and objects, their tracks and channel formats.
inline selection select_items(const loaded_file& file, int programme_index = -1)
{
    using namespace detail;
    selection result;
    const auto& doc = file.document;

    std::vector<std::shared_ptr<const adm::AudioProgramme>> programmes;
    for (const auto& p : doc->getElements<adm::AudioProgramme>()) {
        programmes.push_back(p);
        result.programmes.push_back(p->has<adm::AudioProgrammeName>() ? p->get<adm::AudioProgrammeName>().get() : std::string());
    }

    std::vector<object_context> objects;
    std::set<std::string> seen;
    if (!programmes.empty()) {
        if (programme_index < 0 || programme_index >= static_cast<int>(programmes.size())) {
            programme_index = 0;
        }
        result.programme = programme_index;
        for (const auto& content : programmes[static_cast<size_t>(programme_index)]->getReferences<adm::AudioContent>()) {
            for (const auto& object : content->getReferences<adm::AudioObject>()) {
                collect_objects(object, objects, seen, result);
            }
        }
    }
    else {
        // no programme: every audioObject that no other object references
        std::set<std::string> nested;
        for (const auto& object : doc->getElements<adm::AudioObject>()) {
            for (const auto& n : object->getReferences<adm::AudioObject>()) {
                nested.insert(adm::formatId(n->get<adm::AudioObjectId>()));
            }
        }
        for (const auto& object : doc->getElements<adm::AudioObject>()) {
            if (!nested.count(adm::formatId(object->get<adm::AudioObjectId>()))) {
                collect_objects(object, objects, seen, result);
            }
        }
    }

    for (const auto& ctx : objects) {
        const auto& object = ctx.object;
        const std::string name = name_of(*object);
        std::shared_ptr<const adm::AudioPackFormat> object_pack;
        for (const auto& pack : object->getReferences<adm::AudioPackFormat>()) {
            object_pack = pack;
            break;
        }

        hoa_item hoa;
        std::vector<std::pair<int, int>> hoa_acn;    // (acn, track)
        std::shared_ptr<const adm::AudioPackFormat> hoa_pack;

        for (const auto& uid : object->getReferences<adm::AudioTrackUid>()) {
            const std::string uid_id = upper(adm::formatId(uid->get<adm::AudioTrackUidId>()));
            if (uid_id == "ATU_00000000") {
                result.warnings.push_back("audioObject '" + name + "': silent track (ATU_00000000) ignored");
                continue;
            }
            const auto track_it = file.track_of_uid.find(uid_id);
            if (track_it == file.track_of_uid.end()) {
                result.warnings.push_back("audioObject '" + name + "': " + uid_id + " is not in the chna chunk; track skipped");
                continue;
            }
            const int track = track_it->second;
            const auto cf = channel_format_of(uid);
            if (!cf) {
                result.warnings.push_back("audioObject '" + name + "': " + uid_id + " has no audioChannelFormat; track skipped");
                continue;
            }
            auto pack = uid->getReference<adm::AudioPackFormat>();
            if (!pack) {
                pack = object_pack;
            }
            const adm::TypeDescriptor type = cf->get<adm::TypeDescriptor>();

            if (type == adm::TypeDefinition::OBJECTS) {
                objects_item item;
                item.name = name;
                item.track = track;
                double previous_end = std::numeric_limits<double>::quiet_NaN();
                for (const auto& b : cf->getElements<adm::AudioBlockFormatObjects>()) {
                    object_block block;
                    block.start = ctx.start + (b.has<adm::Rtime>() ? seconds(b.get<adm::Rtime>().get()) : 0.0);
                    block.end = b.has<adm::Duration>() ? block.start + seconds(b.get<adm::Duration>().get()) : ctx.end;
                    const double duration = std::isinf(block.end) ? std::numeric_limits<double>::infinity() : block.end - block.start;
                    const double interp = interp_length(b, duration);
                    block.interp = (!std::isnan(previous_end) && same_time(block.start, previous_end)) ? interp : 0.0;
                    if (std::isinf(block.interp)) {
                        block.interp = 0.0;
                    }
                    block.state = state_of(b);
                    block.state.gain *= ctx.gain;
                    previous_end = block.end;
                    item.blocks.push_back(block);
                }
                if (item.blocks.empty()) {
                    result.warnings.push_back("audioObject '" + name + "': no audioBlockFormat; track skipped");
                    continue;
                }
                result.objects.push_back(std::move(item));
            }
            else if (type == adm::TypeDefinition::DIRECT_SPEAKERS) {
                direct_item item;
                item.name = name;
                item.track = track;
                if (pack) {
                    item.pack_id = adm::formatId(pack->get<adm::AudioPackFormatId>());
                }
                for (const auto& b : cf->getElements<adm::AudioBlockFormatDirectSpeakers>()) {
                    direct_block block;
                    block.start = ctx.start + (b.has<adm::Rtime>() ? seconds(b.get<adm::Rtime>().get()) : 0.0);
                    block.end = b.has<adm::Duration>() ? block.start + seconds(b.get<adm::Duration>().get()) : ctx.end;
                    for (const auto& label : b.get<adm::SpeakerLabels>()) {
                        block.labels.push_back(dolby_speaker_label(label.get()));
                    }
                    if (b.has<adm::SphericalSpeakerPosition>() || b.has<adm::CartesianSpeakerPosition>()) {
                        if (b.has<adm::SphericalSpeakerPosition>()) {
                            const auto spherical = b.get<adm::SphericalSpeakerPosition>();
                            const auto* sp = &spherical;
                            block.has_position = true;
                            block.azimuth = sp->get<adm::Azimuth>().get();
                            block.elevation = sp->get<adm::Elevation>().get();
                            block.distance = sp->has<adm::Distance>() ? sp->get<adm::Distance>().get() : 1.0;
                            const bool az_bounds = sp->has<adm::AzimuthMin>() || sp->has<adm::AzimuthMax>();
                            const bool el_bounds = sp->has<adm::ElevationMin>() || sp->has<adm::ElevationMax>();
                            const bool dist_bounds = sp->has<adm::DistanceMin>() || sp->has<adm::DistanceMax>();
                            if (az_bounds || el_bounds || dist_bounds) {
                                block.bounds = { sp->has<adm::AzimuthMin>() ? sp->get<adm::AzimuthMin>().get() : block.azimuth,
                                                 sp->has<adm::AzimuthMax>() ? sp->get<adm::AzimuthMax>().get() : block.azimuth,
                                                 sp->has<adm::ElevationMin>() ? sp->get<adm::ElevationMin>().get() : block.elevation,
                                                 sp->has<adm::ElevationMax>() ? sp->get<adm::ElevationMax>().get() : block.elevation };
                                if (dist_bounds) {
                                    block.bounds.push_back(sp->has<adm::DistanceMin>() ? sp->get<adm::DistanceMin>().get() : block.distance);
                                    block.bounds.push_back(sp->has<adm::DistanceMax>() ? sp->get<adm::DistanceMax>().get() : block.distance);
                                }
                            }
                        }
                        else {
                            const auto cp = b.get<adm::CartesianSpeakerPosition>();
                            block.has_position = true;
                            block.cartesian = true;
                            block.x = cp.get<adm::X>().get();
                            block.y = cp.get<adm::Y>().get();
                            block.z = cp.has<adm::Z>() ? cp.get<adm::Z>().get() : 0.0;
                            if (cp.has<adm::XMin>() || cp.has<adm::XMax>() || cp.has<adm::YMin>() || cp.has<adm::YMax>()
                                || cp.has<adm::ZMin>() || cp.has<adm::ZMax>()) {
                                block.bounds = { cp.has<adm::XMin>() ? cp.get<adm::XMin>().get() : block.x,
                                                 cp.has<adm::XMax>() ? cp.get<adm::XMax>().get() : block.x,
                                                 cp.has<adm::YMin>() ? cp.get<adm::YMin>().get() : block.y,
                                                 cp.has<adm::YMax>() ? cp.get<adm::YMax>().get() : block.y,
                                                 cp.has<adm::ZMin>() ? cp.get<adm::ZMin>().get() : block.z,
                                                 cp.has<adm::ZMax>() ? cp.get<adm::ZMax>().get() : block.z };
                            }
                        }
                    }
                    item.blocks.push_back(std::move(block));
                }
                if (item.blocks.empty()) {
                    result.warnings.push_back("audioObject '" + name + "': no audioBlockFormat; track skipped");
                    continue;
                }
                // the first block stands for the channel (the whole of it for a static bed)
                const direct_block& first = item.blocks.front();
                item.labels = first.labels;
                item.has_position = first.has_position;
                item.cartesian = first.cartesian;
                item.azimuth = first.azimuth;
                item.elevation = first.elevation;
                item.distance = first.distance;
                item.x = first.x;
                item.y = first.y;
                item.z = first.z;
                item.bounds = first.bounds;
                if (cf->has<adm::Frequency>()) {
                    const auto f = cf->get<adm::Frequency>();
                    item.lfe = f.has<adm::LowPass>();
                }
                // an LFE label marks the channel too, as libear's calculator
                // takes it (a Dolby Atmos master has no frequency element)
                for (const auto& label : item.labels) {
                    const std::string nominal = nominal_label(label);
                    if (nominal == "LFE1" || nominal == "LFE2") {
                        item.lfe = true;
                    }
                }
                result.direct.push_back(std::move(item));
            }
            else if (type == adm::TypeDefinition::HOA) {
                for (const auto& b : cf->getElements<adm::AudioBlockFormatHoa>()) {
                    if (!b.has<adm::Order>() || !b.has<adm::Degree>()) {
                        result.warnings.push_back("audioObject '" + name + "': HOA channel without order/degree; track skipped");
                        break;
                    }
                    const int n = b.get<adm::Order>().get();
                    const int m = b.get<adm::Degree>().get();
                    hoa_acn.emplace_back(hoa::to_acn(n, m), track);
                    hoa.order = std::max(hoa.order, n);
                    if (b.has<adm::Normalization>()) {
                        hoa.normalization = b.get<adm::Normalization>().get();
                    }
                    break;
                }
                hoa_pack = pack;
            }
            else {
                result.warnings.push_back("audioObject '" + name + "': unsupported typeDefinition; track skipped");
            }
        }

        if (!hoa_acn.empty()) {
            hoa.name = name;
            if (hoa_pack) {
                if (auto hp = std::dynamic_pointer_cast<const adm::AudioPackFormatHoa>(hoa_pack)) {
                    // the pack's normalization only when it is set (its default would
                    // override what the blocks say)
                    if (hp->has<adm::Normalization>() && !hp->isDefault<adm::Normalization>()) {
                        hoa.normalization = hp->get<adm::Normalization>().get();
                    }
                }
            }
            std::sort(hoa_acn.begin(), hoa_acn.end());
            const size_t expected = hoa::component_count(hoa.order);
            hoa.complete = hoa_acn.size() == expected;
            for (size_t i = 0; i < hoa_acn.size(); ++i) {
                if (hoa_acn[i].first != static_cast<int>(i)) {
                    hoa.complete = false;
                }
                hoa.tracks.push_back(hoa_acn[i].second);
            }
            if (!hoa.complete) {
                result.warnings.push_back("audioObject '" + name + "': HOA components are not a complete set in ACN order for order "
                                          + std::to_string(hoa.order) + "; mc.ear.hoa~ expects (order+1)^2 channels");
            }
            result.hoa.push_back(std::move(hoa));
        }
    }
    return result;
}

/// Index of the DirectSpeakers block active at `time` (seconds): the last
/// block starting at or before it, or -1 before the first (a bed keeps
/// its last block's parameters after it ends).
inline int direct_block_at(const direct_item& item, double time)
{
    int current = -1;
    for (size_t k = 0; k < item.blocks.size(); ++k) {
        if (item.blocks[k].start <= time + 1e-6) {
            current = static_cast<int>(k);
        }
        else {
            break;
        }
    }
    return current;
}

/// Index of the block of an item active at `time` (seconds), or -1 before the
/// first block. After the last block its last values keep applying.
inline int block_at(const objects_item& item, double time)
{
    int current = -1;
    for (size_t i = 0; i < item.blocks.size(); ++i) {
        if (item.blocks[i].start <= time + 1e-9) {
            current = static_cast<int>(i);
        }
        else {
            break;
        }
    }
    return current;
}

// ----------------------------------------------------------------------
// writing
// ----------------------------------------------------------------------

/// One captured change of an object's parameters at `time` seconds, reached
/// with a ramp of `ramp` seconds (0 for a jump).
struct captured_block {
    double time{ 0.0 };
    double ramp{ 0.0 };
    object_state state;
};

struct captured_object {
    std::string name;
    std::vector<captured_block> blocks;
};

/// One captured change of a DirectSpeakers channel's labels or position at
/// `time` seconds (a timed bed: one audioBlockFormat per change).
struct captured_direct_block {
    double time{ 0.0 };
    std::vector<std::string> labels;
    bool has_position{ false };
    bool cartesian{ false };    ///< the position is x y z instead of polar
    double azimuth{ 0.0 };
    double elevation{ 0.0 };
    double distance{ 1.0 };
    double x{ 0.0 };
    double y{ 1.0 };
    double z{ 0.0 };
    std::vector<double> bounds;    ///< polar: azimuthMin azimuthMax elevationMin elevationMax [distanceMin distanceMax]; Cartesian: XMin XMax YMin YMax ZMin ZMax
};

/// One captured DirectSpeakers channel: what mc.ear.direct~ takes for an
/// input channel (speaker labels, nominal position with optional bounds,
/// LFE, and the audioPackFormatID of a common definitions layout). The
/// labels and position are the channel's (a static bed) unless `blocks`
/// holds more than one change: then each becomes a timed block.
struct captured_direct_channel {
    std::vector<std::string> labels;
    bool has_position{ false };
    bool cartesian{ false };    ///< the position is x y z instead of polar
    double azimuth{ 0.0 };
    double elevation{ 0.0 };
    double distance{ 1.0 };
    double x{ 0.0 };
    double y{ 1.0 };
    double z{ 0.0 };
    std::vector<double> bounds;    ///< polar: azimuthMin azimuthMax elevationMin elevationMax [distanceMin distanceMax]; Cartesian: XMin XMax YMin YMax ZMin ZMax; or empty
    bool lfe{ false };
    std::string pack_id;    ///< common definitions audioPackFormatID (AP_0001xxxx) of the bed, or empty for a custom bed
    std::vector<captured_direct_block> blocks;    ///< the timed changes, when the bed was captured over time

    bool timed() const
    {
        return blocks.size() > 1;
    }
};

/// A captured channel bed: one DirectSpeakers audioObject whose tracks
/// follow the objects' in the file.
struct captured_bed {
    std::string name{ "bed" };
    std::vector<captured_direct_channel> channels;
};

/// A captured HOA scene: one audioObject with (order+1)^2 tracks in ACN
/// order after the bed's; an order below 0 means no scene.
struct captured_scene {
    std::string name{ "scene" };
    int order{ -1 };
    std::string normalization{ "SN3D" };

    size_t channels() const
    {
        return order < 0 ? 0 : hoa::component_count(order);
    }
};

/// Everything written as one audioProgramme: the objects (tracks 1..N),
/// the bed (the next tracks) and the scene (the last tracks).
struct captured_programme {
    std::string name{ "libear-max" };
    std::vector<captured_object> objects;
    captured_bed bed;
    captured_scene scene;

    size_t channels() const
    {
        return objects.size() + bed.channels.size() + scene.channels();
    }

    /// Keep what fits in `channels` tracks: the objects first, then the
    /// bed's channels, then the scene (dropped when incomplete).
    void limit(size_t channels)
    {
        if (objects.size() > channels) {
            objects.resize(channels);
        }
        const size_t for_bed = channels - objects.size();
        if (bed.channels.size() > for_bed) {
            bed.channels.resize(for_bed);
        }
        if (scene.channels() > channels - objects.size() - bed.channels.size()) {
            scene.order = -1;
        }
    }
};

namespace detail {

/// the stream and track formats and the track UID of one PCM channel,
/// referencing its channel and pack formats (as adm::createSimpleObject)
struct channel_chain {
    std::shared_ptr<adm::AudioTrackFormat> track_format;
    std::shared_ptr<adm::AudioTrackUid> uid;
};

inline channel_chain chain_channel(const std::shared_ptr<adm::AudioObject>& object, const std::shared_ptr<adm::AudioPackFormat>& pack,
                                   const std::shared_ptr<adm::AudioChannelFormat>& channel, const std::string& name)
{
    channel_chain c;
    auto stream = adm::AudioStreamFormat::create(adm::AudioStreamFormatName(name), adm::FormatDefinition::PCM);
    c.track_format = adm::AudioTrackFormat::create(adm::AudioTrackFormatName(name), adm::FormatDefinition::PCM);
    c.uid = adm::AudioTrackUid::create();
    pack->addReference(channel);
    stream->setReference(channel);
    c.track_format->setReference(stream);
    object->addReference(c.uid);
    c.uid->setReference(c.track_format);
    c.uid->setReference(pack);
    return c;
}

/// a chna entry for a track: the UID, track format and pack format ids
/// (valid once the document's ids are assigned)
struct chna_source {
    std::shared_ptr<adm::AudioTrackUid> uid;
    std::shared_ptr<adm::AudioTrackFormat> track_format;
    std::shared_ptr<adm::AudioPackFormat> pack;
};

inline void add_object_blocks(adm::SimpleObjectHolder& holder, const captured_object& object, double length)
{
    for (size_t k = 0; k < object.blocks.size(); ++k) {
        const captured_block& c = object.blocks[k];
        const object_state& s = c.state;
        const double next = k + 1 < object.blocks.size() ? object.blocks[k + 1].time : length;
        const double duration = std::max(0.0, next - c.time);

        auto make = [&](auto position) {
            adm::AudioBlockFormatObjects block(position);
            block.set(adm::Rtime(to_time(c.time)));
            block.set(adm::Duration(to_time(duration)));
            block.set(adm::Cartesian(s.cartesian));
            block.set(adm::Width(static_cast<float>(s.width)));
            block.set(adm::Height(static_cast<float>(s.height)));
            block.set(adm::Depth(static_cast<float>(s.depth)));
            block.set(adm::Gain::fromLinear(s.gain));
            block.set(adm::Diffuse(static_cast<float>(s.diffuse)));
            block.set(adm::ScreenRef(s.screenref));
            if (s.channellock) {
                if (s.channellock_distance > 0.0) {
                    block.set(adm::ChannelLock(adm::ChannelLockFlag(true), adm::MaxDistance(static_cast<float>(s.channellock_distance))));
                }
                else {
                    block.set(adm::ChannelLock(adm::ChannelLockFlag(true)));
                }
            }
            if (s.divergence > 0.0) {
                if (s.cartesian) {
                    block.set(adm::ObjectDivergence(adm::Divergence(static_cast<float>(s.divergence)),
                                                    adm::PositionRange(static_cast<float>(s.divergence_range))));
                }
                else {
                    block.set(adm::ObjectDivergence(adm::Divergence(static_cast<float>(s.divergence)),
                                                    adm::AzimuthRange(static_cast<float>(s.divergence_range))));
                }
            }
            if (!s.zones.empty()) {
                adm::ZoneExclusion exclusion;
                for (const exclusion_zone& z : s.zones) {
                    const auto f = [&z](size_t i) { return static_cast<float>(z.bounds[i]); };
                    if (z.cartesian) {
                        adm::CartesianZone zone(adm::MinX(f(0)), adm::MaxX(f(1)), adm::MinY(f(2)), adm::MaxY(f(3)),
                                                adm::MinZ(f(4)), adm::MaxZ(f(5)));
                        if (!z.label.empty()) zone.set(adm::ZoneLabel(z.label));
                        exclusion.add(adm::Zone(zone));
                    }
                    else {
                        adm::PolarZone zone(adm::MinElevation(f(2)), adm::MaxElevation(f(3)), adm::MinAzimuth(f(0)),
                                            adm::MaxAzimuth(f(1)));
                        if (!z.label.empty()) zone.set(adm::ZoneLabel(z.label));
                        exclusion.add(adm::Zone(zone));
                    }
                }
                block.set(exclusion);
            }
            // the renderer ramps over c.ramp seconds; a ramp of 0 is a jump
            if (c.ramp > 0.0) {
                block.set(adm::JumpPosition(adm::JumpPositionFlag(true),
                                            adm::InterpolationLength(to_nanoseconds(std::min(c.ramp, duration)))));
            }
            else {
                block.set(adm::JumpPosition(adm::JumpPositionFlag(true)));
            }
            holder.audioChannelFormat->add(block);
        };

        if (s.cartesian) {
            adm::CartesianPosition position(adm::X(static_cast<float>(s.x)), adm::Y(static_cast<float>(s.y)),
                                            adm::Z(static_cast<float>(s.z)));
            if (s.screenedgelock_h != "none" || s.screenedgelock_v != "none") {
                adm::ScreenEdgeLock lock;
                if (s.screenedgelock_h != "none") lock.set(adm::HorizontalEdge(s.screenedgelock_h));
                if (s.screenedgelock_v != "none") lock.set(adm::VerticalEdge(s.screenedgelock_v));
                position.set(lock);
            }
            make(position);
        }
        else {
            adm::SphericalPosition position(adm::Azimuth(static_cast<float>(s.azimuth)),
                                            adm::Elevation(static_cast<float>(s.elevation)),
                                            adm::Distance(static_cast<float>(s.distance)));
            if (s.screenedgelock_h != "none" || s.screenedgelock_v != "none") {
                adm::ScreenEdgeLock lock;
                if (s.screenedgelock_h != "none") lock.set(adm::HorizontalEdge(s.screenedgelock_h));
                if (s.screenedgelock_v != "none") lock.set(adm::VerticalEdge(s.screenedgelock_v));
                position.set(lock);
            }
            make(position);
        }
    }
}

/// The nominal position of a common definitions channel: what the file
/// reconstructs for it, and what 'inputlayout' sets.
struct common_channel {
    adm::AudioTrackFormatId track_id;    ///< AT_<channel id>_01 in the common definitions
    std::vector<std::array<double, 3>> nominal;    ///< the positions (azimuth, elevation, distance) the channel stands for
};

/// Whether a polar position is one of the nominal ones.
inline bool nominal_position(const common_channel& channel, double azimuth, double elevation, double distance)
{
    const double tolerance = 1e-6;
    for (const auto& p : channel.nominal) {
        if (std::abs(p[0] - azimuth) <= tolerance && std::abs(p[1] - elevation) <= tolerance
            && std::abs(p[2] - distance) <= tolerance) {
            return true;
        }
    }
    return false;
}

/// A bed whose channels all name the same common definitions pack and
/// carry one label each that names a distinct channel of that pack, at its
/// nominal position and without bounds: it is written as a reference to
/// that pack and its channels (as the EAR's tools do), otherwise as custom
/// channel formats, so that positions and bounds edited after 'inputlayout'
/// are kept. Returns the pack id, or empty with `why_not` set when the
/// pack cannot be used.
inline adm::AudioPackFormatId common_bed_pack(const captured_bed& bed, std::vector<adm::AudioTrackFormatId>& track_ids,
                                              std::vector<std::string>& labels, std::string& why_not)
{
    adm::AudioPackFormatId none;
    const std::string& id = bed.channels.front().pack_id;
    if (id.empty()) {
        return none;
    }
    adm::AudioPackFormatId pack_id;
    try {
        pack_id = adm::parseAudioPackFormatId(id);
    }
    catch (const std::exception&) {
        why_not = "packformat " + id + " is not a valid audioPackFormatID";
        return none;
    }
    const auto common = adm::getCommonDefinitions();
    const auto pack = common->lookup(pack_id);
    if (!pack) {
        why_not = "packformat " + id + " is not a common definitions layout";
        return none;
    }
    // the layout the pack stands for, when libear knows it: 'inputlayout'
    // sets its nominal positions, which differ from the common definitions'
    // for the LFE
    std::map<std::string, std::array<double, 3>> layout_position;
    for (const auto& entry : adm::audioPackFormatLookupTable()) {
        if (entry.second != pack_id) {
            continue;
        }
        try {
            const ear::Layout layout = ear::getLayout(entry.first);
            for (const auto& channel : layout.channels()) {
                const auto pos = channel.polarPositionNominal();
                layout_position[nominal_label(channel.name())] = { pos.azimuth, pos.elevation, pos.distance };
            }
        }
        catch (const std::exception&) {
        }
        break;
    }
    // the pack's channels by nominal label
    std::map<std::string, common_channel> channel_of_label;
    for (const auto& cf : pack->getReferences<adm::AudioChannelFormat>()) {
        std::string channel_id = adm::formatId(cf->get<adm::AudioChannelFormatId>());    // AC_0001xxxx
        channel_id.replace(0, 3, "AT_");
        adm::AudioTrackFormatId track_id;
        try {
            track_id = adm::parseAudioTrackFormatId(channel_id + "_01");
        }
        catch (const std::exception&) {
            continue;
        }
        if (!common->lookup(track_id)) {
            continue;
        }
        for (const auto& b : cf->getElements<adm::AudioBlockFormatDirectSpeakers>()) {
            common_channel channel;
            channel.track_id = track_id;
            if (b.has<adm::SphericalSpeakerPosition>()) {
                const auto sp = b.get<adm::SphericalSpeakerPosition>();
                channel.nominal.push_back({ static_cast<double>(sp.get<adm::Azimuth>().get()),
                                            static_cast<double>(sp.get<adm::Elevation>().get()),
                                            sp.has<adm::Distance>() ? static_cast<double>(sp.get<adm::Distance>().get()) : 1.0 });
            }
            for (const auto& label : b.get<adm::SpeakerLabels>()) {
                const std::string name = nominal_label(label.get());
                const auto layout = layout_position.find(name);
                if (layout != layout_position.end()) {
                    channel.nominal.push_back(layout->second);
                }
                channel_of_label.emplace(name, channel);
            }
            break;
        }
    }
    std::set<std::string> seen;
    for (const auto& ch : bed.channels) {
        if (ch.pack_id != id) {
            why_not = "the channels name different packformats";
            return none;
        }
        if (ch.timed()) {
            why_not = "packformat " + id + " cannot carry timed changes (the common definitions are static)";
            return none;
        }
        if (ch.labels.size() != 1) {
            why_not = "packformat " + id + " needs exactly one speaker label per channel";
            return none;
        }
        const std::string label = nominal_label(ch.labels.front());
        const auto it = channel_of_label.find(label);
        if (it == channel_of_label.end() || !seen.insert(label).second) {
            why_not = "packformat " + id + ": speaker label " + ch.labels.front() + " is not a distinct channel of that layout";
            return none;
        }
        if (ch.cartesian) {
            why_not = "packformat " + id + ": " + ch.labels.front() + " is given in Cartesian coordinates (the layout's channels are polar)";
            return none;
        }
        if (!ch.bounds.empty()) {
            why_not = "packformat " + id + ": " + ch.labels.front() + " has position bounds (the layout's channels have none)";
            return none;
        }
        if (ch.has_position && !nominal_position(it->second, ch.azimuth, ch.elevation, ch.distance)) {
            std::ostringstream pos;
            pos << ch.azimuth << " " << ch.elevation << " " << ch.distance;
            why_not = "packformat " + id + ": " + ch.labels.front() + " is not at its nominal position (" + pos.str() + ")";
            return none;
        }
        track_ids.push_back(it->second.track_id);
        labels.push_back(label);
    }
    return pack_id;
}

} // namespace detail

/// Build an ADM document for a captured programme: a programme, a content,
/// one Objects audioObject per captured object (pack, channel, stream and
/// track formats and track UID each), a DirectSpeakers audioObject for the
/// bed and an HOA audioObject for the scene. `length` is the audio length
/// in seconds (the last block of each object lasts until then). The chna
/// entries are appended to `chna_ids` in track order: objects, bed, scene.
/// Things written differently from what was asked are explained in
/// `warnings` (a bed whose packformat could not be used as a common
/// definitions layout).
inline std::shared_ptr<adm::Document> build_document(const captured_programme& captured, double length,
                                                     std::vector<bw64::AudioId>& chna_ids, std::vector<std::string>& warnings)
{
    auto doc = adm::Document::create();
    auto programme = adm::AudioProgramme::create(adm::AudioProgrammeName(captured.name));
    auto content = adm::AudioContent::create(adm::AudioContentName(captured.name));
    programme->addReference(content);
    doc->add(programme);

    std::vector<detail::chna_source> tracks;

    for (const auto& object : captured.objects) {
        auto holder = adm::createSimpleObject(object.name);
        content->addReference(holder.audioObject);
        detail::add_object_blocks(holder, object, length);
        tracks.push_back({ holder.audioTrackUid, holder.audioTrackFormat, holder.audioPackFormat });
    }

    if (!captured.bed.channels.empty()) {
        const captured_bed& bed = captured.bed;
        std::vector<adm::AudioTrackFormatId> track_ids;
        std::vector<std::string> labels;
        std::string why_not;
        const adm::AudioPackFormatId common = detail::common_bed_pack(bed, track_ids, labels, why_not);
        if (common != adm::AudioPackFormatId()) {
            adm::addCommonDefinitionsTo(doc);
            auto holder = adm::addTailoredCommonDefinitionsObjectTo(doc, bed.name, common, track_ids, labels);
            content->addReference(holder.audioObject);
            auto pack = doc->lookup(common);
            for (size_t i = 0; i < labels.size(); ++i) {
                tracks.push_back({ holder.audioTrackUids.at(labels[i]), doc->lookup(track_ids[i]), pack });
            }
        }
        else {
            if (!why_not.empty()) {
                warnings.push_back("bed '" + bed.name + "': " + why_not + "; written with its own channel formats");
            }
            auto object = adm::AudioObject::create(adm::AudioObjectName(bed.name));
            auto pack = adm::AudioPackFormat::create(adm::AudioPackFormatName(bed.name), adm::TypeDefinition::DIRECT_SPEAKERS);
            object->addReference(pack);
            content->addReference(object);
            doc->add(object);
            for (size_t i = 0; i < bed.channels.size(); ++i) {
                const captured_direct_channel& ch = bed.channels[i];
                const std::string name = bed.name + " " + (ch.labels.empty() ? std::to_string(i + 1) : ch.labels.front());
                auto channel = adm::AudioChannelFormat::create(adm::AudioChannelFormatName(name), adm::TypeDefinition::DIRECT_SPEAKERS);
                // a static channel is one block without timing; a timed one has
                // a block per change, each lasting until the next
                std::vector<captured_direct_block> blocks = ch.blocks;
                if (blocks.size() <= 1) {
                    captured_direct_block only;
                    only.labels = ch.labels;
                    only.has_position = ch.has_position;
                    only.cartesian = ch.cartesian;
                    only.azimuth = ch.azimuth;
                    only.elevation = ch.elevation;
                    only.distance = ch.distance;
                    only.x = ch.x;
                    only.y = ch.y;
                    only.z = ch.z;
                    only.bounds = ch.bounds;
                    blocks = { only };
                }
                for (size_t k = 0; k < blocks.size(); ++k) {
                    const captured_direct_block& c = blocks[k];
                    adm::AudioBlockFormatDirectSpeakers block;
                    if (blocks.size() > 1) {
                        const double next = k + 1 < blocks.size() ? blocks[k + 1].time : length;
                        block.set(adm::Rtime(to_time(c.time)));
                        block.set(adm::Duration(to_time(std::max(0.0, next - c.time))));
                    }
                    for (const auto& label : c.labels) {
                        block.add(adm::SpeakerLabel(label));
                    }
                    if (c.has_position && c.cartesian) {
                        adm::CartesianSpeakerPosition position(adm::X(static_cast<float>(c.x)), adm::Y(static_cast<float>(c.y)),
                                                               adm::Z(static_cast<float>(c.z)));
                        if (c.bounds.size() >= 6) {
                            position.set(adm::XMin(static_cast<float>(c.bounds[0])));
                            position.set(adm::XMax(static_cast<float>(c.bounds[1])));
                            position.set(adm::YMin(static_cast<float>(c.bounds[2])));
                            position.set(adm::YMax(static_cast<float>(c.bounds[3])));
                            position.set(adm::ZMin(static_cast<float>(c.bounds[4])));
                            position.set(adm::ZMax(static_cast<float>(c.bounds[5])));
                        }
                        block.set(position);
                    }
                    else if (c.has_position) {
                        adm::SphericalSpeakerPosition position(adm::Azimuth(static_cast<float>(c.azimuth)),
                                                              adm::Elevation(static_cast<float>(c.elevation)),
                                                              adm::Distance(static_cast<float>(c.distance)));
                        if (c.bounds.size() >= 4) {
                            position.set(adm::AzimuthMin(static_cast<float>(c.bounds[0])));
                            position.set(adm::AzimuthMax(static_cast<float>(c.bounds[1])));
                            position.set(adm::ElevationMin(static_cast<float>(c.bounds[2])));
                            position.set(adm::ElevationMax(static_cast<float>(c.bounds[3])));
                        }
                        if (c.bounds.size() >= 6) {
                            position.set(adm::DistanceMin(static_cast<float>(c.bounds[4])));
                            position.set(adm::DistanceMax(static_cast<float>(c.bounds[5])));
                        }
                        block.set(position);
                    }
                    channel->add(block);
                }
                if (ch.lfe) {
                    channel->set(adm::Frequency(adm::LowPass(120.0f)));
                }
                auto chain = detail::chain_channel(object, pack, channel, name);
                tracks.push_back({ chain.uid, chain.track_format, pack });
            }
        }
    }

    if (captured.scene.order >= 0) {
        const captured_scene& scene = captured.scene;
        auto object = adm::AudioObject::create(adm::AudioObjectName(scene.name));
        auto pack = adm::AudioPackFormatHoa::create(adm::AudioPackFormatName(scene.name), adm::Normalization(scene.normalization));
        object->addReference(pack);
        content->addReference(object);
        doc->add(object);
        for (size_t i = 0; i < scene.channels(); ++i) {
            int n = 0, m = 0;
            hoa::from_acn(static_cast<int>(i), n, m);
            const std::string name = scene.name + " " + std::to_string(i + 1);
            auto channel = adm::AudioChannelFormat::create(adm::AudioChannelFormatName(name), adm::TypeDefinition::HOA);
            channel->add(adm::AudioBlockFormatHoa(adm::Order(n), adm::Degree(m), adm::Normalization(scene.normalization)));
            auto chain = detail::chain_channel(object, pack, channel, name);
            tracks.push_back({ chain.uid, chain.track_format, pack });
        }
    }

    adm::reassignIds(doc);

    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& t = tracks[i];
        chna_ids.emplace_back(static_cast<uint16_t>(i + 1), adm::formatId(t.uid->get<adm::AudioTrackUidId>()),
                              adm::formatId(t.track_format->get<adm::AudioTrackFormatId>()),
                              adm::formatId(t.pack->get<adm::AudioPackFormatId>()));
    }
    return doc;
}

/// Objects only (no bed, no scene).
inline std::shared_ptr<adm::Document> build_document(const std::string& programme_name, const std::vector<captured_object>& objects,
                                                     double length, std::vector<bw64::AudioId>& chna_ids)
{
    captured_programme captured;
    captured.name = programme_name;
    captured.objects = objects;
    std::vector<std::string> warnings;
    return build_document(captured, length, chna_ids, warnings);
}

inline std::string to_xml(const std::shared_ptr<adm::Document>& doc)
{
    std::stringstream xml;
    adm::writeXml(xml, doc);
    return xml.str();
}

/// Write `audio_in` (any WAV/BW64 file) to `out` as a BW64 file carrying the
/// given ADM document and chna entries. Returns the number of frames written.
inline uint64_t write_file(const std::string& out, const std::string& audio_in, const std::shared_ptr<adm::Document>& doc,
                           const std::vector<bw64::AudioId>& chna_ids)
{
    auto reader = bw64::readFile(audio_in);
    for (const auto& id : chna_ids) {
        if (id.trackIndex() > reader->channels()) {
            throw std::runtime_error("the audio file has " + std::to_string(reader->channels()) + " channels but object "
                                     + std::to_string(id.trackIndex()) + " needs track " + std::to_string(id.trackIndex()));
        }
    }
    auto chna = std::make_shared<bw64::ChnaChunk>();
    for (const auto& id : chna_ids) {
        chna->addAudioId(id);
    }
    auto axml = std::make_shared<bw64::AxmlChunk>(to_xml(doc));
    auto writer = bw64::writeFile(out, reader->channels(), reader->sampleRate(), reader->bitDepth(), chna, axml);

    const uint64_t block = 4096;
    std::vector<float> buffer(block * reader->channels());
    uint64_t total = 0;
    while (true) {
        const uint64_t got = reader->read(buffer.data(), block);
        if (got == 0) {
            break;
        }
        writer->write(buffer.data(), got);
        total += got;
    }
    return total;
}

} // namespace earmax::admio
