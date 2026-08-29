#include "create_player_motion.hpp"
#include "zdomf_mocap.hpp"
#include "zdomf_runtime.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void put16(std::vector<std::uint8_t>& out, std::size_t at, std::uint16_t value) {
    out[at] = static_cast<std::uint8_t>(value);
    out[at + 1] = static_cast<std::uint8_t>(value >> 8);
}
void put32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t value) {
    put16(out, at, static_cast<std::uint16_t>(value));
    put16(out, at + 2, static_cast<std::uint16_t>(value >> 16));
}
std::vector<std::uint8_t> synthetic_mocap() {
    constexpr std::size_t body_directory = 8;
    constexpr std::size_t secondary_directory = 32;
    constexpr std::size_t pair_bytes = 12 + 0x60 + 12 + 0x44;
    std::vector<std::uint8_t> out(56 + pair_bytes * 6);
    put32(out, 0, body_directory);
    put32(out, 4, secondary_directory);
    std::size_t cursor = 56;
    for (std::size_t clip = 0; clip < 6; ++clip) {
        const auto body = cursor;
        const auto secondary = body + 12 + 0x60;
        put32(out, body_directory + clip * 4, static_cast<std::uint32_t>(body));
        put32(out, secondary_directory + clip * 4, static_cast<std::uint32_t>(secondary));
        out[body + 7] = 1;
        out[secondary + 7] = 1;
        put32(out, body + 8, 12);
        put32(out, secondary + 8, 12);
        put16(out, secondary + 12, static_cast<std::uint16_t>(clip));
        put16(out, secondary + 14, static_cast<std::uint16_t>(100 + clip));
        for (std::size_t part = 0; part < 8; ++part) {
            const auto at = secondary + 16 + part * 8;
            put16(out, at, static_cast<std::uint16_t>(part + clip));
            put16(out, at + 2, static_cast<std::uint16_t>(part + 10));
            put16(out, at + 4, static_cast<std::uint16_t>(part + 20));
            put16(out, at + 6, 0xabcd);
        }
        for (std::size_t part = 0; part < 12; ++part) {
            const auto at = body + 12 + part * 8;
            put16(out, at, static_cast<std::uint16_t>(part + 30));
            put16(out, at + 2, static_cast<std::uint16_t>(part + 40));
            put16(out, at + 4, static_cast<std::uint16_t>(part + 50));
            put16(out, at + 6, 0xdcba);
        }
        cursor += pair_bytes;
    }
    return out;
}
}

int main() {
    try {
        const auto mocap = nba97::decode_zdomf_mocap(synthetic_mocap());
        check(mocap.clips[5].physical_frames == 1 &&
              mocap.clips[5].frames[0].root_height == 105,
              "paired directory/frame decode");
        check(mocap.clips[0].frames[0].joints[7].angles.z == 27 &&
              mocap.clips[0].frames[0].joints[8].angles.x == 30 &&
              mocap.clips[0].frames[0].joints[19].angles.z == 61,
              "8+12 joint assembly");
        const auto captured = nba97::create_player_full_body_motion_at(0);
        const auto one_presentation = nba97::create_player_full_body_motion_at(1);
        const auto two_presentations = nba97::create_player_full_body_motion_at(2);
        const auto accumulator_skip = nba97::create_player_full_body_motion_at(4);
        const auto jog_loop = nba97::create_player_full_body_motion_at(30);
        const auto orbit_loop = nba97::create_player_full_body_motion_at(128);
        check(captured.logical_tick == 7 && captured.accumulator == 0x80 &&
              captured.root_yaw == 808,
              "Create Player synchronized motion anchor");
        check(one_presentation.logical_tick == 8 &&
              one_presentation.accumulator == 0x100 &&
              one_presentation.root_yaw == 816,
              "Create Player first retail presentation advance");
        check(two_presentations.logical_tick == 9 &&
              two_presentations.accumulator == 0x180 &&
              two_presentations.root_yaw == 824,
              "Create Player two-presentation no$psx motion signature");
        check(accumulator_skip.logical_tick == 12 &&
              accumulator_skip.accumulator == 0 &&
              accumulator_skip.root_yaw == 840,
              "Create Player 0x300/0x280 fractional cadence");
        check(jog_loop.logical_tick == 7 && jog_loop.accumulator == 0x80,
              "Create Player 30-presentation jog loop");
        check(orbit_loop.root_yaw == 808,
              "Create Player 128-presentation 360 orbit");
        check(nba97::create_player_presentations(1000) == 29 &&
              nba97::create_player_presentations(1001) == 30,
              "Create Player two-vblank NTSC presentation clock");
        const nba97::ZdomfEulerAngles a{0x7f0, 0, 0};
        const nba97::ZdomfEulerAngles b{-0x7f0, 0, 0};
        const auto halfway = nba97::blend_zdomf_euler(a, b, 0x80);
        check(halfway.x == 0x800, "wrapped half-turn interpolation");
        check(nba97::zdomf_height_scale(63) == 63 * 0x270 &&
              nba97::zdomf_height_scale(90) == 90 * 0x270,
              "FUN_80062C00 height scale");
        std::vector<std::uint8_t> trig(4096 * 4);
        put32(trig, 0, 0x10000000);
        std::array<nba97::ZdomfVec3, 20> pivots{};
        for (auto& pivot : pivots) pivot = {10, 0, 0};
        nba97::ZdomfMocapPose still{};
        still.root_height = 160;
        nba97::ZdomfRuntimeConfig isolated_runtime_config{64, {1, 2, 3}, 0, 0};
        isolated_runtime_config.apply_frontend_view = false;
        const auto runtime = nba97::build_zdomf_runtime_pose(
            pivots, trig, still, isolated_runtime_config);
        const auto placed = nba97::apply_zdomf_runtime_pose(runtime, 0, {0, 0, 0});
        check(runtime.scale_16_16 == 64 * 0x270 &&
              runtime.root_translation.y == 44 &&
              placed.x == 1 && placed.y == 44 && placed.z == 3 &&
              runtime.part_origins[0].x == 0 &&
              runtime.part_endpoints[0].x == 10 &&
              runtime.part_origins[1].x == 10 &&
              runtime.group_offsets[0].x == 40 &&
              runtime.group_offsets[2].x == 60,
              "FUN_80066090 parent origins/groups and FUN_800696C4 root lift");
        check(runtime.mirrored_matrices[0].rotation[0][0] == 4096 &&
              runtime.mirrored_matrices[0].rotation[1][1] == -4096 &&
              runtime.mirrored_matrices[0].rotation[2][2] == -4096 &&
              runtime.mirrored_origins[1].x == 10,
              "FUN_80066090 second matrix and mirrored parent chain");
        auto translated_config = isolated_runtime_config;
        translated_config.frontend_translation = {448, 192, 1280};
        const auto translated_runtime = nba97::build_zdomf_runtime_pose(
            pivots, trig, still, translated_config);
        const auto translated_placed = nba97::apply_zdomf_runtime_pose(
            translated_runtime, 0, {0, 0, 0});
        check(translated_runtime.record_root_translation.x == 449 &&
              translated_runtime.record_root_translation.y == 236 &&
              translated_runtime.record_root_translation.z == 1283 &&
              translated_placed.x == 449 && translated_placed.y == 236 &&
              translated_placed.z == 1283,
              "Create Player frontend translation preserves dynamic root lift");
        check(runtime.scaled_root_transform.rotation[0][0] == 2496 &&
              runtime.scaled_root_transform.rotation[1][1] == 2496 &&
              runtime.scaled_root_transform.rotation[2][2] == 2496 &&
              runtime.composed_root_transform.rotation[0][0] == 2496 &&
              runtime.composed_part_matrices[0].rotation[0][0] == 2496,
              "staged FUN_80062C40/80066FF4/80066090 matrix chain");
        std::cout << "ZDOMF MOCAP: PASS - two six-entry directories, 8+12 joint frames, "
                     "wrapped blending, height scale, translated root lift, and "
                     "29.97 Hz Create Player hop-jog/orbit cadence\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ZDOMF MOCAP: FAIL - " << error.what() << '\n';
        return 1;
    }
}
