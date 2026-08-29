#include "zdomf_gte_compose.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint32_t packed_pair(std::int16_t low, std::int16_t high) {
    return std::uint16_t(low) | (std::uint32_t(std::uint16_t(high)) << 16);
}

} // namespace

int main() {
    try {
        // Exact live no$psx captures from the two consecutive composition
        // boundaries. FUN_80066FF4 builds 0x800F9450 from the frontend and
        // scaled-root matrices using column submissions.
        nba97::ZdomfTransform view{};
        view.rotation = {{{{6272, 0, -1892}},
                          {{-5, -4096, -19}},
                          {{-1183, 19, -3920}}}};
        view.translation = {{448, 192, 5}};
        nba97::ZdomfTransform scaled_root{};
        scaled_root.rotation = {{{{-2108, 0, 1259}},
                                 {{0, 2457, 0}},
                                 {{-1260, 0, -2108}}}};
        scaled_root.translation = {{250, 226, 635}};
        const auto root_result = nba97::compose_zdomf_gte_columns(
            view, scaled_root);

        const std::array<std::array<std::int16_t, 3>, 3> expected_root{{
            {{-2646, 0, 2901}},
            {{8, -2457, 8}},
            {{1814, 11, 1653}},
        }};
        check(root_result.matrix.rotation == expected_root,
              "FUN_80066FF4 live root composition differs");

        nba97::ZdomfTransform signed_scale_probe{};
        signed_scale_probe.rotation[0] = {{4096, -4096, 1}};
        const auto scaled_probe = nba97::scale_zdomf_gte_rotation(
            signed_scale_probe, 39312);
        check(scaled_probe.rotation[0][0] == 2457 &&
              scaled_probe.rotation[0][1] == -2457 &&
              scaled_probe.rotation[0][2] == 0,
              "FUN_80062C40 signed high-word scaling differs");

        // FUN_80066090 then consumes that exact matrix at PC 0x80066434,
        // part r7=9, but submits rows from the local part matrix.
        nba97::ZdomfTransform gte{};
        gte.rotation = root_result.matrix.rotation;
        gte.translation = {{544, 16, 594}};
        nba97::ZdomfTransform local{};
        local.rotation = {{{{-96, 3442, 2213}},
                           {{1076, 2155, -3308}},
                           {{-3950, 505, -955}}}};

        const auto result = nba97::compose_zdomf_gte_rows(gte, local);
        const std::array<std::array<std::int16_t, 3>, 3> expected_mvmva{{
            {{1629, -2061, 859}},
            {{-3038, -1298, -853}},
            {{1875, -313, -2134}},
        }};
        const std::array<std::array<std::int16_t, 3>, 3> expected_matrix{{
            {{1629, -3038, 1875}},
            {{-2061, -1298, -313}},
            {{859, -853, -2134}},
        }};
        check(result.mvmva_vectors == expected_mvmva,
              "FUN_80066090 MVMVA row results differ");
        check(result.matrix.rotation == expected_matrix,
              "FUN_80066090 packed matrix differs");
        check(packed_pair(result.matrix.rotation[0][0],
                          result.matrix.rotation[0][1]) == 0xF422065Du,
              "live 0x80066434 first output word differs");
        check(result.matrix.translation == std::array<std::int32_t, 3>{{0, 0, 0}},
              "rotation composition leaked translation");

        // Exact continuation of the live part-9 capture. FUN_80066090 loads
        // this composed matrix, parent translation, and attachment vector,
        // then stores MAC1..MAC3 at 0x80138FA8..0x80138FB0.
        nba97::ZdomfTransform captured_part9{};
        captured_part9.rotation = {{{{1978, 2819, -1882}},
                                    {{1856, -241, 1588}},
                                    {{1022, -1693, -1454}}}};
        const auto captured_translation = nba97::transform_zdomf_gte_attachment(
            captured_part9, {563, -39, 553}, {91, 0, 0});
        check(captured_translation ==
                  std::array<std::int32_t, 3>{{606, 2, 575}},
              "FUN_80066090 live part9 attachment translation differs");
        std::cout << "ZDOMF GTE COMPOSE: PASS - live view x scaled-root "
                     "matrix, part9 first word 0xF422065D, all IR values, "
                     "and translated origin {606,2,575} match\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ZDOMF GTE COMPOSE: FAIL - " << error.what() << '\n';
        return 1;
    }
}
