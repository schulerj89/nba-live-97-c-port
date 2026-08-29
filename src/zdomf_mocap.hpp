#pragma once

#include "zdomf_transform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace nba97 {

struct ZdomfMocapJoint {
    ZdomfEulerAngles angles{};
    std::uint16_t marker = 0;
};

struct ZdomfMocapPose {
    // FUN_80069D88 supplies parts 0..7 from the 0x44-byte stream and
    // parts 8..19 from the 0x60-byte stream.
    std::array<ZdomfMocapJoint, 20> joints{};
    std::int16_t root_word = 0;
    std::int16_t root_height = 0;
};

struct ZdomfMocapClip {
    std::uint16_t body_flags = 0;
    std::uint16_t secondary_flags = 0;
    std::uint8_t timing_code = 0;
    std::size_t physical_frames = 0;
    std::size_t logical_ticks = 0;
    std::vector<ZdomfMocapPose> frames;
};

struct ZdomfMocap {
    std::array<ZdomfMocapClip, 6> clips{};
    std::size_t body_directory_offset = 0;
    std::size_t secondary_directory_offset = 0;
};

// Native interpretation of the relocation performed by FUN_80035260.
// The first directory contains twelve-joint 0x60-byte frames; the second
// contains an additional four-byte root header plus eight 8-byte joints.
ZdomfMocap decode_zdomf_mocap(const std::vector<std::uint8_t>& data);
ZdomfMocap load_zdomf_mocap(const std::filesystem::path& path);

// FUN_80065D40 chooses an equivalent wrapped Euler representation before
// interpolating with an unsigned 8.8 weight (0 = a, 256 = b).
ZdomfEulerAngles blend_zdomf_euler(const ZdomfEulerAngles& a,
                                   const ZdomfEulerAngles& b,
                                   std::uint16_t weight);
ZdomfMocapPose blend_zdomf_pose(const ZdomfMocapPose& a,
                                const ZdomfMocapPose& b,
                                std::uint16_t weight);

// Converts a logical playback tick into the physical-keyframe interpolation
// implied by FUN_80035260's flag 0x08 timing expansion.
ZdomfMocapPose sample_zdomf_mocap(const ZdomfMocap& mocap,
                                  std::size_t clip,
                                  std::size_t logical_tick);

// FUN_80065CF4 conversion used by FUN_80069D88 flags 1/2. The fixed retail
// maps reorder each stream into runtime part order and flip X/Z half-turns.
ZdomfMocapPose canonicalize_zdomf_pose(const ZdomfMocapPose& pose,
                                       bool body,
                                       bool secondary);

} // namespace nba97
