#pragma once

#include "zdomf_model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace nba97 {

struct ZdomfEulerAngles {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t z = 0;
};

// Binary-compatible with the 32-byte records loaded by FUN_80066C40 and
// FUN_80066C6C: nine 4.12 rotation values, two bytes of padding, then XYZ
// translation registers.
struct ZdomfTransform {
    std::array<std::array<std::int16_t, 3>, 3> rotation{};
    std::array<std::int32_t, 3> translation{};
};

struct ZdomfTransformSet {
    std::array<ZdomfEulerAngles, 20> angles{};
    std::array<ZdomfTransform, 20> parts{};
    std::size_t available_sets = 0;
};

// Direct FUN_80067100 matrix construction for callers outside the base-pose
// decoder (notably the recovered frontend camera path).
ZdomfTransform make_zdomf_rotation(
    const std::vector<std::uint8_t>& packed_trig,
    const ZdomfEulerAngles& angles);

ZdomfTransformSet decode_zdomf_base_transforms(
    const std::vector<std::uint8_t>& deflist,
    const std::vector<std::uint8_t>& packed_trig,
    std::size_t set_index = 0);
ZdomfTransformSet load_zdomf_base_transforms(
    const std::filesystem::path& deflist_path,
    const std::filesystem::path& packed_trig_path,
    std::size_t set_index = 0);

// Native equivalent of the MVMVA operation used by FUN_80067378, followed by
// the signed-16 writeback performed by FUN_80062F4C/FUN_800631B0.
ZdomfVec3 apply_zdomf_transform(const ZdomfTransform& transform,
                                const ZdomfVec3& value);

} // namespace nba97
