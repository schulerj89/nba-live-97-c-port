#include "psh_font.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace nba97 {
namespace {

std::uint16_t readU16(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + 2 > data.size()) throw std::runtime_error("truncated font u16");
    return static_cast<std::uint16_t>(data[at] | (data[at + 1] << 8));
}

std::int16_t readS16(const std::vector<std::uint8_t>& data, std::size_t at) {
    return static_cast<std::int16_t>(readU16(data, at));
}

std::uint32_t readU24(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + 3 > data.size()) throw std::runtime_error("truncated font u24");
    return static_cast<std::uint32_t>(data[at]) |
           (static_cast<std::uint32_t>(data[at + 1]) << 8) |
           (static_cast<std::uint32_t>(data[at + 2]) << 16);
}

std::uint32_t readU32(const std::vector<std::uint8_t>& data, std::size_t at) {
    if (at + 4 > data.size()) throw std::runtime_error("truncated font u32");
    return static_cast<std::uint32_t>(data[at]) |
           (static_cast<std::uint32_t>(data[at + 1]) << 8) |
           (static_cast<std::uint32_t>(data[at + 2]) << 16) |
           (static_cast<std::uint32_t>(data[at + 3]) << 24);
}

bool magicAt(const std::vector<std::uint8_t>& data, std::size_t at,
             const char* magic) {
    return at + 4 <= data.size() && data[at] == magic[0] &&
           data[at + 1] == magic[1] && data[at + 2] == magic[2] &&
           data[at + 3] == magic[3];
}

int hexDigit(std::uint8_t value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

int parseTag(const std::vector<std::uint8_t>& data, std::size_t at) {
    int value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        const int digit = hexDigit(data[at + i]);
        if (digit < 0) return -1;
        value = value * 16 + digit;
    }
    return value;
}

std::uint8_t expand5(std::uint16_t value) {
    return static_cast<std::uint8_t>((value << 3) | (value >> 2));
}

PshGlyph decodeGlyph(const std::vector<std::uint8_t>& data, std::size_t bitmap) {
    if (bitmap + 16 > data.size() || data[bitmap] != 0x40)
        throw std::runtime_error("font glyph is not a PSX 4bpp bitmap");

    const std::uint32_t palette_delta = readU24(data, bitmap + 1);
    const std::size_t palette = bitmap + palette_delta;
    if (!palette_delta || palette + 48 > data.size() || data[palette] != 0x23)
        throw std::runtime_error("font glyph has no PSX palette chunk");
    if (readU16(data, palette + 4) < 16)
        throw std::runtime_error("font glyph palette is too small");

    PshGlyph glyph;
    const std::uint16_t stored_width = readU16(data, bitmap + 4);
    const std::uint16_t stored_height = readU16(data, bitmap + 6);
    glyph.center_y = readS16(data, bitmap + 10);
    // FEONLY 0x80029EC0 tests the Position-X halfword as signed. When bit 15
    // is set, its four recovered UV assignments transpose the stored bitmap:
    // destination(x,y) samples source(y,x), and width/height are exchanged.
    glyph.source_transposed = readS16(data, bitmap + 12) < 0;
    glyph.width = glyph.source_transposed ? stored_height : stored_width;
    glyph.height = glyph.source_transposed ? stored_width : stored_height;
    if (!stored_width || !stored_height)
        throw std::runtime_error("font glyph has invalid dimensions");

    const std::size_t packed_row = (static_cast<std::size_t>(stored_width) + 1) / 2;
    const std::size_t row_stride = (packed_row + 1) & ~std::size_t{1};
    if (bitmap + 16 + row_stride * stored_height > palette)
        throw std::runtime_error("font glyph bitmap is truncated");

    glyph.rgba.resize(static_cast<std::size_t>(glyph.width) * glyph.height * 4);
    for (std::size_t y = 0; y < glyph.height; ++y) {
        for (std::size_t x = 0; x < glyph.width; ++x) {
            const std::size_t source_x = glyph.source_transposed ? y : x;
            const std::size_t source_y = glyph.source_transposed ? x : y;
            const std::uint8_t packed =
                data[bitmap + 16 + source_y * row_stride + source_x / 2];
            const std::uint8_t index =
                (source_x & 1) ? packed >> 4 : packed & 0x0f;
            const std::uint16_t color = readU16(data, palette + 16 + index * 2);
            const std::size_t output = (y * glyph.width + x) * 4;
            glyph.rgba[output] = expand5(color & 0x1f);
            glyph.rgba[output + 1] = expand5((color >> 5) & 0x1f);
            glyph.rgba[output + 2] = expand5((color >> 10) & 0x1f);
            glyph.rgba[output + 3] = index == 0 ? 0 : 255;
        }
    }
    return glyph;
}

} // namespace

const PshGlyph* PshFont::glyph(char character) const noexcept {
    const auto found = glyphs_.find(static_cast<unsigned char>(character));
    return found == glyphs_.end() ? nullptr : &found->second;
}

int PshFont::textWidth(const std::string& text) const noexcept {
    int width = 0;
    for (char character : text) {
        if (character == ' ') {
            width += space_width_;
        } else if (const PshGlyph* item = glyph(character)) {
            width += std::max(0, static_cast<int>(item->width) - kerning_);
        } else {
            width += space_width_;
        }
    }
    return width;
}

std::size_t PshFont::transposedGlyphCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        glyphs_.begin(), glyphs_.end(), [](const auto& entry) {
            return entry.second.source_transposed;
        }));
}

PshFont load_psh_font(const std::filesystem::path& path,
                      int space_width, int kerning) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open " + path.string());
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
    if (data.size() < 16 || !magicAt(data, 0, "SHPP") || !magicAt(data, 12, "GIMX"))
        throw std::runtime_error(path.string() + ": not an SHPP/GIMX font archive");

    const std::uint32_t count = readU32(data, 8);
    if (16 + static_cast<std::size_t>(count) * 8 > data.size())
        throw std::runtime_error(path.string() + ": truncated font directory");

    PshFont font;
    font.space_width_ = space_width;
    font.kerning_ = kerning;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::size_t entry = 16 + static_cast<std::size_t>(index) * 8;
        const int character = parseTag(data, entry);
        if (character < 0 || character > 0xff) continue;
        const std::size_t bitmap = readU32(data, entry + 4);
        font.glyphs_[static_cast<unsigned char>(character)] = decodeGlyph(data, bitmap);
    }
    return font;
}

void draw_psh_text_centered(PshImage& destination, const PshFont& font,
                            const std::string& text, int center_x, int y) {
    int x = center_x - font.textWidth(text) / 2;
    for (char character : text) {
        if (character == ' ') {
            x += font.spaceWidth();
            continue;
        }
        const PshGlyph* glyph = font.glyph(character);
        if (!glyph) {
            x += font.spaceWidth();
            continue;
        }
        const int draw_y = y - glyph->center_y;
        for (int glyph_y = 0; glyph_y < glyph->height; ++glyph_y) {
            const int target_y = draw_y + glyph_y;
            if (target_y < 0 || target_y >= destination.height) continue;
            for (int glyph_x = 0; glyph_x < glyph->width; ++glyph_x) {
                const int target_x = x + glyph_x;
                if (target_x < 0 || target_x >= destination.width) continue;
                const std::size_t source =
                    (static_cast<std::size_t>(glyph_y) * glyph->width + glyph_x) * 4;
                if (glyph->rgba[source + 3] == 0) continue;
                const std::size_t target =
                    (static_cast<std::size_t>(target_y) * destination.width + target_x) * 4;
                std::copy_n(glyph->rgba.data() + source, 4,
                            destination.rgba.data() + target);
            }
        }
        x += std::max(0, static_cast<int>(glyph->width) - font.kerning());
    }
}

} // namespace nba97
