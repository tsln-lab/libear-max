/// @file
/// @brief   The dbmd chunk of a Dolby Atmos master: Dolby's binary metadata
///          next to the ADM, which tools such as WaveLab's Dolby Atmos import
///          require before they take a file as a Dolby Atmos master.
///
/// There is no public description of the chunk; its layout is read off
/// masters written by Dolby's tools (the Dolby Atmos Conversion Tool, the
/// Dolby Atmos Renderer, the Home Theater RMU) and by Logic Pro:
///
/// - a version, 06 00 00 01;
/// - segments, each an id byte, a 16-bit little-endian payload size, the
///   payload and a checksum byte: the two's complement of the sum of the
///   size and payload bytes;
/// - two zero bytes.
///
/// Every reference file has a segment 7 of 96 bytes, identical in all of
/// them, and a Dolby Atmos segment 9 of 248 bytes: three 32-byte strings
/// (the first "Created using Dolby equipment", the second the writing
/// tool's name, the third empty), then 152 bytes of settings whose meaning
/// is unknown. The settings are copied from a Dolby Home Theater RMU master
/// that WaveLab accepted. Some files also carry a segment 10 with one entry
/// per track; it is optional (that RMU master has none) and not written.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#pragma once

#include "bw64/bw64.hpp"

#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

namespace earmax::admio {

/// The tool name written in the Dolby Atmos segment.
constexpr const char* k_dbmd_tool_name = "libear-max";

/// The first string of the Dolby Atmos segment.
constexpr const char* k_dbmd_first_string = "";

namespace detail {

/// Append a segment: id, size, payload and checksum.
inline void append_dbmd_segment(std::vector<uint8_t>& out, uint8_t id, const std::vector<uint8_t>& payload)
{
    const auto size = static_cast<uint16_t>(payload.size());
    const uint8_t size_bytes[2] = { static_cast<uint8_t>(size & 0xff), static_cast<uint8_t>(size >> 8) };
    unsigned sum = size_bytes[0] + size_bytes[1];
    for (uint8_t b : payload) {
        sum += b;
    }
    out.push_back(id);
    out.push_back(size_bytes[0]);
    out.push_back(size_bytes[1]);
    out.insert(out.end(), payload.begin(), payload.end());
    out.push_back(static_cast<uint8_t>((0x100u - (sum & 0xffu)) & 0xffu));
}

/// A string in a 32-byte field: at most 31 characters, zero-filled.
inline void append_dbmd_string(std::vector<uint8_t>& out, const std::string& text)
{
    const size_t n = std::min<size_t>(text.size(), 31);
    out.insert(out.end(), text.begin(), text.begin() + static_cast<long>(n));
    out.insert(out.end(), 32 - n, 0);
}

} // namespace detail

/// The body of the dbmd chunk (see the file comment).
inline std::vector<uint8_t> dolby_dbmd(const std::string& tool_name = k_dbmd_tool_name,
                                       const std::string& first_string = k_dbmd_first_string)
{
    std::vector<uint8_t> out = { 0x06, 0x00, 0x00, 0x01 };

    // segment 7: the same 96 bytes in every reference file
    std::vector<uint8_t> segment7(96, 0);
    const uint8_t segment7_head[] = { 0x00, 0x47, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02 };
    std::memcpy(segment7.data(), segment7_head, sizeof(segment7_head));
    detail::append_dbmd_segment(out, 7, segment7);

    // segment 9, Dolby Atmos: three strings, then the settings
    std::vector<uint8_t> segment9;
    detail::append_dbmd_string(segment9, first_string);
    detail::append_dbmd_string(segment9, tool_name);
    detail::append_dbmd_string(segment9, "");
    std::vector<uint8_t> settings(152, 0);
    const std::pair<size_t, uint8_t> set_bytes[] = {
        { 3, 0x03 },  { 7, 0x03 },  { 9, 0x05 },  { 15, 0x22 }, { 22, 0x03 }, { 38, 0xf0 }, { 39, 0x08 },
        { 43, 0x03 }, { 56, 0x84 }, { 72, 0xf0 }, { 74, 0x08 }, { 88, 0xf0 }, { 90, 0x08 }, { 104, 0xf0 },
        { 106, 0x08 },
    };
    for (const auto& b : set_bytes) {
        settings[b.first] = b.second;
    }
    segment9.insert(segment9.end(), settings.begin(), settings.end());
    detail::append_dbmd_segment(out, 9, segment9);

    out.push_back(0);
    out.push_back(0);
    return out;
}

/// The dbmd chunk for libbw64, written after the data with
/// Bw64Writer::setAxmlChunk() (which takes any chunk).
class dbmd_chunk : public bw64::Chunk {
public:
    explicit dbmd_chunk(std::vector<uint8_t> body) : m_body(std::move(body)) {}

    uint32_t id() const override
    {
        return bw64::utils::fourCC("dbmd");
    }

    uint64_t size() const override
    {
        return m_body.size();
    }

    void write(std::ostream& stream) const override
    {
        stream.write(reinterpret_cast<const char*>(m_body.data()), static_cast<std::streamsize>(m_body.size()));
    }

private:
    std::vector<uint8_t> m_body;
};

} // namespace earmax::admio
