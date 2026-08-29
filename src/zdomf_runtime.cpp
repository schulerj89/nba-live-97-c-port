#include "zdomf_runtime.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

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

ZdomfTransform mirrored_rotation(ZdomfTransform rotation) {
    // The i<8 branch in FUN_80066090 submits a second matrix with the three
    // sign changes visible in its GTE column vectors. This is intentionally
    // not replaced by a generic host reflection matrix.
    rotation.rotation[1][0] = static_cast<std::int16_t>(-rotation.rotation[1][0]);
    rotation.rotation[1][1] = static_cast<std::int16_t>(-rotation.rotation[1][1]);
    rotation.rotation[2][2] = static_cast<std::int16_t>(-rotation.rotation[2][2]);
    return rotation;
}

ZdomfWorldVec3 add(ZdomfWorldVec3 a, ZdomfWorldVec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

ZdomfTransform frontend_view_transform(
    const std::vector<std::uint8_t>& packed_trig, bool enabled) {
    if (!enabled) return make_zdomf_rotation(packed_trig, {0, 0, 0});
    // FUN_8006A1E8 initializes {0x5DC,0,0}. FUN_8006A3C4 passes those
    // angles to FUN_80066DA8, then applies signed (value << 4) / 10 to the
    // first matrix row at DAT_800ED278 before FUN_800696C4 composes it.
    auto view = make_zdomf_rotation(packed_trig, {0x5dc, 0, 0});
    for (auto& value : view.rotation[0]) {
        value = static_cast<std::int16_t>((static_cast<std::int32_t>(value) * 16) / 10);
    }
    return view;
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
    // FUN_80066090 treats mocap angles as already-global part rotations. It
    // chains translated pivots through the parent pointers installed by
    // FUN_80069098, but does not recursively compose parent rotations.
    out.hierarchy = build_zdomf_hierarchy(pivots);
    const auto& parents = zdomf_parent_table();
    for (std::size_t part = 0; part < local.size(); ++part) {
        const auto parent = parents[part];
        const auto origin = parent < 0 ? ZdomfWorldVec3{} :
            out.part_endpoints[static_cast<std::size_t>(parent)];
        const auto endpoint = add(origin, rotate_world(local[part],
            {pivots[part].x, pivots[part].y, pivots[part].z}));
        out.part_matrices[part] = local[part];
        out.part_matrices[part].translation = {origin.x, origin.y, origin.z};
        out.part_origins[part] = origin;
        out.part_endpoints[part] = endpoint;
        auto& node = out.hierarchy.parts[part];
        node.world_transform = local[part];
        node.world_transform.translation = {origin.x, origin.y, origin.z};
        node.joint_origin = origin;
        node.joint_end = endpoint;
    }
    // Parts 0..7 have the second matrix/parent chain consumed by the alternate
    // model pass in FUN_800632D4. Its parent graph is two four-part roots.
    for (std::size_t part = 0; part < out.mirrored_matrices.size(); ++part) {
        const auto chain_root = part == 0 || part == 4;
        const auto origin = chain_root ? ZdomfWorldVec3{} : out.mirrored_endpoints[part - 1];
        const auto mirrored = mirrored_rotation(local[part]);
        const auto endpoint = add(origin, rotate_world(mirrored,
            {pivots[part].x, pivots[part].y, pivots[part].z}));
        out.mirrored_matrices[part] = mirrored;
        out.mirrored_matrices[part].translation = {origin.x, origin.y, origin.z};
        out.mirrored_origins[part] = origin;
        out.mirrored_endpoints[part] = endpoint;
    }
    constexpr std::array<std::size_t, 4> group_parts{{3, 7, 15, 19}};
    for (std::size_t group = 0; group < group_parts.size(); ++group)
        out.group_offsets[group] = out.part_endpoints[group_parts[group]];
    out.scale_16_16 = zdomf_height_scale(config.height_value);
    out.root_transform = make_zdomf_rotation(
        packed_trig, {0, static_cast<std::int16_t>(config.root_yaw * 4), 0});
    out.frontend_view_transform = frontend_view_transform(
        packed_trig, config.apply_frontend_view);
    // FUN_800696C4: ((signed frame-root >> 4) * scale >> 16) + root + 0x24.
    out.root_translation = config.root_position;
    out.root_translation.y += 0x24 + shift16(
        std::int64_t(std::int32_t(mocap.root_height) >> 4) * out.scale_16_16);
    return out;
}

ZdomfWorldVec3 apply_zdomf_runtime_pose(const ZdomfRuntimePose& runtime,
                                        std::size_t part,
                                        const ZdomfVec3& vertex) {
    if (part >= runtime.part_matrices.size())
        throw std::runtime_error("invalid ZDOMF runtime part");
    auto world = add(rotate_world(runtime.part_matrices[part],
                                  {vertex.x, vertex.y, vertex.z}),
                     runtime.part_origins[part]);
    world.x = shift16(std::int64_t(world.x) * runtime.scale_16_16);
    world.y = shift16(std::int64_t(world.y) * runtime.scale_16_16);
    world.z = shift16(std::int64_t(world.z) * runtime.scale_16_16);
    world = rotate_world(runtime.root_transform, world);
    world.x += runtime.root_translation.x;
    world.y += runtime.root_translation.y;
    world.z += runtime.root_translation.z;
    world = rotate_world(runtime.frontend_view_transform, world);
    return world;
}

ZdomfWorldVec3 apply_zdomf_runtime_mirrored_pose(
    const ZdomfRuntimePose& runtime,
    std::size_t part,
    const ZdomfVec3& vertex) {
    if (part >= runtime.mirrored_matrices.size())
        throw std::runtime_error("invalid ZDOMF mirrored runtime part");
    auto world = add(rotate_world(runtime.mirrored_matrices[part],
                                  {vertex.x, vertex.y, vertex.z}),
                     runtime.mirrored_origins[part]);
    world.x = shift16(std::int64_t(world.x) * runtime.scale_16_16);
    world.y = shift16(std::int64_t(world.y) * runtime.scale_16_16);
    world.z = shift16(std::int64_t(world.z) * runtime.scale_16_16);
    world = rotate_world(runtime.root_transform, world);
    world.x += runtime.root_translation.x;
    world.y += runtime.root_translation.y;
    world.z += runtime.root_translation.z;
    world = rotate_world(runtime.frontend_view_transform, world);
    return world;
}

} // namespace nba97
