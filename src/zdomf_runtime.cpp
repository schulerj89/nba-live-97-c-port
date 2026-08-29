#include "zdomf_runtime.hpp"

#include <algorithm>
#include <cstdint>

namespace nba97 {
namespace {

std::int32_t shift12(std::int64_t value) {
    if (value >= 0) return static_cast<std::int32_t>(value / 4096);
    return static_cast<std::int32_t>(-(((-value) + 4095) / 4096));
}
std::int32_t shift16(std::int64_t value) {
    if (value >= 0) return static_cast<std::int32_t>(value / 65536);
    return static_cast<std::int32_t>(-(((-value) + 65535) / 65536));
}
ZdomfWorldVec3 rotate_world(const ZdomfTransform& transform,
                            const ZdomfWorldVec3& value) {
    const std::array<std::int32_t, 3> input{{value.x, value.y, value.z}};
    std::array<std::int32_t, 3> output{};
    for (std::size_t row = 0; row < 3; ++row) {
        std::int64_t sum = 0;
        for (std::size_t column = 0; column < 3; ++column) {
            sum += std::int64_t(transform.rotation[row][column]) * input[column];
        }
        output[row] = shift12(sum);
    }
    return {output[0], output[1], output[2]};
}

} // namespace

std::int32_t zdomf_height_scale(std::uint8_t height_value) {
    return std::int32_t(height_value) * 0x270;
}

ZdomfRuntimePose build_zdomf_runtime_pose(
    const std::array<ZdomfVec3, 20>& pivots,
    const std::vector<std::uint8_t>& packed_trig,
    const ZdomfMocapPose& mocap,
    const ZdomfRuntimeConfig& config) {
    std::array<ZdomfTransform, 20> local{};
    for (std::size_t part = 0; part < local.size(); ++part) {
        auto angles = mocap.joints[part].angles;
        if (part == 11) angles.x = static_cast<std::int16_t>(angles.x + config.part11_angle);
        local[part] = make_zdomf_rotation(packed_trig, angles);
    }
    ZdomfRuntimePose out{};
    out.hierarchy = build_zdomf_hierarchy(pivots, &local);
    out.scale_16_16 = zdomf_height_scale(config.height_value);
    out.root_transform = make_zdomf_rotation(
        packed_trig, {0, static_cast<std::int16_t>(config.root_yaw * 4), 0});
    // FUN_800696C4: ((signed frame-root >> 4) * scale >> 16) + root + 0x24.
    out.root_translation = config.root_position;
    out.root_translation.y += 0x24 + shift16(
        std::int64_t(std::int32_t(mocap.root_height) >> 4) * out.scale_16_16);
    return out;
}

ZdomfWorldVec3 apply_zdomf_runtime_pose(const ZdomfRuntimePose& runtime,
                                        std::size_t part,
                                        const ZdomfVec3& vertex) {
    auto world = apply_zdomf_hierarchy(runtime.hierarchy, part, vertex);
    world.x = shift16(std::int64_t(world.x) * runtime.scale_16_16);
    world.y = shift16(std::int64_t(world.y) * runtime.scale_16_16);
    world.z = shift16(std::int64_t(world.z) * runtime.scale_16_16);
    world = rotate_world(runtime.root_transform, world);
    world.x += runtime.root_translation.x;
    world.y += runtime.root_translation.y;
    world.z += runtime.root_translation.z;
    return world;
}

} // namespace nba97
