#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace nba97 {

struct ZdomfVec3 {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t z = 0;
};

struct ZdomfCornerRef {
    ZdomfVec3 position{};
    std::size_t source_offset = 0;
    std::uint16_t triangle_index = 0;
    std::uint8_t component = 0;
    std::uint8_t part = 0;
};

struct ZdomfFace {
    std::array<ZdomfCornerRef, 3> corners{};
    std::array<std::array<std::uint8_t, 2>, 3> uv{};
    std::size_t packet_offset = 0;
    std::uint16_t clut = 0;
    std::uint16_t tpage = 0;
};

struct ZdomfLayout {
    std::size_t descriptor_offset = 0;
    std::size_t primary_packet_a_offset = 0;
    std::size_t primary_packet_b_offset = 0;
    std::size_t secondary_packet_a_offset = 0;
    std::size_t secondary_packet_b_offset = 0;
    std::size_t part_header_offset = 0;
    std::size_t transformed_vertex_offset = 0;
    std::size_t transformed_vertex_end = 0;
    std::size_t secondary_source_offset = 0;
    std::size_t secondary_source_end = 0;
};

struct ZdomfModel {
    std::array<ZdomfVec3, 20> pivots{};
    std::array<std::uint32_t, 20> part_triangle_counts{};
    std::vector<ZdomfFace> primary_faces;
    std::vector<ZdomfFace> secondary_faces;
    ZdomfLayout layout{};
    std::uint32_t secondary_face_count = 0;
    std::uint32_t secondary_triangle_count = 0;
    std::size_t mixed_part_face_count = 0;
};

// Reproduces FUN_800687BC's offset arithmetic as bounded native references.
// No emulated addresses or retail data are retained by the returned model.
ZdomfModel decode_zdomf_model(const std::vector<std::uint8_t>& data);
ZdomfModel load_zdomf_model(const std::filesystem::path& path);

} // namespace nba97
