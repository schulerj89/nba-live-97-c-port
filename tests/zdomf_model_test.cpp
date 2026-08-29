#include "zdomf_model.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void put16(std::vector<std::uint8_t>& data, std::size_t at, std::uint16_t value) {
    if (at + 2 > data.size()) data.resize(at + 2);
    data[at] = static_cast<std::uint8_t>(value);
    data[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t>& data, std::size_t at, std::uint32_t value) {
    if (at + 4 > data.size()) data.resize(at + 4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        data[at + shift / 8] = static_cast<std::uint8_t>(value >> shift);
    }
}

void put_vertex(std::vector<std::uint8_t>& data, std::size_t at,
                std::int16_t x, std::int16_t y, std::int16_t z) {
    put16(data, at, static_cast<std::uint16_t>(x));
    put16(data, at + 2, static_cast<std::uint16_t>(y));
    put16(data, at + 4, static_cast<std::uint16_t>(z));
    put16(data, at + 6, 0x55AA);
}

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() {
    try {
        std::vector<std::uint8_t> data(0xCA4, 0);
        put32(data, 0xC6C, 2); // primary faces
        put32(data, 0xC70, 1); // secondary faces

        auto cursor = std::size_t{0xCA4} + 3 * 12;
        put32(data, cursor, 0); put32(data, cursor + 4, 0); put32(data, cursor + 8, 0);
        cursor += 12;
        cursor += 4;      // table B sentinel
        cursor += 2 * 12; // primary index table 0
        cursor += 4 * 4;  // secondary index table 0 + sentinel
        cursor += 2 * 12; // primary index table 1
        cursor += 2 * 12; // primary index table 2
        cursor += 1 * 12; // secondary index table 1
        cursor += 4 * 4;  // secondary index table 2 + sentinel
        const auto packet_a = cursor;
        cursor += 2 * 32;
        cursor += 2 * 32;
        const auto secondary_packet_a = cursor;
        cursor += 32;
        cursor += 32;
        cursor += 4;      // packet-bank sentinel
        const auto part_headers = cursor;
        put32(data, part_headers + 2 * 16, 2);
        put32(data, part_headers + 3 * 16, 1);
        put32(data, part_headers + 20 * 16 + 1 * 16, 1);
        cursor += 20 * 16 + 6 * 16 + 4;
        const auto primary_source = cursor;
        cursor += 3 * 64; // two source-record banks per triangle
        cursor += 1 * 64; // secondary source-record banks
        const auto secondary_source = cursor - 1 * 64;
        const auto transformed = cursor;
        data.resize(transformed + 3 * 24);

        put_vertex(data, transformed + 0, 10, 20, 30);   // part 2, tri 0, component 0
        put_vertex(data, transformed + 8, 11, 21, 31);
        put_vertex(data, transformed + 16, 12, 22, 32);
        put_vertex(data, transformed + 24, 40, 50, 60);  // part 2, tri 1
        put_vertex(data, transformed + 48, -7, -8, -9);  // part 3, tri 0
        put_vertex(data, secondary_source + 8, 70, 80, 90);
        put_vertex(data, secondary_source + 16, 71, 81, 91);
        put_vertex(data, secondary_source + 24, 72, 82, 92);

        // Source packet banks are contiguous within each part: part 2 has
        // A0/A1 then B0/B1; part 3 starts after those four records.
        for (const auto packet : std::array<std::size_t, 3>{{
                 primary_source, primary_source + 32, primary_source + 128}}) {
            put32(data, packet + 4, 0x24807F7E);
            put32(data, packet + 12, 0x4321140A);
            put32(data, packet + 20, 0x00BD281E);
            put32(data, packet + 28, 0x00003C32);
        }

        // Face 0 is wholly part 2. Face 1 deliberately crosses part 3 -> 2.
        put32(data, 0xCA4 + 0, 0x00000200);
        put32(data, 0xCA4 + 4, 0x00000201);
        put32(data, 0xCA4 + 8, 0x00000202);
        put32(data, 0xCA4 + 12, 0x00000300);
        put32(data, 0xCA4 + 16, 0x00010200);
        put32(data, 0xCA4 + 20, 0x00000201);
        put32(data, 0xCA4 + 24, 0x00000100);
        put32(data, 0xCA4 + 28, 0x00000101);
        put32(data, 0xCA4 + 32, 0x00000102);

        for (std::size_t face = 0; face < 2; ++face) {
            const auto packet = packet_a + face * 32;
            put32(data, packet + 4, 0x24807F7E);
            put32(data, packet + 12, 0x1234140A);
            put32(data, packet + 20, 0x003E281E);
            put32(data, packet + 28, 0x00003C32);
        }
        put32(data, secondary_packet_a + 4, 0x24807F7E);
        put32(data, secondary_packet_a + 12, 0x5678140A);
        put32(data, secondary_packet_a + 20, 0x003E281E);
        put32(data, secondary_packet_a + 28, 0x00003C32);

        const auto model = nba97::decode_zdomf_model(data);
        check(model.primary_faces.size() == 2, "primary face count");
        check(model.secondary_faces.size() == 1 &&
              model.secondary_faces[0].corners[0].part == 2 &&
              model.secondary_faces[0].corners[2].position.z == 32 &&
              model.secondary_faces[0].corners[2].projected_packet_word_offset ==
                  secondary_source + 24,
              "secondary six-group face decode");
        check(model.mixed_part_face_count == 1, "mixed-part face count");
        check(model.part_triangle_counts[2] == 2 && model.part_triangle_counts[3] == 1,
              "part triangle counts");
        check(model.part_triangles[2].size() == 2 &&
               model.part_triangles[2][1][0].x == 40,
               "part source-triangle preservation");
        check(model.part_faces[2].size() == 2 &&
              model.part_faces[3].size() == 1 &&
              model.part_faces[2][1].packet_offset == primary_source + 32 &&
              model.part_faces[3][0].packet_offset == primary_source + 128 &&
              model.part_faces[2][0].clut == 0x4321 &&
              model.part_faces[2][0].tpage == 0x00BD &&
              model.part_faces[2][0].uv[2][0] == 50,
              "part POLY_FT3 stream decode");
        check(model.layout.primary_packet_a_offset == packet_a, "derived packet offset");
        check(model.layout.transformed_vertex_offset == transformed, "derived vertex offset");
        check(model.layout.primary_source_offset == primary_source &&
              model.layout.secondary_packet_a_offset == secondary_packet_a &&
              model.layout.secondary_source_offset == secondary_source,
              "primary/secondary packet source offsets");
        const auto& mixed = model.primary_faces[1];
        check(mixed.corners[0].part == 3 && mixed.corners[1].part == 2 &&
              mixed.corners[2].part == 2, "per-corner part ownership");
        check(mixed.corners[0].position.x == -7 && mixed.corners[0].position.y == -8 &&
              mixed.corners[0].position.z == -9, "signed vertex decode");
        check(mixed.clut == 0x1234 && mixed.tpage == 0x003E, "FT3 metadata decode");
        check(mixed.modulation[0] == 0x7e && mixed.modulation[1] == 0x7f &&
              mixed.modulation[2] == 0x80, "FT3 modulation decode");
        check(mixed.uv[0][0] == 10 && mixed.uv[0][1] == 20 &&
              mixed.uv[2][0] == 50 && mixed.uv[2][1] == 60, "FT3 UV decode");
        std::cout << "ZDOMF MODEL: PASS - derived relocation layout, signed vertices, "
                     "primary/part FT3 metadata, and per-corner part ownership\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ZDOMF MODEL: FAIL - " << error.what() << '\n';
        return 1;
    }
}
