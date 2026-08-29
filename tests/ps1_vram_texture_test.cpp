#include "ps1_vram_texture.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
PshImage image(int width, int height, std::array<std::uint8_t, 3> rgb) {
    PshImage out;
    out.width = width;
    out.height = height;
    out.rgba.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t at = 0; at < out.rgba.size(); at += 4) {
        out.rgba[at] = rgb[0]; out.rgba[at + 1] = rgb[1];
        out.rgba[at + 2] = rgb[2]; out.rgba[at + 3] = 255;
    }
    return out;
}
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        nba97::Ps1VramTextureAtlas atlas;
        atlas.upload8(image(255, 118, {{10, 20, 30}}), 832, 256);
        atlas.upload8(image(120, 80, {{40, 50, 60}}), 889, 374);
        atlas.upload4(image(64, 45, {{70, 80, 90}}), 922, 454);
        std::array<std::uint8_t, 3> rgb{};
        check(atlas.sample(0x00BD, 10, 20, rgb) && rgb[0] == 10,
              "TPAGE BD jersey address");
        check(atlas.sample(0x00BE, 0, 20, rgb) && rgb[1] == 20,
              "TPAGE BE crosses into jersey x=128");
        check(atlas.sample(0x00BD, 114, 118, rgb) && rgb[2] == 60,
              "FUN_80067A14 shorts upload origin");
        check(atlas.sample(0x003E, 104, 199, rgb) && rgb[0] == 70,
              "4-bpp SHOE upload origin");
        std::cout << "PS1 VRAM TEXTURE: PASS - 4/8-bpp TPAGE word addressing and recovered upload origins\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PS1 VRAM TEXTURE: FAIL - " << error.what() << '\n';
        return 1;
    }
}
