#pragma once

#include "psh_image.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace nba97 {

struct Ps1VramTextureUpload {
    int x_words = 0;
    int y = 0;
    int bits_per_pixel = 8;
    PshImage image{};
};

// Host-side equivalent of the PS1 texture-page address calculation. Upload
// coordinates remain in native 16-bit VRAM words; callers supply packet UVs.
class Ps1VramTextureAtlas final {
public:
    void upload8(PshImage image, int x_words, int y);
    void upload4(PshImage image, int x_words, int y);
    [[nodiscard]] bool sample(std::uint16_t tpage, int u, int v,
                              std::array<std::uint8_t, 3>& rgb) const;
    [[nodiscard]] std::size_t uploadCount() const { return uploads_.size(); }

private:
    std::vector<Ps1VramTextureUpload> uploads_;
};

} // namespace nba97
