#pragma once

#include "zdomf_model.hpp"
#include "zdomf_transform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace nba97 {

struct ZdomfWorldVec3 {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

struct ZdomfHierarchyNode {
    std::int8_t parent = -1;
    std::uint8_t depth = 0;
    ZdomfVec3 local_pivot{};
    ZdomfWorldVec3 joint_origin{};
    ZdomfWorldVec3 joint_end{};
    ZdomfTransform world_transform{};
};

struct ZdomfHierarchy {
    std::array<ZdomfHierarchyNode, 20> parts{};
    std::size_t root_count = 0;
    std::size_t max_depth = 0;
};

// Parent links reconstructed from FUN_80069098's 148-byte runtime-record
// pointer assignments. Three chains begin at parts 0, 4, and 8.
const std::array<std::int8_t, 20>& zdomf_parent_table();

// Builds the static world-pose boundary. local_rotations is deliberately
// explicit so the later ZFEMOCAP decoder can feed this library without
// changing hierarchy or projection code.
ZdomfHierarchy build_zdomf_hierarchy(
    const std::array<ZdomfVec3, 20>& pivots,
    const std::array<ZdomfTransform, 20>* local_rotations = nullptr);

ZdomfWorldVec3 apply_zdomf_hierarchy(
    const ZdomfHierarchy& hierarchy,
    std::size_t part,
    const ZdomfVec3& local_vertex);

} // namespace nba97
