#pragma once

#include "zdomf_transform.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace nba97 {

enum ZdomfProjectionFlag : std::uint32_t {
    ZdomfProjectionNone = 0,
    ZdomfProjectionDivideOverflow = 1u << 0,
    ZdomfProjectionScreenSaturated = 1u << 1,
    ZdomfProjectionDepthSaturated = 1u << 2,
};

struct ZdomfProjectionConfig {
    ZdomfTransform camera{};
    std::int32_t geom_offset_x = 256;
    std::int32_t geom_offset_y = 120;
    // Native viewport composition offset. Unlike the GTE registers above,
    // this is applied by the port after projection.
    std::int32_t draw_offset_x = 128;
    std::int32_t draw_offset_y = 0;
    std::uint16_t projection_distance = 160;
};

struct ZdomfProjectedVertex {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::uint16_t depth = 0;
    std::uint32_t flags = ZdomfProjectionNone;
};

// Recovered static Create Player camera initialized by FUN_8006A1E8 and
// loaded into the GTE by FUN_8006A55C/FUN_8006A3C4. The port's 128-pixel
// viewport placement is kept separate from those recovered GTE registers.
ZdomfProjectionConfig make_create_player_projection(
    const std::vector<std::uint8_t>& packed_trig);
ZdomfProjectionConfig load_create_player_projection(
    const std::filesystem::path& packed_trig_path);

// Native RTPS-compatible projection boundary used by FUN_8006734C and by
// the RTPT triangle helpers at FUN_80065330/FUN_80065388.
ZdomfProjectedVertex project_zdomf_vertex(
    const ZdomfProjectionConfig& config,
    const ZdomfVec3& vertex);

} // namespace nba97
