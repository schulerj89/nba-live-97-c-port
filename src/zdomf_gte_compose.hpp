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

// FUN_80062C40 scales all nine signed 4.12 rotation elements by one signed
// 16.16 value before the root matrix is submitted to FUN_80066FF4.
ZdomfTransform scale_zdomf_gte_rotation(
    const ZdomfTransform& matrix,
    std::int32_t scale_16_16);

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

// Reproduces 0x80066580..0x800665D8 in FUN_80066090. The original loads the
// composed part rotation, the parent matrix translation, and the part's
// attachment vector, then stores GTE MAC1/MAC2/MAC3 as the part translation.
std::array<std::int32_t, 3> transform_zdomf_gte_attachment(
    const ZdomfTransform& composed_part_matrix,
    const std::array<std::int32_t, 3>& parent_translation,
    const ZdomfVec3& attachment_vector);

} // namespace nba97
