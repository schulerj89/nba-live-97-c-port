#pragma once

#include "psh_image.hpp"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nba97 {

enum class Ps1TextureSample {
    Missing,
    Transparent,
    Opaque,
};

struct Ps1TextureTrace {
    int packet_bits_per_pixel = 0;
    int word_x = -1;
    int word_y = -1;
    int texel_in_word = -1;
    std::uint16_t vram_word = 0;
    std::size_t upload_index = static_cast<std::size_t>(-1);
    int upload_bits_per_pixel = 0;
    int upload_x = -1;
    int upload_y = -1;
    std::uint8_t palette_index = 0;
    std::uint16_t palette_value = 0;
    std::size_t clut_upload_index = static_cast<std::size_t>(-1);
};

struct Ps1VramTextureUpload {
    int x_words = 0;
    int y = 0;
    int bits_per_pixel = 8;
    PshImage image{};
    std::vector<std::uint8_t> indices;
    std::uint16_t variant = 0xffff;
};

struct Ps1VramClutUpload {
    int x = 0;
    int y = 0;
    std::uint16_t variant = 0;
    std::vector<std::uint16_t> colors;
};

// Host-side equivalent of the PS1 texture-page address calculation. Upload
// coordinates remain in native 16-bit VRAM words; callers supply packet UVs.
class Ps1VramTextureAtlas final {
public:
    void upload8(PshImage image, int x_words, int y);
    void upload4(PshImage image, int x_words, int y);
    void upload8Indexed(int width, int height, std::vector<std::uint8_t> indices,
                        int x_words, int y, std::uint16_t variant = 0xffff);
    void upload4Indexed(int width, int height, std::vector<std::uint8_t> indices,
                        int x_words, int y);
    void uploadClut(std::vector<std::uint16_t> colors, int x, int y,
                    std::uint16_t variant = 0);
    [[nodiscard]] bool sample(std::uint16_t clut, std::uint16_t tpage,
                              int u, int v,
                              std::uint16_t palette_variant,
                              std::uint16_t texture_variant,
                              std::array<std::uint8_t, 3>& rgb) const;
    [[nodiscard]] Ps1TextureSample sampleDetailed(
        std::uint16_t clut, std::uint16_t tpage, int u, int v,
        std::uint16_t palette_variant, std::uint16_t texture_variant,
        std::array<std::uint8_t, 3>& rgb,
        Ps1TextureTrace* trace = nullptr) const;
    [[nodiscard]] std::size_t uploadCount() const { return uploads_.size(); }

private:
    [[nodiscard]] const std::vector<std::size_t>& uploadCandidates(
        std::uint16_t texture_variant) const;
    [[nodiscard]] const std::vector<std::size_t>& clutCandidates(
        std::uint16_t palette_variant) const;

    std::vector<Ps1VramTextureUpload> uploads_;
    std::vector<Ps1VramClutUpload> cluts_;
    // A Create Player team atlas contains 936 mutually exclusive head
    // variants and 144 mutually exclusive CLUT uploads. Sampling used to walk
    // every entry for every rasterized pixel. Cache the small reverse-ordered
    // subset that can be active for one variant; upload methods invalidate the
    // corresponding cache so last-write-wins semantics remain exact.
    mutable std::unordered_map<std::uint16_t, std::vector<std::size_t>>
        upload_candidates_;
    mutable std::unordered_map<std::uint16_t, std::vector<std::size_t>>
        clut_candidates_;
};

} // namespace nba97
