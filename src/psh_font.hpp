#pragma once

#include "psh_image.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace nba97 {

struct PshGlyph {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::int16_t center_y = 0;
    bool source_transposed = false;
    std::vector<std::uint8_t> rgba;
};

class PshFont final {
public:
    [[nodiscard]] const PshGlyph* glyph(char character) const noexcept;
    [[nodiscard]] int textWidth(const std::string& text) const noexcept;
    [[nodiscard]] std::size_t glyphCount() const noexcept { return glyphs_.size(); }
    [[nodiscard]] std::size_t transposedGlyphCount() const noexcept;
    [[nodiscard]] int spaceWidth() const noexcept { return space_width_; }
    [[nodiscard]] int kerning() const noexcept { return kerning_; }

private:
    friend PshFont load_psh_font(const std::filesystem::path&, int, int);
    std::unordered_map<unsigned char, PshGlyph> glyphs_;
    int space_width_ = 0;
    int kerning_ = 0;
};

PshFont load_psh_font(const std::filesystem::path& path,
                      int space_width, int kerning);
void draw_psh_text_centered(PshImage& destination, const PshFont& font,
                            const std::string& text, int center_x, int y);

} // namespace nba97
