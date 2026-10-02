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
/// Known limitations of this phase: zoneExclusion is not supported by libadm
/// 0.14 and is therefore not read or written; audioObject importance and
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

#include "ear_max_hoa.h"

#include <algorithm>
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
};

struct object_block {
    double start{ 0.0 };
    double end{ std::numeric_limits<double>::infinity() };
    double interp{ 0.0 };    ///< seconds to interpolate from the previous block, 0 for a jump
    object_state state;
};

struct objects_item {
    std::string name;
    int track{ 0 };    ///< 0-based file track
    std::vector<object_block> blocks;
};

struct direct_item {
    std::string name;
    int track{ 0 };
    std::vector<std::string> labels;
    bool has_position{ false };
    double azimuth{ 0.0 };
    double elevation{ 0.0 };
    double distance{ 1.0 };
    std::vector<double> bounds;    ///< azimuthMin azimuthMax elevationMin elevationMax [distanceMin distanceMax]
    bool lfe{ false };
    std::string pack_id;
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
                    for (const auto& label : b.get<adm::SpeakerLabels>()) {
                        item.labels.push_back(label.get());
                    }
                    if (b.has<adm::SphericalSpeakerPosition>() || b.has<adm::CartesianSpeakerPosition>()) {
                        if (b.has<adm::SphericalSpeakerPosition>()) {
                            const auto spherical = b.get<adm::SphericalSpeakerPosition>();
                            const auto* sp = &spherical;
                            item.has_position = true;
                            item.azimuth = sp->get<adm::Azimuth>().get();
                            item.elevation = sp->get<adm::Elevation>().get();
                            item.distance = sp->has<adm::Distance>() ? sp->get<adm::Distance>().get() : 1.0;
                            const bool az_bounds = sp->has<adm::AzimuthMin>() || sp->has<adm::AzimuthMax>();
                            const bool el_bounds = sp->has<adm::ElevationMin>() || sp->has<adm::ElevationMax>();
                            const bool dist_bounds = sp->has<adm::DistanceMin>() || sp->has<adm::DistanceMax>();
                            if (az_bounds || el_bounds || dist_bounds) {
                                item.bounds = { sp->has<adm::AzimuthMin>() ? sp->get<adm::AzimuthMin>().get() : item.azimuth,
                                                sp->has<adm::AzimuthMax>() ? sp->get<adm::AzimuthMax>().get() : item.azimuth,
                                                sp->has<adm::ElevationMin>() ? sp->get<adm::ElevationMin>().get() : item.elevation,
                                                sp->has<adm::ElevationMax>() ? sp->get<adm::ElevationMax>().get() : item.elevation };
                                if (dist_bounds) {
                                    item.bounds.push_back(sp->has<adm::DistanceMin>() ? sp->get<adm::DistanceMin>().get() : item.distance);
                                    item.bounds.push_back(sp->has<adm::DistanceMax>() ? sp->get<adm::DistanceMax>().get() : item.distance);
                                }
                            }
                        }
                        else {
                            result.warnings.push_back("audioObject '" + name + "': cartesian DirectSpeakers position is not supported; labels only");
                        }
                    }
                    break;    // DirectSpeakers metadata is static: the first block defines the channel
                }
                if (cf->has<adm::Frequency>()) {
                    const auto f = cf->get<adm::Frequency>();
                    item.lfe = f.has<adm::LowPass>();
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
                    if (hp->has<adm::Normalization>()) {
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

/// Build an ADM document with one Objects audioObject per captured object:
/// a programme, a content, and for each object the pack, channel, stream,
/// track formats and track UID. `length` is the audio length in seconds
/// (the last block of each object lasts until then). The chna entries for
/// track i (0-based) are appended to `chna_ids`.
inline std::shared_ptr<adm::Document> build_document(const std::string& programme_name, const std::vector<captured_object>& objects,
                                                     double length, std::vector<bw64::AudioId>& chna_ids)
{
    auto doc = adm::Document::create();
    auto programme = adm::AudioProgramme::create(adm::AudioProgrammeName(programme_name));
    auto content = adm::AudioContent::create(adm::AudioContentName(programme_name));
    programme->addReference(content);
    doc->add(programme);

    std::vector<adm::SimpleObjectHolder> holders;
    for (const auto& object : objects) {
        auto holder = adm::createSimpleObject(object.name);
        content->addReference(holder.audioObject);
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
        holders.push_back(holder);
    }
    adm::reassignIds(doc);

    for (size_t i = 0; i < holders.size(); ++i) {
        const auto& h = holders[i];
        chna_ids.emplace_back(static_cast<uint16_t>(i + 1), adm::formatId(h.audioTrackUid->get<adm::AudioTrackUidId>()),
                              adm::formatId(h.audioTrackFormat->get<adm::AudioTrackFormatId>()),
                              adm::formatId(h.audioPackFormat->get<adm::AudioPackFormatId>()));
    }
    return doc;
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
