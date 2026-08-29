#include "zdomf_hierarchy.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        std::array<nba97::ZdomfVec3, 20> pivots{};
        for (auto& pivot : pivots) pivot = {10, 0, 0};
        const auto hierarchy = nba97::build_zdomf_hierarchy(pivots);
        check(hierarchy.root_count == 3 && hierarchy.max_depth == 5,
              "recovered topology dimensions");
        check(hierarchy.parts[0].joint_end.x == 10 &&
              hierarchy.parts[3].joint_origin.x == 30 &&
              hierarchy.parts[3].joint_end.x == 40,
              "first four-part chain");
        check(hierarchy.parts[12].parent == 9 && hierarchy.parts[16].parent == 9,
              "torso branch parents");
        check(hierarchy.parts[15].joint_end.x == 60 &&
              hierarchy.parts[19].joint_end.x == 60,
              "four-deep leg branches");
        const auto vertex = nba97::apply_zdomf_hierarchy(hierarchy, 3, {2, 3, 4});
        check(vertex.x == 32 && vertex.y == 3 && vertex.z == 4,
              "FUN_800632D4 parent-matrix translation");
        std::cout << "ZDOMF HIERARCHY: PASS - FUN_80069098 parent graph, "
                     "three roots, five-edge depth, and fixed-point world composition\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ZDOMF HIERARCHY: FAIL - " << error.what() << '\n';
        return 1;
    }
}
