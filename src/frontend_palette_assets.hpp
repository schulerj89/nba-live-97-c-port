#pragma once
#include "psh_image.hpp"
#include "recovered/frontend_palette.h"
#include <array>

namespace nba97 {
// Shared immutable indexed source. Only the two 160-word CLUTs animate.
class FrontendPaletteAssets {
public:
    static constexpr unsigned count=33;
    explicit FrontendPaletteAssets(const std::filesystem::path&);
    explicit FrontendPaletteAssets(const std::vector<std::uint8_t>&);
    const std::uint16_t* bank() const { return bank_.data(); }
    void draw(PshImage&, const Nba97FrontendPalette&) const;
private:
    std::array<std::uint16_t,count*160> bank_{};
    std::array<std::uint8_t,4*128*240> indices_{};
    std::array<std::uint16_t,4*96> local_{};
    void parse(const std::vector<std::uint8_t>&);
};
}
