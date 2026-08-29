#include "ps1_vram_texture.hpp"

#include <utility>

namespace nba97 {

void Ps1VramTextureAtlas::upload8(PshImage image, int x_words, int y) {
    uploads_.push_back({x_words, y, 8, std::move(image)});
}

void Ps1VramTextureAtlas::upload4(PshImage image, int x_words, int y) {
    uploads_.push_back({x_words, y, 4, std::move(image)});
}

bool Ps1VramTextureAtlas::sample(std::uint16_t tpage, int u, int v,
                                 std::array<std::uint8_t, 3>& rgb) const {
    // TPAGE bits 7-8 select 4/8/15-bpp.
    const auto depth = (tpage >> 7) & 3u;
    if (depth > 1u) return false;
    const int bits_per_pixel = depth == 0 ? 4 : 8;
    const int page_x_words = (tpage & 0x0fu) * 64;
    const int page_y = (tpage & 0x10u) ? 256 : 0;
    const int global_y = page_y + v;
    for (const auto& upload : uploads_) {
        if (upload.bits_per_pixel != bits_per_pixel) continue;
        const int texels_per_word = 16 / bits_per_pixel;
        const int global_x = page_x_words * texels_per_word + u;
        const int tx = global_x - upload.x_words * texels_per_word;
        const int ty = global_y - upload.y;
        if (tx < 0 || ty < 0 || tx >= upload.image.width ||
            ty >= upload.image.height) continue;
        const auto at = (static_cast<std::size_t>(ty) * upload.image.width + tx) * 4;
        if (!upload.image.rgba[at + 3]) return false;
        rgb = {{upload.image.rgba[at], upload.image.rgba[at + 1],
                upload.image.rgba[at + 2]}};
        return true;
    }
    return false;
}

} // namespace nba97
