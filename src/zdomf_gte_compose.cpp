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

std::int16_t gte_ir(std::int64_t mac) {
    const auto shifted = gte_shift12(mac);
    if (shifted > std::numeric_limits<std::int16_t>::max())
        return std::numeric_limits<std::int16_t>::max();
    if (shifted < std::numeric_limits<std::int16_t>::min())
        return std::numeric_limits<std::int16_t>::min();
    return static_cast<std::int16_t>(shifted);
}

} // namespace

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

} // namespace nba97
