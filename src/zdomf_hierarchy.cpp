#include "zdomf_hierarchy.hpp"

#include <algorithm>
#include <stdexcept>

namespace nba97 {
namespace {

constexpr std::array<std::int8_t, 20> kParents{{
    -1, 0, 1, 2,
    -1, 4, 5, 6,
    -1, 8, 9, 10,
    9, 12, 13, 14,
    9, 16, 17, 18,
}};

std::int32_t shift12(std::int64_t value) {
    if (value >= 0) return static_cast<std::int32_t>(value / 4096);
    return static_cast<std::int32_t>(-(((-value) + 4095) / 4096));
}

ZdomfTransform identity() {
    ZdomfTransform out{};
    out.rotation[0][0] = 4096;
    out.rotation[1][1] = 4096;
    out.rotation[2][2] = 4096;
    return out;
}

ZdomfTransform compose(const ZdomfTransform& parent,
                       const ZdomfTransform& local) {
    ZdomfTransform out{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            std::int64_t sum = 0;
            for (std::size_t inner = 0; inner < 3; ++inner) {
                sum += std::int64_t(parent.rotation[row][inner]) *
                       local.rotation[inner][column];
            }
            out.rotation[row][column] = static_cast<std::int16_t>(shift12(sum));
        }
    }
    return out;
}

ZdomfWorldVec3 rotate(const ZdomfTransform& transform,
                      const ZdomfVec3& value) {
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

const std::array<std::int8_t, 20>& zdomf_parent_table() {
    return kParents;
}

ZdomfHierarchy build_zdomf_hierarchy(
    const std::array<ZdomfVec3, 20>& pivots,
    const std::array<ZdomfTransform, 20>* local_rotations) {
    ZdomfHierarchy hierarchy{};
    const auto unit = identity();
    for (std::size_t part = 0; part < hierarchy.parts.size(); ++part) {
        auto& node = hierarchy.parts[part];
        node.parent = kParents[part];
        node.local_pivot = pivots[part];
        const auto& local = local_rotations ? (*local_rotations)[part] : unit;
        if (node.parent < 0) {
            ++hierarchy.root_count;
            node.world_transform = local;
            node.depth = 0;
        } else {
            const auto parent = static_cast<std::size_t>(node.parent);
            if (parent >= part) throw std::runtime_error("invalid ZDOMF parent order");
            const auto& parent_node = hierarchy.parts[parent];
            node.depth = static_cast<std::uint8_t>(parent_node.depth + 1);
            node.joint_origin = parent_node.joint_end;
            node.world_transform = compose(parent_node.world_transform, local);
        }
        hierarchy.max_depth = std::max<std::size_t>(hierarchy.max_depth, node.depth);
        node.world_transform.translation = {
            node.joint_origin.x, node.joint_origin.y, node.joint_origin.z};
        const auto offset = rotate(node.world_transform, node.local_pivot);
        node.joint_end = {node.joint_origin.x + offset.x,
                          node.joint_origin.y + offset.y,
                          node.joint_origin.z + offset.z};
    }
    if (hierarchy.root_count != 3 || hierarchy.max_depth != 5) {
        throw std::runtime_error("unexpected ZDOMF hierarchy topology");
    }
    return hierarchy;
}

ZdomfWorldVec3 apply_zdomf_hierarchy(
    const ZdomfHierarchy& hierarchy,
    std::size_t part,
    const ZdomfVec3& local_vertex) {
    if (part >= hierarchy.parts.size()) {
        throw std::runtime_error("invalid ZDOMF hierarchy part");
    }
    const auto& node = hierarchy.parts[part];
    const auto rotated = rotate(node.world_transform, local_vertex);
    // FUN_800632D4/FUN_80069A08 load the part rotation but load translation
    // from the parent matrix pointer at part+0xA4. The part's own endpoint is
    // the origin inherited by its children, not its own vertex translation.
    return {rotated.x + node.joint_origin.x,
            rotated.y + node.joint_origin.y,
            rotated.z + node.joint_origin.z};
}

} // namespace nba97
