#include "zdomf_gte_compose.hpp"

#include <cstdint>
#include <limits>

namespace nba97 {
namespace {

std::int32_t gte_shift12(std::int64_t value) {
    // MVMVA uses sf=1. Arithmetic right shift rounds negative values toward
    // negative infinity, matching the retail MIPS/GTE result.
    if (value >= 0) return static_cast<std::int32_t>(value / 4096);
    return static_cast<std::int32_t>(-(((-value) + 4095) / 4096));
}

std::int32_t signed_shift16(std::int64_t value) {
    if (value >= 0) return static_cast<std::int32_t>(value / 65536);
    return static_cast<std::int32_t>(-(((-value) + 65535) / 65536));
}

std::int16_t gte_ir(std::int64_t mac) {
    const auto shifted = gte_shift12(mac);
    if (shifted > std::numeric_limits<std::int16_t>::max())
        return std::numeric_limits<std::int16_t>::max();
    if (shifted < std::numeric_limits<std::int16_t>::min())
        return std::numeric_limits<std::int16_t>::min();
    return static_cast<std::int16_t>(shifted);
}

} // namespace

ZdomfTransform scale_zdomf_gte_rotation(
    const ZdomfTransform& matrix,
    std::int32_t scale_16_16) {
    ZdomfTransform out = matrix;
    for (auto& row : out.rotation) {
        for (auto& value : row) {
            value = static_cast<std::int16_t>(signed_shift16(
                std::int64_t(value) * scale_16_16));
        }
    }
    return out;
}

ZdomfGteComposeResult compose_zdomf_gte_columns(
    const ZdomfTransform& gte_matrix,
    const ZdomfTransform& right_matrix) {
    ZdomfGteComposeResult out{};
    for (std::size_t submitted_column = 0; submitted_column < 3;
         ++submitted_column) {
        for (std::size_t ir = 0; ir < 3; ++ir) {
            std::int64_t mac = 0;
            for (std::size_t component = 0; component < 3; ++component) {
                mac += std::int64_t(gte_matrix.rotation[ir][component]) *
                       right_matrix.rotation[component][submitted_column];
            }
            const auto value = gte_ir(mac);
            out.mvmva_vectors[submitted_column][ir] = value;
            out.matrix.rotation[ir][submitted_column] = value;
        }
    }
    return out;
}

ZdomfGteComposeResult compose_zdomf_gte_rows(
    const ZdomfTransform& gte_matrix,
    const ZdomfTransform& local_matrix) {
    ZdomfGteComposeResult out{};
    for (std::size_t submitted_row = 0; submitted_row < 3; ++submitted_row) {
        for (std::size_t ir = 0; ir < 3; ++ir) {
            std::int64_t mac = 0;
            for (std::size_t component = 0; component < 3; ++component) {
                mac += std::int64_t(gte_matrix.rotation[ir][component]) *
                       local_matrix.rotation[submitted_row][component];
            }
            const auto value = gte_ir(mac);
            out.mvmva_vectors[submitted_row][ir] = value;
            // 80066434..8006646C interleave IR results into PS1 MATRIX
            // column positions. This is the layout difference that the host
            // multiply previously missed.
            out.matrix.rotation[ir][submitted_row] = value;
        }
    }
    return out;
}

std::array<std::int32_t, 3> transform_zdomf_gte_attachment(
    const ZdomfTransform& composed_part_matrix,
    const std::array<std::int32_t, 3>& parent_translation,
    const ZdomfVec3& attachment_vector) {
    const std::array<std::int32_t, 3> input{{
        attachment_vector.x, attachment_vector.y, attachment_vector.z}};
    std::array<std::int32_t, 3> mac{};
    for (std::size_t row = 0; row < 3; ++row) {
        std::int64_t value = std::int64_t(parent_translation[row]) * 4096;
        for (std::size_t component = 0; component < 3; ++component) {
            value += std::int64_t(composed_part_matrix.rotation[row][component]) *
                     input[component];
        }
        // MVMVA has sf=1 and FUN_80066090 stores MAC1..MAC3, not the
        // saturated 16-bit IR registers.
        mac[row] = gte_shift12(value);
    }
    return {mac[0], mac[1], mac[2]};
}

} // namespace nba97
