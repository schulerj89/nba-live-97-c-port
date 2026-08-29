#include "zdomf_transform.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void put16(std::vector<std::uint8_t>& data, std::size_t at, std::uint16_t value) {
    data[at] = static_cast<std::uint8_t>(value);
    data[at + 1] = static_cast<std::uint8_t>(value >> 8);
}
void put32(std::vector<std::uint8_t>& data, std::size_t at, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        data[at + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        std::vector<std::uint8_t> trig(4096 * 4, 0);
        put32(trig, 0, 0x10000000);       // sin=0, cos=4096
        put32(trig, 1024 * 4, 0x00001000); // sin=4096, cos=0
        std::vector<std::uint8_t> deflist(20 * 8, 0);
        put16(deflist, 8 + 4, 1024); // part 1: +90 degrees around Z

        const auto set = nba97::decode_zdomf_base_transforms(deflist, trig);
        check(set.available_sets == 1, "transform set count");
        const auto identity = nba97::apply_zdomf_transform(
            set.parts[0], nba97::ZdomfVec3{10, -20, 30});
        check(identity.x == 10 && identity.y == -20 && identity.z == 30,
              "identity fixed-point transform");
        const auto rotated = nba97::apply_zdomf_transform(
            set.parts[1], nba97::ZdomfVec3{10, 0, 0});
        check(rotated.x == 0 && rotated.y == 10 && rotated.z == 0,
              "quarter-turn fixed-point transform");
        std::cout << "ZDOMF TRANSFORM: PASS - FUN_80067100 matrix construction and "
                     "FUN_80067378 fixed-point application\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ZDOMF TRANSFORM: FAIL - " << error.what() << '\n';
        return 1;
    }
}
