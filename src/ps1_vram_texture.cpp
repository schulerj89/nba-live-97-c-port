#include "ps1_vram_texture.hpp"

#include <utility>

namespace nba97 {
namespace {
std::uint8_t expand5(std::uint16_t value) {
    return static_cast<std::uint8_t>((value << 3) | (value >> 2));
}

int texels_per_word(int bits_per_pixel) {
    return 16 / bits_per_pixel;
}

int upload_word_width(const Ps1VramTextureUpload& upload) {
    const auto per_word = texels_per_word(upload.bits_per_pixel);
    return (static_cast<int>(upload.image.width) + per_word - 1) / per_word;
}

std::uint16_t indexed_upload_word(const Ps1VramTextureUpload& upload,
                                  int word_offset, int row) {
    const auto per_word = texels_per_word(upload.bits_per_pixel);
    const auto mask = upload.bits_per_pixel == 4 ? 0x0fu : 0xffu;
    std::uint16_t word = 0;
    for (int texel = 0; texel < per_word; ++texel) {
        const auto x = word_offset * per_word + texel;
        std::uint8_t index = 0;
        if (x < upload.image.width) {
            index = upload.indices[static_cast<std::size_t>(row) *
                                   upload.image.width + x];
        }
        word |= static_cast<std::uint16_t>(index & mask) <<
                (texel * upload.bits_per_pixel);
    }
    return word;
}
}

void Ps1VramTextureAtlas::upload8(PshImage image, int x_words, int y) {
    uploads_.push_back({x_words, y, 8, std::move(image), {}, 0xffff});
    upload_candidates_.clear();
}

void Ps1VramTextureAtlas::upload4(PshImage image, int x_words, int y) {
    uploads_.push_back({x_words, y, 4, std::move(image), {}, 0xffff});
    upload_candidates_.clear();
}

void Ps1VramTextureAtlas::upload8Indexed(
    int width, int height, std::vector<std::uint8_t> indices,
    int x_words, int y, std::uint16_t variant) {
    PshImage image{}; image.width = static_cast<std::uint16_t>(width);
    image.height = static_cast<std::uint16_t>(height);
    uploads_.push_back({x_words, y, 8, std::move(image), std::move(indices), variant});
    upload_candidates_.clear();
}

void Ps1VramTextureAtlas::upload4Indexed(
    int width, int height, std::vector<std::uint8_t> indices,
    int x_words, int y) {
    PshImage image{}; image.width = static_cast<std::uint16_t>(width);
    image.height = static_cast<std::uint16_t>(height);
    uploads_.push_back({x_words, y, 4, std::move(image), std::move(indices), 0xffff});
    upload_candidates_.clear();
}

void Ps1VramTextureAtlas::uploadClut(
    std::vector<std::uint16_t> colors, int x, int y, std::uint16_t variant) {
    cluts_.push_back({x, y, variant, std::move(colors)});
    clut_candidates_.clear();
}

const std::vector<std::size_t>& Ps1VramTextureAtlas::uploadCandidates(
    std::uint16_t texture_variant) const {
    const auto found = upload_candidates_.find(texture_variant);
    if (found != upload_candidates_.end()) return found->second;
    std::vector<std::size_t> candidates;
    candidates.reserve(16);
    for (std::size_t index = uploads_.size(); index-- > 0;) {
        const auto variant = uploads_[index].variant;
        if (variant == 0xffff || variant == texture_variant)
            candidates.push_back(index);
    }
    return upload_candidates_.emplace(texture_variant, std::move(candidates))
        .first->second;
}

const std::vector<std::size_t>& Ps1VramTextureAtlas::clutCandidates(
    std::uint16_t palette_variant) const {
    const auto found = clut_candidates_.find(palette_variant);
    if (found != clut_candidates_.end()) return found->second;
    std::vector<std::size_t> candidates;
    candidates.reserve(8);
    for (std::size_t index = cluts_.size(); index-- > 0;) {
        const auto variant = cluts_[index].variant;
        if (variant == 0xffff || variant == palette_variant)
            candidates.push_back(index);
    }
    return clut_candidates_.emplace(palette_variant, std::move(candidates))
        .first->second;
}

bool Ps1VramTextureAtlas::sample(std::uint16_t clut, std::uint16_t tpage,
                                 int u, int v, std::uint16_t palette_variant,
                                 std::uint16_t texture_variant,
                                 std::array<std::uint8_t, 3>& rgb) const {
    return sampleDetailed(clut, tpage, u, v, palette_variant,
                          texture_variant, rgb) == Ps1TextureSample::Opaque;
}

Ps1TextureSample Ps1VramTextureAtlas::sampleDetailed(
    std::uint16_t clut, std::uint16_t tpage, int u, int v,
    std::uint16_t palette_variant, std::uint16_t texture_variant,
    std::array<std::uint8_t, 3>& rgb, Ps1TextureTrace* trace) const {
    if (trace) *trace = {};
    // TPAGE bits 7-8 select 4/8/15-bpp.
    const auto depth = (tpage >> 7) & 3u;
    if (depth > 1u) return Ps1TextureSample::Missing;
    const int bits_per_pixel = depth == 0 ? 4 : 8;
    const int packet_texels_per_word = texels_per_word(bits_per_pixel);
    const int page_x_words = (tpage & 0x0fu) * 64;
    const int page_y = (tpage & 0x10u) ? 256 : 0;
    const int global_y = page_y + v;
    const int word_x = page_x_words + u / packet_texels_per_word;
    const int texel_in_word = u % packet_texels_per_word;
    if (trace) {
        trace->packet_bits_per_pixel = bits_per_pixel;
        trace->word_x = word_x;
        trace->word_y = global_y;
        trace->texel_in_word = texel_in_word;
    }
    // PS1 VRAM is mutable 16-bit storage. Resolve the last upload that covers
    // the addressed word regardless of the upload's source pixel depth, then
    // interpret that raw word using the packet TPAGE depth. Retaining an
    // upload's 4/8-bpp identity while sampling is observably wrong when later
    // uploads overlap a word through a differently typed TPAGE.
    for (const auto upload_index : uploadCandidates(texture_variant)) {
        const auto& upload = uploads_[upload_index];
        const int ty = global_y - upload.y;
        const int upload_word = word_x - upload.x_words;
        if (upload_word < 0 || ty < 0 ||
            upload_word >= upload_word_width(upload) ||
            ty >= upload.image.height) continue;
        const int tx = upload_word * packet_texels_per_word + texel_in_word;
        if (trace) {
            trace->upload_index = upload_index;
            trace->upload_bits_per_pixel = upload.bits_per_pixel;
            trace->upload_x = tx;
            trace->upload_y = ty;
        }
        if (!upload.indices.empty()) {
            const auto word = indexed_upload_word(upload, upload_word, ty);
            if (trace) trace->vram_word = word;
            const auto index = static_cast<std::uint8_t>(
                (word >> (texel_in_word * bits_per_pixel)) &
                (bits_per_pixel == 4 ? 0x0fu : 0xffu));
            if (trace) trace->palette_index = index;
            const int clut_x = (clut & 0x3fu) * 16;
            const int clut_y = clut >> 6;
            for (const auto clut_index : clutCandidates(palette_variant)) {
                const auto& palette = cluts_[clut_index];
                if (palette.y != clut_y ||
                    clut_x + index < palette.x ||
                    clut_x + index >= palette.x + int(palette.colors.size()))
                    continue;
                const auto color = palette.colors[clut_x + index - palette.x];
                if (trace) {
                    trace->palette_value = color;
                    trace->clut_upload_index = clut_index;
                }
                if (color == 0) return Ps1TextureSample::Transparent;
                rgb = {{expand5(color & 31u), expand5((color >> 5) & 31u),
                        expand5((color >> 10) & 31u)}};
                return Ps1TextureSample::Opaque;
            }
            return Ps1TextureSample::Missing;
        }
        // The RGBA overloads are retained for small synthetic tests. Actual
        // Create Player uploads are indexed, so a cross-depth word always
        // takes the raw path above.
        if (upload.bits_per_pixel != bits_per_pixel || tx < 0 ||
            tx >= upload.image.width) return Ps1TextureSample::Missing;
        const auto at = (static_cast<std::size_t>(ty) * upload.image.width + tx) * 4;
        if (!upload.image.rgba[at + 3]) return Ps1TextureSample::Transparent;
        rgb = {{upload.image.rgba[at], upload.image.rgba[at + 1],
                upload.image.rgba[at + 2]}};
        return Ps1TextureSample::Opaque;
    }
    return Ps1TextureSample::Missing;
}

} // namespace nba97
