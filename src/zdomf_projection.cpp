#include "zdomf_projection.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace nba97 {
namespace {

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing private projection asset: " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}

std::int32_t shift12(std::int64_t value) {
    if (value >= 0) return static_cast<std::int32_t>(value / 4096);
    return static_cast<std::int32_t>(-(((-value) + 4095) / 4096));
}

std::int32_t clamp_screen(std::int64_t value, std::uint32_t& flags) {
    if (value < -1024) {
        flags |= ZdomfProjectionScreenSaturated;
        return -1024;
    }
    if (value > 1023) {
        flags |= ZdomfProjectionScreenSaturated;
        return 1023;
    }
    return static_cast<std::int32_t>(value);
}

} // namespace

ZdomfProjectionConfig make_create_player_projection(
    const std::vector<std::uint8_t>& packed_trig) {
    ZdomfProjectionConfig config{};
    // FUN_8006A1E8 initializes the view rotation to {1500,0,0} and the
    // translation registers to {0,-384,3328}. FUN_80035950 establishes the
    // frontend geometry offset {256,120}. FEONLY static 0x800C6508 supplies
    // H=160 to FUN_80066D9C. The final 128 pixels are the native port's
    // explicit viewport composition, not a claimed GTE register value.
    config.camera = make_zdomf_rotation(packed_trig, {1500, 0, 0});
    config.camera.translation = {0, -384, 3328};
    config.projection_distance = 160;
    return config;
}

ZdomfProjectionConfig load_create_player_projection(
    const std::filesystem::path& packed_trig_path) {
    return make_create_player_projection(read_file(packed_trig_path));
}

ZdomfProjectedVertex project_zdomf_vertex(
    const ZdomfProjectionConfig& config,
    const ZdomfVec3& vertex) {
    std::array<std::int32_t, 3> camera{};
    const std::array<std::int32_t, 3> input{{vertex.x, vertex.y, vertex.z}};
    for (std::size_t row = 0; row < 3; ++row) {
        std::int64_t accumulator =
            std::int64_t(config.camera.translation[row]) * 4096;
        for (std::size_t column = 0; column < 3; ++column) {
            accumulator += std::int64_t(config.camera.rotation[row][column]) *
                           input[column];
        }
        camera[row] = shift12(accumulator);
    }

    ZdomfProjectedVertex out{};
    if (camera[2] < 0 || camera[2] > 0xFFFF) {
        out.flags |= ZdomfProjectionDepthSaturated;
    }
    out.depth = static_cast<std::uint16_t>(std::clamp(camera[2], 0, 0xFFFF));

    std::uint32_t quotient = 0x1FFFF;
    if (out.depth != 0 && std::uint32_t(out.depth) * 2u > config.projection_distance) {
        // Integer form of the GTE H/SZ reciprocal boundary. This preserves
        // the hardware's 17-bit saturation and round-to-nearest behavior;
        // the reciprocal-table edge cases are covered independently later.
        quotient = std::min<std::uint32_t>(
            0x1FFFFu,
            static_cast<std::uint32_t>(
                (std::uint64_t(config.projection_distance) * 0x10000u +
                 out.depth / 2u) / out.depth));
    } else {
        out.flags |= ZdomfProjectionDivideOverflow;
    }

    const auto sx = config.geom_offset_x +
        ((std::int64_t(camera[0]) * quotient) >> 16);
    const auto sy = config.geom_offset_y +
        ((std::int64_t(camera[1]) * quotient) >> 16);
    out.x = static_cast<std::int16_t>(
        clamp_screen(sx, out.flags) + config.draw_offset_x);
    out.y = static_cast<std::int16_t>(
        clamp_screen(sy, out.flags) + config.draw_offset_y);
    return out;
}

} // namespace nba97
