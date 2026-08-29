#pragma once

#include "zdomf_hierarchy.hpp"
#include "zdomf_gte_compose.hpp"
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
    bool apply_frontend_view = false;
    // Live frontend camera angles passed to FUN_80066DA8 before the original
    // row-0 scale and FUN_80066FF4 composition. The startup value is 1500/0/0;
    // Create Player updates this context while the preview is active.
    ZdomfEulerAngles frontend_angles{1500, 0, 0};
    // FUN_80031F48's Create Player state installs DAT_800ED55C/55E/560 as
    // {0x1C0,0xC0,0x500}. This translation is applied after the frontend
    // rotation and carries the mocap root lift into screen-camera space.
    ZdomfWorldVec3 frontend_translation{};
    bool use_record_root_translation = false;
    ZdomfWorldVec3 record_root_translation{};
};

struct ZdomfRuntimePose {
    ZdomfHierarchy hierarchy{};
    std::array<ZdomfTransform, 20> part_matrices{};
    std::array<ZdomfWorldVec3, 20> part_origins{};
    std::array<ZdomfWorldVec3, 20> part_endpoints{};
    std::array<ZdomfTransform, 8> mirrored_matrices{};
    std::array<ZdomfWorldVec3, 8> mirrored_origins{};
    std::array<ZdomfWorldVec3, 8> mirrored_endpoints{};
    // FUN_80066090 publishes four chain-end offsets used by later attachment
    // and special-group stages: parts 3, 7, 15, and 19.
    std::array<ZdomfWorldVec3, 4> group_offsets{};
    ZdomfTransform root_transform{};
    ZdomfTransform frontend_view_transform{};
    // Staged original FUN_80062C40 -> FUN_80066FF4 -> FUN_80066090 path.
    // These remain separate from the active renderer while parent routing and
    // the remaining special-group boundaries are traced polygon-by-polygon.
    ZdomfTransform scaled_root_transform{};
    ZdomfTransform composed_root_transform{};
    std::array<ZdomfTransform, 20> composed_part_matrices{};
    // Literal FUN_80066090 record contract. Each part's vertices use its
    // composed matrix but inherit translation from the parent's +0x64 output
    // MATRIX; roots inherit the shared transformed root translation.
    ZdomfWorldVec3 record_root_translation{};
    std::array<ZdomfWorldVec3, 20> record_part_origins{};
    std::array<ZdomfWorldVec3, 20> record_part_endpoints{};
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

// Applies the exact per-record matrix/parent-translation boundary used by
// FUN_800631B8/FUN_800632D4 instead of recombining a host-space skeleton.
ZdomfWorldVec3 apply_zdomf_runtime_record_pose(
    const ZdomfRuntimePose& runtime, std::size_t part,
    const ZdomfVec3& vertex);

ZdomfWorldVec3 apply_zdomf_runtime_mirrored_pose(
    const ZdomfRuntimePose& runtime,
    std::size_t part,
    const ZdomfVec3& vertex);

} // namespace nba97
