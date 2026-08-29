#include "zdomf_model.hpp"

#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace nba97 {
namespace {

constexpr std::size_t kPivotOffset = 0xBCC;
constexpr std::size_t kGeometryHeaderOffset = 0xC6C;
constexpr std::size_t kDescriptorOffset = 0xCA4;

void require_range(const std::vector<std::uint8_t>& data, std::size_t offset,
                   std::size_t length, const char* what) {
    if (offset > data.size() || length > data.size() - offset) {
        throw std::runtime_error(std::string("truncated ZDOMF ") + what);
    }
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& data, std::size_t offset) {
    require_range(data, offset, 2, "u16");
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::int16_t read_s16(const std::vector<std::uint8_t>& data, std::size_t offset) {
    return static_cast<std::int16_t>(read_u16(data, offset));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    require_range(data, offset, 4, "u32");
    return std::uint32_t(data[offset]) | (std::uint32_t(data[offset + 1]) << 8) |
           (std::uint32_t(data[offset + 2]) << 16) |
           (std::uint32_t(data[offset + 3]) << 24);
}

std::size_t checked_add(std::size_t value, std::size_t count,
                        std::size_t stride, const char* what) {
    if (count > (std::numeric_limits<std::size_t>::max() - value) / stride) {
        throw std::runtime_error(std::string("overflow in ZDOMF ") + what);
    }
    return value + count * stride;
}

ZdomfVec3 read_vec3(const std::vector<std::uint8_t>& data, std::size_t offset) {
    require_range(data, offset, 8, "vertex");
    return {read_s16(data, offset), read_s16(data, offset + 2),
            read_s16(data, offset + 4)};
}

} // namespace

ZdomfModel decode_zdomf_model(const std::vector<std::uint8_t>& data) {
    require_range(data, kPivotOffset, 20 * 8, "pivot table");
    require_range(data, kGeometryHeaderOffset, 8, "geometry header");

    ZdomfModel model{};
    for (std::size_t part = 0; part < model.pivots.size(); ++part) {
        model.pivots[part] = read_vec3(data, kPivotOffset + part * 8);
    }

    const auto primary_count = read_u32(data, kGeometryHeaderOffset);
    model.secondary_face_count = read_u32(data, kGeometryHeaderOffset + 4);
    model.layout.descriptor_offset = kDescriptorOffset;

    // Exact cursor sequence recovered from FUN_800687BC. These sections are
    // pointer/index tables followed by two mutable GPU packet banks.
    auto cursor = checked_add(kDescriptorOffset, primary_count, 12,
                              "primary descriptors");
    cursor = checked_add(cursor, model.secondary_face_count, 12,
                         "secondary descriptors");
    const auto table_count_a = read_u32(data, cursor);
    const auto table_count_b = read_u32(data, cursor + 4);
    cursor = checked_add(cursor, 3, 4, "post-descriptor header");
    cursor = checked_add(cursor, table_count_a, 4, "table A");
    cursor = checked_add(cursor, std::size_t(table_count_b) + 1, 4, "table B");
    cursor = checked_add(cursor, primary_count, 12, "primary index table 0");
    cursor = checked_add(cursor, std::size_t(model.secondary_face_count) * 3 + 1,
                         4, "secondary index table 0");
    cursor = checked_add(cursor, primary_count, 12, "primary index table 1");
    cursor = checked_add(cursor, primary_count, 12, "primary index table 2");
    cursor = checked_add(cursor, model.secondary_face_count, 12,
                         "secondary index table 1");
    cursor = checked_add(cursor, std::size_t(model.secondary_face_count) * 3 + 1,
                         4, "secondary index table 2");

    model.layout.primary_packet_a_offset = cursor;
    cursor = checked_add(cursor, primary_count, 32, "primary packet bank A");
    model.layout.primary_packet_b_offset = cursor;
    cursor = checked_add(cursor, primary_count, 32, "primary packet bank B");
    cursor = checked_add(cursor, model.secondary_face_count, 32,
                         "secondary packet bank A");
    cursor = checked_add(cursor, model.secondary_face_count, 32,
                         "secondary packet bank B");
    cursor = checked_add(cursor, 1, 4, "packet-bank sentinel");

    model.layout.part_header_offset = cursor;
    require_range(data, cursor, 20 * 16 + 6 * 16 + 4, "part headers");
    std::uint64_t primary_triangles = 0;
    for (std::size_t part = 0; part < model.part_triangle_counts.size(); ++part) {
        const auto count = read_u32(data, cursor + part * 16);
        model.part_triangle_counts[part] = count;
        primary_triangles += count;
    }
    const auto secondary_header = cursor + 20 * 16;
    std::uint64_t secondary_triangles = 0;
    for (std::size_t group = 0; group < 6; ++group) {
        secondary_triangles += read_u32(data, secondary_header + group * 16);
    }
    if (primary_triangles > std::numeric_limits<std::uint32_t>::max() ||
        secondary_triangles > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("unreasonable ZDOMF triangle count");
    }
    model.secondary_triangle_count = static_cast<std::uint32_t>(secondary_triangles);

    // FUN_800687BC reserves two 32-byte records per source triangle, then
    // points each part's mutable signed-XYZ output at the following buffer.
    cursor = checked_add(secondary_header, 6, 16, "secondary part headers");
    cursor = checked_add(cursor, 1, 4, "part-header sentinel");
    cursor = checked_add(cursor, static_cast<std::size_t>(primary_triangles), 64,
                         "primary source records");
    cursor = checked_add(cursor, static_cast<std::size_t>(secondary_triangles), 64,
                         "secondary source records");
    model.layout.transformed_vertex_offset = cursor;
    model.layout.transformed_vertex_end = checked_add(
        cursor, static_cast<std::size_t>(primary_triangles), 24,
        "transformed vertex buffer");
    require_range(data, model.layout.transformed_vertex_offset,
                  model.layout.transformed_vertex_end - model.layout.transformed_vertex_offset,
                  "transformed vertex buffer");

    std::array<std::size_t, 20> part_vertex_offsets{};
    auto vertex_cursor = model.layout.transformed_vertex_offset;
    for (std::size_t part = 0; part < part_vertex_offsets.size(); ++part) {
        part_vertex_offsets[part] = vertex_cursor;
        vertex_cursor = checked_add(vertex_cursor, model.part_triangle_counts[part],
                                    24, "part vertex buffer");
    }
    if (vertex_cursor != model.layout.transformed_vertex_end) {
        throw std::runtime_error("inconsistent ZDOMF transformed vertex extent");
    }

    model.primary_faces.reserve(primary_count);
    for (std::uint32_t face_index = 0; face_index < primary_count; ++face_index) {
        ZdomfFace face{};
        for (std::size_t corner_index = 0; corner_index < 3; ++corner_index) {
            const auto descriptor_offset = kDescriptorOffset +
                (static_cast<std::size_t>(face_index) * 3 + corner_index) * 4;
            const auto descriptor = read_u32(data, descriptor_offset);
            auto& corner = face.corners[corner_index];
            corner.component = static_cast<std::uint8_t>(descriptor & 0xFF);
            corner.part = static_cast<std::uint8_t>((descriptor >> 8) & 0xFF);
            corner.triangle_index = static_cast<std::uint16_t>(descriptor >> 16);
            if (corner.part >= model.part_triangle_counts.size() ||
                corner.component >= 3 ||
                corner.triangle_index >= model.part_triangle_counts[corner.part]) {
                throw std::runtime_error("invalid ZDOMF primary face descriptor");
            }
            corner.source_offset = part_vertex_offsets[corner.part] +
                static_cast<std::size_t>(corner.triangle_index) * 24 +
                static_cast<std::size_t>(corner.component) * 8;
            corner.position = read_vec3(data, corner.source_offset);
        }
        if (face.corners[1].part != face.corners[0].part ||
            face.corners[2].part != face.corners[0].part) {
            ++model.mixed_part_face_count;
        }

        face.packet_offset = model.layout.primary_packet_a_offset +
            static_cast<std::size_t>(face_index) * 32;
        const auto command = read_u32(data, face.packet_offset + 4);
        if ((command >> 24) != 0x24) {
            throw std::runtime_error("unexpected ZDOMF primary GPU primitive");
        }
        for (std::size_t corner_index = 0; corner_index < 3; ++corner_index) {
            const auto packed = read_u32(data, face.packet_offset + 12 + corner_index * 8);
            face.uv[corner_index] = {{static_cast<std::uint8_t>(packed & 0xFF),
                                      static_cast<std::uint8_t>((packed >> 8) & 0xFF)}};
            if (corner_index == 0) face.clut = static_cast<std::uint16_t>(packed >> 16);
            if (corner_index == 1) face.tpage = static_cast<std::uint16_t>(packed >> 16);
        }
        model.primary_faces.push_back(face);
    }
    return model;
}

ZdomfModel load_zdomf_model(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing ZDOMF model: " + path.string());
    const std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(input), {}};
    return decode_zdomf_model(data);
}

} // namespace nba97
