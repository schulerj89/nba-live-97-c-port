#pragma once

#include "zdomf_hierarchy.hpp"
#include "zdomf_mocap.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace nba97 {

struct ZdomfRuntimeConfig {
    std::uint8_t height_value = 63;
    ZdomfWorldVec3 root_position{};
    std::int16_t root_yaw = 0;
    std::int16_t part11_angle = 0;
};

struct ZdomfRuntimePose {
    ZdomfHierarchy hierarchy{};
    ZdomfTransform root_transform{};
    std::int32_t scale_16_16 = 0;
    ZdomfWorldVec3 root_translation{};
};

// FUN_80062C00 stores height * 0x270 as a 16.16 scale value.
std::int32_t zdomf_height_scale(std::uint8_t height_value);

// Reproduces the runtime boundary around FUN_80066090/FUN_800696C4: twenty
// local Euler matrices, hierarchy composition, part-11 adjustment, root yaw,
// frame root lift, and the exact fixed-point height scale.
ZdomfRuntimePose build_zdomf_runtime_pose(
    const std::array<ZdomfVec3, 20>& pivots,
    const std::vector<std::uint8_t>& packed_trig,
    const ZdomfMocapPose& mocap,
    const ZdomfRuntimeConfig& config = {});

ZdomfWorldVec3 apply_zdomf_runtime_pose(const ZdomfRuntimePose& runtime,
                                        std::size_t part,
                                        const ZdomfVec3& vertex);

} // namespace nba97
