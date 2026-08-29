#pragma once

#include <cstddef>
#include <cstdint>

namespace nba97 {

// Steady-state Create Player motion recovered from FUN_80039574,
// FUN_800355A0, and FUN_80034DC0. The origin is the synchronized retail
// capture used by the renderer audit: clip-1 tick 7, accumulator 0x80, and
// context yaw 808. It is an observed orbit phase, not a fixed camera value.
struct CreatePlayerMotionState {
    std::uint64_t presentations = 0;
    std::size_t logical_tick = 7;
    std::uint16_t accumulator = 0x80;
    std::uint16_t root_yaw = 808;
};

// FUN_80039574 waits for two NTSC vblanks before each frontend presentation.
// 60000/1001 vblanks per second therefore becomes 30000/1001 presentations.
constexpr std::uint64_t create_player_presentations(
    std::uint32_t elapsed_ms) noexcept {
    return (std::uint64_t(elapsed_ms) * 30u) / 1001u;
}

constexpr CreatePlayerMotionState create_player_full_body_motion_at(
    std::uint64_t presentations) noexcept {
    constexpr std::uint64_t playback_step = 0x300;
    constexpr std::uint64_t logical_tick_threshold = 0x28u << 4;
    constexpr std::size_t logical_tick_count = 36;
    constexpr std::size_t captured_tick = 7;
    constexpr std::uint64_t captured_accumulator = 0x80;
    constexpr std::uint16_t captured_yaw = 808;

    const auto phase = captured_accumulator + presentations * playback_step;
    return {
        presentations,
        (captured_tick + static_cast<std::size_t>(phase / logical_tick_threshold)) %
            logical_tick_count,
        static_cast<std::uint16_t>(phase % logical_tick_threshold),
        static_cast<std::uint16_t>((captured_yaw + presentations * 8u) & 0x3ffu)};
}

constexpr CreatePlayerMotionState create_player_full_body_motion(
    std::uint32_t elapsed_ms) noexcept {
    return create_player_full_body_motion_at(
        create_player_presentations(elapsed_ms));
}

} // namespace nba97
