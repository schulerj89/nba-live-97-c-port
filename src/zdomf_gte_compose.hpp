#pragma once

#include "zdomf_transform.hpp"

#include <array>

namespace nba97 {

// Observable intermediate values from FUN_80066090's three MVMVA commands.
// Each entry is the GTE IR1/IR2/IR3 result for one submitted local matrix row.
struct ZdomfGteComposeResult {
    std::array<std::array<std::int16_t, 3>, 3> mvmva_vectors{};
    ZdomfTransform matrix{};
};

// Reproduces FUN_80066FF4: submit each column of `right_matrix` to the GTE
// and store the result as the matching output column (ordinary fixed-point
// matrix multiplication, but with retail GTE shift and saturation semantics).
ZdomfGteComposeResult compose_zdomf_gte_columns(
    const ZdomfTransform& gte_matrix,
    const ZdomfTransform& right_matrix);

// Reproduces FUN_80066090's rotation-only GTE composition. The original loads
// `gte_matrix` as the GTE rotation matrix, submits each row of `local_matrix`
// as V0, then packs the three result vectors as output columns.
ZdomfGteComposeResult compose_zdomf_gte_rows(
    const ZdomfTransform& gte_matrix,
    const ZdomfTransform& local_matrix);

} // namespace nba97
