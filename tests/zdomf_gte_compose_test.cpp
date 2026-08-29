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
        std::cout << "ZDOMF GTE COMPOSE: PASS - live view x scaled-root "
                     "matrix, part9 first word 0xF422065D, and all IR values "
                     "match\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ZDOMF GTE COMPOSE: FAIL - " << error.what() << '\n';
        return 1;
    }
}
