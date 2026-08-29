#include "zdomf_projection.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void put32(std::vector<std::uint8_t>& data, std::size_t at, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        data[at + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        nba97::ZdomfProjectionConfig identity{};
        identity.projection_distance = 500;
        identity.camera.rotation = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}};
        identity.camera.translation = {0, 0, 1000};
        const auto projected = nba97::project_zdomf_vertex(identity, {100, 50, 0});
        check(projected.x == 434 && projected.y == 145 && projected.depth == 1000,
              "identity RTPS projection");
        check(projected.flags == nba97::ZdomfProjectionNone,
              "identity RTPS flags");

        check(nba97::zdomf_gte_unr_divide(160, 611) == 17162,
              "PS1 GTE UNR reciprocal fixture");

        identity.camera.translation[2] = 0;
        const auto overflow = nba97::project_zdomf_vertex(identity, {2000, 0, 0});
        check((overflow.flags & nba97::ZdomfProjectionDivideOverflow) != 0,
              "RTPS divide overflow");
        check((overflow.flags & nba97::ZdomfProjectionScreenSaturated) != 0,
              "RTPS screen saturation");

        std::vector<std::uint8_t> trig(4096 * 4, 0);
        put32(trig, 0, 0x10000000);
        const auto create = nba97::make_create_player_projection(trig);
        check(create.camera.translation[0] == 0 &&
              create.camera.translation[1] == -384 &&
              create.camera.translation[2] == 3328,
              "recovered Create Player camera translation");
        check(create.geom_offset_x == 256 && create.geom_offset_y == 120 &&
              create.draw_offset_x == 128 && create.projection_distance == 160,
              "Create Player projection registers and native viewport");

        std::cout << "ZDOMF PROJECTION: PASS - FUN_8006734C RTPS boundary and "
                     "Create Player camera state\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ZDOMF PROJECTION: FAIL - " << error.what() << '\n';
        return 1;
    }
}
