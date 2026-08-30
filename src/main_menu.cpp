#include "main_menu.hpp"
#include "create_player_preview.hpp"
#include "frontend_title.hpp"
#include "recovered/semantic_trace.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace nba97 {
namespace {
constexpr int kWidth = 512;
constexpr int kHeight = 240;
constexpr std::array<const char*, 4> kOptionNames{
    "quarter", "mode", "style", "level"};
constexpr std::array<const char*, 4> kOptionValues{
    "3 min", "exhibition", "arcade", "rookie"};
constexpr std::array<const char*, 5> kButtonNames{
    "rules", "options", "rosters", "users", "card"};
constexpr std::array<int, 4> kCardCenters{86, 200, 314, 428};
constexpr std::array<int, 5> kButtonCenters{55, 155, 256, 357, 457};

void putPixel(PshImage& image, int x, int y, std::uint8_t r, std::uint8_t g,
              std::uint8_t b, std::uint8_t a = 255) {
    if (x < 0 || x >= image.width || y < 0 || y >= image.height) return;
    const std::size_t at = (static_cast<std::size_t>(y) * image.width + x) * 4;
    image.rgba[at] = r;
    image.rgba[at + 1] = g;
    image.rgba[at + 2] = b;
    image.rgba[at + 3] = a;
}

void fillRect(PshImage& image, int left, int top, int right, int bottom,
              std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    for (int y = std::max(0, top); y < std::min<int>(image.height, bottom); ++y)
        for (int x = std::max(0, left); x < std::min<int>(image.width, right); ++x)
            putPixel(image, x, y, r, g, b);
}

void outlineRect(PshImage& image, int left, int top, int right, int bottom,
                 int thickness, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    fillRect(image, left, top, right, top + thickness, r, g, b);
    fillRect(image, left, bottom - thickness, right, bottom, r, g, b);
    fillRect(image, left, top, left + thickness, bottom, r, g, b);
    fillRect(image, right - thickness, top, right, bottom, r, g, b);
}

void blitScaled(PshImage& destination, const PshImage& source,
                int source_x, int source_y, int source_w, int source_h,
                int target_x, int target_y, int target_w, int target_h) {
    for (int y = 0; y < target_h; ++y) {
        const int sy = source_y + y * source_h / target_h;
        if (sy < 0 || sy >= source.height) continue;
        for (int x = 0; x < target_w; ++x) {
            const int sx = source_x + x * source_w / target_w;
            if (sx < 0 || sx >= source.width) continue;
            const std::size_t from = (static_cast<std::size_t>(sy) * source.width + sx) * 4;
            const std::uint8_t alpha = source.rgba[from + 3];
            if (alpha == 0) continue;
            const int dx = target_x + x;
            const int dy = target_y + y;
            if (dx < 0 || dx >= destination.width || dy < 0 || dy >= destination.height) continue;
            const std::size_t to = (static_cast<std::size_t>(dy) * destination.width + dx) * 4;
            for (int channel = 0; channel < 3; ++channel)
                destination.rgba[to + channel] = static_cast<std::uint8_t>(
                    (source.rgba[from + channel] * alpha +
                     destination.rgba[to + channel] * (255 - alpha)) / 255);
            destination.rgba[to + 3] = 255;
        }
    }
}

void blitRotatedClockwise(PshImage& destination, const PshImage& source,
                          int target_x, int target_y) {
    for (int sy = 0; sy < source.height; ++sy) {
        for (int sx = 0; sx < source.width; ++sx) {
            const std::size_t from =
                (static_cast<std::size_t>(sy) * source.width + sx) * 4;
            if (source.rgba[from + 3] == 0) continue;
            const int dx = target_x + source.height - 1 - sy;
            const int dy = target_y + sx;
            putPixel(destination, dx, dy, source.rgba[from], source.rgba[from + 1],
                     source.rgba[from + 2], source.rgba[from + 3]);
        }
    }
}

void blitReorderPlate(PshImage& destination, const PshImage& source, int x, int y, int side) {
    // 8009370C/80093714, interpreted by 80034A5C: two fixed, four-corner
    // plate shapes. Ordinary native textured triangles, not a GPU emulator.
    static constexpr int quads[2][8] = {{0,0,86,10,0,58,106,60}, {20,10,106,0,0,60,106,58}};
    const auto& q = quads[side];
    static constexpr int triangles[2][3] = {{0,1,2}, {1,3,2}};
    for (const auto& t : triangles) {
        const int a=t[0], b=t[1], c=t[2];
        const double denominator = (q[2*b+1]-q[2*c+1])*(q[2*a]-q[2*c]) +
                                   (q[2*c]-q[2*b])*(q[2*a+1]-q[2*c+1]);
        for (int yy=0; yy<60; ++yy) for (int xx=0; xx<106; ++xx) {
            const double u=((q[2*b+1]-q[2*c+1])*(xx+0.5-q[2*c]) +
                            (q[2*c]-q[2*b])*(yy+0.5-q[2*c+1]))/denominator;
            const double v=((q[2*c+1]-q[2*a+1])*(xx+0.5-q[2*c]) +
                            (q[2*a]-q[2*c])*(yy+0.5-q[2*c+1]))/denominator;
            const double w=1-u-v;
            if (u<0 || v<0 || w<0) continue;
            const int sx=std::clamp(static_cast<int>((u*(a&1)+v*(b&1)+w*(c&1))*source.width),0,int(source.width)-1);
            const int sy=std::clamp(static_cast<int>((u*(a>>1)+v*(b>>1)+w*(c>>1))*source.height),0,int(source.height)-1);
            const auto at=(static_cast<std::size_t>(sy)*source.width+sx)*4;
            if(source.rgba[at+3]) putPixel(destination,x+xx,y+yy,source.rgba[at],source.rgba[at+1],source.rgba[at+2]);
        }
    }
}

const PshImage* sprite(const MenuSpritePack& sprites, const char* tag) {
    const auto found = sprites.find(tag);
    return found == sprites.end() ? nullptr : &found->second;
}

void blitAt(PshImage& destination, const MenuSpritePack& sprites,
            const char* tag, int x, int y) {
    if (const PshImage* item = sprite(sprites, tag))
        blitScaled(destination, *item, 0, 0, item->width, item->height,
                   x, y, item->width, item->height);
}

void blitInsideFrame(PshImage& destination, const PshImage& source,
                     const PshImage& frame, int source_x, int source_y,
                     int frame_x, int frame_y) {
    const int frame_size = frame.width * frame.height;
    if (frame_size <= 0) return;

    // The recovered `frml` record supplies its own irregular aperture. Treat
    // opaque frame texels as walls, flood transparent texels from the outside,
    // then retain the largest enclosed component. This matches the PS1
    // ordering-table composite while preventing wide team plates (notably
    // Chicago and Dallas) from showing through the frame's exterior cut-outs.
    std::vector<std::uint8_t> outside(static_cast<std::size_t>(frame_size), 0);
    std::deque<int> pending;
    const auto frameOpaque = [&](int at) {
        return frame.rgba[static_cast<std::size_t>(at) * 4 + 3] != 0;
    };
    const auto queueOutside = [&](int x, int y) {
        const int at = y * frame.width + x;
        if (!frameOpaque(at) && outside[static_cast<std::size_t>(at)] == 0) {
            outside[static_cast<std::size_t>(at)] = 1;
            pending.push_back(at);
        }
    };
    for (int x = 0; x < frame.width; ++x) {
        queueOutside(x, 0);
        queueOutside(x, frame.height - 1);
    }
    for (int y = 0; y < frame.height; ++y) {
        queueOutside(0, y);
        queueOutside(frame.width - 1, y);
    }
    while (!pending.empty()) {
        const int at = pending.front();
        pending.pop_front();
        const int x = at % frame.width;
        const int y = at / frame.width;
        if (x > 0) queueOutside(x - 1, y);
        if (x + 1 < frame.width) queueOutside(x + 1, y);
        if (y > 0) queueOutside(x, y - 1);
        if (y + 1 < frame.height) queueOutside(x, y + 1);
    }

    std::vector<std::uint8_t> visited(static_cast<std::size_t>(frame_size), 0);
    std::vector<int> aperture;
    for (int seed = 0; seed < frame_size; ++seed) {
        if (frameOpaque(seed) || outside[static_cast<std::size_t>(seed)] != 0 ||
            visited[static_cast<std::size_t>(seed)] != 0)
            continue;
        std::vector<int> component;
        pending.push_back(seed);
        visited[static_cast<std::size_t>(seed)] = 1;
        while (!pending.empty()) {
            const int at = pending.front();
            pending.pop_front();
            component.push_back(at);
            const int x = at % frame.width;
            const int y = at / frame.width;
            const auto queueComponent = [&](int next) {
                if (!frameOpaque(next) && outside[static_cast<std::size_t>(next)] == 0 &&
                    visited[static_cast<std::size_t>(next)] == 0) {
                    visited[static_cast<std::size_t>(next)] = 1;
                    pending.push_back(next);
                }
            };
            if (x > 0) queueComponent(at - 1);
            if (x + 1 < frame.width) queueComponent(at + 1);
            if (y > 0) queueComponent(at - frame.width);
            if (y + 1 < frame.height) queueComponent(at + frame.width);
        }
        if (component.size() > aperture.size()) aperture = std::move(component);
    }

    std::vector<std::uint8_t> aperture_mask(static_cast<std::size_t>(frame_size), 0);
    for (const int at : aperture) aperture_mask[static_cast<std::size_t>(at)] = 1;
    for (int sy = 0; sy < source.height; ++sy) {
        for (int sx = 0; sx < source.width; ++sx) {
            const int fx = source_x + sx - frame_x;
            const int fy = source_y + sy - frame_y;
            if (fx < 0 || fx >= frame.width || fy < 0 || fy >= frame.height ||
                aperture_mask[static_cast<std::size_t>(fy * frame.width + fx)] == 0)
                continue;
            const std::size_t from =
                (static_cast<std::size_t>(sy) * source.width + sx) * 4;
            if (source.rgba[from + 3] == 0) continue;
            putPixel(destination, source_x + sx, source_y + sy,
                     source.rgba[from], source.rgba[from + 1],
                     source.rgba[from + 2], source.rgba[from + 3]);
        }
    }
}

void blitPaletteCrossfade(PshImage& destination,
                          const PshImage& from, const PshImage& to,
                          int target_x, int target_y,
                          std::uint32_t factor) {
    if (from.width != to.width || from.height != to.height) return;
    factor = (std::min)(factor, 16u);
    for (int y = 0; y < to.height; ++y) {
        for (int x = 0; x < to.width; ++x) {
            const std::size_t source_at =
                (static_cast<std::size_t>(y) * to.width + x) * 4;
            const std::uint8_t alpha = factor < 8
                ? from.rgba[source_at + 3] : to.rgba[source_at + 3];
            if (alpha == 0) continue;
            const int dx = target_x + x;
            const int dy = target_y + y;
            if (dx < 0 || dx >= destination.width ||
                dy < 0 || dy >= destination.height)
                continue;
            const std::size_t destination_at =
                (static_cast<std::size_t>(dy) * destination.width + dx) * 4;
            for (int channel = 0; channel < 3; ++channel) {
                const std::uint32_t old_5 = from.rgba[source_at + channel] >> 3;
                const std::uint32_t new_5 = to.rgba[source_at + channel] >> 3;
                const std::uint32_t value_5 =
                    (old_5 * (16 - factor) + new_5 * factor) >> 4;
                destination.rgba[destination_at + channel] =
                    static_cast<std::uint8_t>((value_5 << 3) | (value_5 >> 2));
            }
            destination.rgba[destination_at + 3] = 255;
        }
    }
}

void blitDisabled(PshImage& destination, const MenuSpritePack& sprites,
                  const char* tag, int x, int y) {
    const PshImage* item = sprite(sprites, tag);
    if (!item) return;
    PshImage disabled = *item;
    for (std::size_t at = 0; at < disabled.rgba.size(); at += 4) {
        if (disabled.rgba[at + 3] == 0) continue;
        const int grey = (disabled.rgba[at] + disabled.rgba[at + 1] +
                          disabled.rgba[at + 2]) / 3;
        disabled.rgba[at] = static_cast<std::uint8_t>(grey * 42 / 100);
        disabled.rgba[at + 1] = static_cast<std::uint8_t>(grey * 42 / 100);
        disabled.rgba[at + 2] = static_cast<std::uint8_t>(grey * 42 / 100);
    }
    blitScaled(destination, disabled, 0, 0, disabled.width, disabled.height,
               x, y, disabled.width, disabled.height);
}

void blitBottomStrip(PshImage& destination, const MenuSpritePack& sprites,
                     const char* tag, int x, int y, int strip_y) {
    const PshImage* item = sprite(sprites, tag);
    if (!item || strip_y >= item->height) return;
    blitScaled(destination, *item, 0, strip_y, item->width, item->height - strip_y,
               x, y + strip_y, item->width, item->height - strip_y);
}

void blitBlueBorder(PshImage& destination, const MenuSpritePack& sprites,
                    const char* tag, int x, int y) {
    const PshImage* item = sprite(sprites, tag);
    if (!item) return;
    PshImage tinted = *item;
    for (std::size_t at = 0; at < tinted.rgba.size(); at += 4) {
        if (tinted.rgba[at + 3] == 0) continue;
        const int light = std::max({tinted.rgba[at], tinted.rgba[at + 1], tinted.rgba[at + 2]});
        tinted.rgba[at] = static_cast<std::uint8_t>(light / 8);
        tinted.rgba[at + 1] = static_cast<std::uint8_t>(light / 5);
        tinted.rgba[at + 2] = static_cast<std::uint8_t>(std::min(255, 35 + light));
    }
    blitScaled(destination, tinted, 0, 0, tinted.width, tinted.height,
               x, y, tinted.width, tinted.height);
}

void blitJiggledSprite(PshImage& destination, const MenuSpritePack& sprites,
                       const char* tag, int x, int y,
                       std::uint32_t elapsed_ms) {
    const PshImage* item = sprite(sprites, tag);
    if (!item) return;
    for (int row = 0; row < item->height; ++row) {
        const int wave_x = static_cast<int>(std::lround(
            std::sin(elapsed_ms * 0.011 + row * 0.24) * 1.5));
        blitScaled(destination, *item, 0, row, item->width, 1,
                   x + wave_x, y + row, item->width, 1);
    }
}

struct CornerJumble {
    int top_left_x;
    int top_left_y;
    int top_right_x;
    int top_right_y;
    int bottom_left_x;
    int bottom_left_y;
    int bottom_right_x;
    int bottom_right_y;
};

void blitJumbledChunk(PshImage& destination, const PshImage& item,
                      int source_x, int source_width, int x, int y,
                      const CornerJumble& offsets) {
    if (source_width <= 0 || item.height <= 0) return;
    const int source_height = static_cast<int>(item.height);
    const int minimum_x = std::min({offsets.top_left_x, offsets.top_right_x,
                                    offsets.bottom_left_x, offsets.bottom_right_x});
    const int maximum_x = std::max({offsets.top_left_x, offsets.top_right_x,
                                    offsets.bottom_left_x, offsets.bottom_right_x});
    const int minimum_y = std::min({offsets.top_left_y, offsets.top_right_y,
                                    offsets.bottom_left_y, offsets.bottom_right_y});
    const int maximum_y = std::max({offsets.top_left_y, offsets.top_right_y,
                                    offsets.bottom_left_y, offsets.bottom_right_y});
    const int width_denominator = std::max(1, source_width - 1);
    const int height_denominator = std::max(1, source_height - 1);
    for (int destination_y = minimum_y;
         destination_y < source_height + maximum_y; ++destination_y) {
        const double vertical = std::clamp(
            static_cast<double>(destination_y) / height_denominator, 0.0, 1.0);
        for (int destination_x = minimum_x;
             destination_x < source_width + maximum_x; ++destination_x) {
            const double horizontal = std::clamp(
                static_cast<double>(destination_x) / width_denominator, 0.0, 1.0);
            const double top_x = offsets.top_left_x +
                (offsets.top_right_x - offsets.top_left_x) * horizontal;
            const double bottom_x = offsets.bottom_left_x +
                (offsets.bottom_right_x - offsets.bottom_left_x) * horizontal;
            const double left_y = offsets.top_left_y +
                (offsets.bottom_left_y - offsets.top_left_y) * vertical;
            const double right_y = offsets.top_right_y +
                (offsets.bottom_right_y - offsets.top_right_y) * vertical;
            const int sample_x = static_cast<int>(std::lround(
                destination_x - (top_x + (bottom_x - top_x) * vertical)));
            const int sample_y = static_cast<int>(std::lround(
                destination_y - (left_y + (right_y - left_y) * horizontal)));
            if (sample_x < 0 || sample_x >= source_width ||
                sample_y < 0 || sample_y >= source_height)
                continue;
            const std::size_t from =
                (static_cast<std::size_t>(sample_y) * item.width +
                 source_x + sample_x) * 4;
            if (item.rgba[from + 3] == 0) continue;
            putPixel(destination, x + source_x + destination_x,
                     y + destination_y, item.rgba[from], item.rgba[from + 1],
                     item.rgba[from + 2]);
        }
    }
}

void blitJumbledTitleSprite(PshImage& destination,
                            const MenuSpritePack& sprites,
                            const char* tag, int x, int y,
                            std::uint32_t elapsed_ms, const int16_t* title_corners = nullptr) {
    const PshImage* item = sprite(sprites, tag);
    if (!item) return;

    if(title_corners) {
        drawFrontendTitle(destination,*item,title_corners);
        return;
    }

    // Native approximation, NOT recovered title animation. 3186C replaces
    // sprites and34A5C updates UVs; neither proves this eight-frame pattern
    // or128-pixel split. Actual32BF0/32BF8 motion uses per-corner RNG and
    // alternating objects (see recovered/frontend_title.c). Its presentation
    // clock/shared RNG/quad integration is pending; keep this limitation explicit.
    static constexpr std::array<CornerJumble, 8> frames{{
        { 0,  0,  1, -1, -1,  1,  0,  0},
        {-1,  1,  2,  0,  0, -1, -1,  1},
        { 1, -1, -1,  1, -2,  0,  1,  1},
        {-2,  0,  0,  2,  1, -1,  2,  0},
        { 0,  2,  2, -1, -1,  0,  0,  1},
        { 1,  0, -2, -1,  0,  2,  1,  0},
        {-1, -1,  1,  1,  2,  0, -1,  2},
        { 2,  1,  0, -2, -1,  1,  1,  0},
    }};
    const CornerJumble& offsets = frames[(elapsed_ms / 75) % frames.size()];
    const int first_width = std::min(128, static_cast<int>(item->width));
    blitJumbledChunk(destination, *item, 0, first_width, x, y, offsets);
    if (first_width < item->width)
        blitJumbledChunk(destination, *item, first_width,
                         item->width - first_width, x, y, offsets);
}

void blitJiggledTitle(PshImage& destination, const MenuSpritePack& sprites,
                      std::uint32_t elapsed_ms) {
    blitJiggledSprite(destination, sprites, "ba09", 148, 10, elapsed_ms);
}

int scaledTextWidth(const PshFont& font, const std::string& text, int scale,
                    int horizontal_percent = 100) {
    return font.textWidth(text) * scale * horizontal_percent / 100;
}

void drawText(PshImage& destination, const PshFont& font, const std::string& text,
              int x, int baseline_y, int scale, std::uint8_t tint_r = 255,
              std::uint8_t tint_g = 255, std::uint8_t tint_b = 255,
              bool per_letter_jiggle = false, std::uint32_t elapsed_ms = 0,
              int horizontal_percent = 100) {
    int cursor = x;
    int letter = 0;
    for (char character : text) {
        if (character == ' ') {
            cursor += font.spaceWidth() * scale * horizontal_percent / 100;
            ++letter;
            continue;
        }
        const PshGlyph* glyph = font.glyph(character);
        if (!glyph) {
            cursor += font.spaceWidth() * scale;
            ++letter;
            continue;
        }
        const int jiggle = per_letter_jiggle
            ? static_cast<int>(std::lround(std::sin(elapsed_ms * 0.012 + letter * 0.85) * 2.0))
            : 0;
        const int top = baseline_y - glyph->center_y * scale + jiggle;
        for (int gy = 0; gy < glyph->height; ++gy) {
            for (int gx = 0; gx < glyph->width; ++gx) {
                const std::size_t from =
                    (static_cast<std::size_t>(gy) * glyph->width + gx) * 4;
                if (glyph->rgba[from + 3] == 0) continue;
                const int luminance = std::max({glyph->rgba[from], glyph->rgba[from + 1],
                                                glyph->rgba[from + 2]});
                const auto tint = [luminance](std::uint8_t value) {
                    return static_cast<std::uint8_t>(value * luminance / 255);
                };
                for (int py = 0; py < scale; ++py)
                    for (int px = 0; px < scale; ++px)
                        putPixel(destination,
                                 cursor + (gx * scale + px) * horizontal_percent / 100,
                                 top + gy * scale + py,
                                 tint(tint_r), tint(tint_g), tint(tint_b));
            }
        }
        cursor += std::max(0, static_cast<int>(glyph->width) - font.kerning()) *
                  scale * horizontal_percent / 100;
        ++letter;
    }
}

// Controller records in ZFONT1.PSH are already fully colored UI artwork, not
// ordinary monochrome letters. Preserve their decoded palette instead of
// passing them through drawText's luminance tint path.
void drawNativeControlGlyph(PshImage& destination, const PshFont& font,
                            unsigned char character, int x, int baseline_y) {
    const PshGlyph* glyph = font.glyph(static_cast<char>(character));
    if (!glyph) return;
    const int top = baseline_y - glyph->center_y;
    for (int gy = 0; gy < glyph->height; ++gy) {
        for (int gx = 0; gx < glyph->width; ++gx) {
            const std::size_t from =
                (static_cast<std::size_t>(gy) * glyph->width + gx) * 4;
            if (glyph->rgba[from + 3] == 0) continue;
            putPixel(destination, x + gx, top + gy,
                     glyph->rgba[from], glyph->rgba[from + 1],
                     glyph->rgba[from + 2]);
        }
    }
}

void drawCenteredText(PshImage& destination, const PshFont& font,
                      const std::string& text, int center_x, int baseline_y,
                      int scale, std::uint8_t r = 255, std::uint8_t g = 255,
                      std::uint8_t b = 255, bool jiggle = false,
                      std::uint32_t elapsed_ms = 0,
                      int horizontal_percent = 100) {
    drawText(destination, font, text,
             center_x - scaledTextWidth(font, text, scale, horizontal_percent) / 2,
             baseline_y, scale, r, g, b, jiggle, elapsed_ms, horizontal_percent);
}

const char* rosterCategoryLabel(int category) noexcept {
    static constexpr std::array<const char*, 6> labels{
        "player attributes", "player ratings", "95/96 season",
        "95/96 playoffs", "current season", "current playoff"};
    return category >= 0 && category < static_cast<int>(labels.size())
        ? labels[static_cast<std::size_t>(category)] : "";
}

const char* rosterDisplayLabel(int display) noexcept {
    static constexpr std::array<const char*, 56> labels{
        "first name", "nickname", "birthdate", "birthplace", "height",
        "weight", "hand", "school", "years pro", "draft year",
        "drafted by", "drafted", "overall", "acquired how?", "acquired from?",
        "overall rating", "field goals", "3 point FGs", "free throws",
        "dunking", "stealing", "blocking", "def. awareness", "agility",
        "off. rebounds", "def. rebounds", "jumping", "strength",
        "ball control", "off. awareness", "speed", "dribbling",
        "games played/started", "points & pts. per game",
        "minutes played & per game", "field goals", "3 point FGs",
        "free throws", "off/def rebounds", "rebounds & per game",
        "blocks & per game", "steals & per game", "assists & per game",
        "fouls & ejections", "games played/started",
        "points & pts. per game", "minutes played & per game", "field goals",
        "3 point FGs", "free throws", "off/def rebounds",
        "rebounds & per game", "blocks & per game", "steals & per game",
        "assists & per game", "fouls"};
    return display >= 0 && display < static_cast<int>(labels.size())
        ? labels[static_cast<std::size_t>(display)] : "";
}

struct RosterDisplayValues {
    std::string first;
    std::string second;
    bool paired = false;
};

RosterDisplayValues rosterDisplayValues(const PlayerRecord& player,
                                         const RosterDatabase& database,
                                         int category, int display) {
    if (category == 0)
        return {database.playerAttribute(player, static_cast<std::size_t>(display)), {}, false};
    if (category == 1) {
        if (display == 15) return {std::to_string(player.overallRating()), {}, false};
        const int rating = display - 16;
        return rating >= 0 && rating < 16
            ? RosterDisplayValues{std::to_string(player.ratings[static_cast<std::size_t>(rating)]), {}, false}
            : RosterDisplayValues{};
    }

    static constexpr std::array<std::int16_t, 12> first_fields{
        22, 6, 8, 0, 1, 2, 15, 17, 18, 19, 20, 21};
    static constexpr std::array<std::int16_t, 12> second_fields{
        23, 32, 34, 43, 44, 45, 16, 37, 38, 39, 40, 25};
    const int first_display = category >= 4 ? 44 : 32;
    const int offset = display - first_display;
    if (offset < 0 || offset >= static_cast<int>(first_fields.size())) return {};
    PlayerStatPeriod period = PlayerStatPeriod::Season1995_96;
    if (category == 3) period = PlayerStatPeriod::Playoffs1995_96;
    else if (category == 4) period = PlayerStatPeriod::CurrentSeason;
    else if (category == 5) period = PlayerStatPeriod::CurrentPlayoffs;
    const StatLine& stats = player.stats(period);
    const std::int16_t second_field = category >= 4 && offset == 11
        ? static_cast<std::int16_t>(-1) : second_fields[static_cast<std::size_t>(offset)];
    return {stats.format(first_fields[static_cast<std::size_t>(offset)]),
            second_field >= 0 ? stats.format(second_field) : std::string{},
            second_field >= 0};
}

} // namespace

void MainMenu::reset() noexcept {
    row_ = MenuRow::GameOptions;
    option_ = 0;
    button_ = 0;
}

void MainMenu::setActiveUserProfiles(int count) noexcept {
    active_user_profiles_ = std::clamp(count, 0, 20);
    if (!buttonEnabled(button_)) button_ = 2;
}

bool MainMenu::buttonEnabled(int index) const noexcept {
    return index >= 0 && index < 5 && (index != 3 || active_user_profiles_ > 0);
}

bool MainMenu::moveHorizontal(int direction) noexcept {
    if (!direction) return false;
    if (row_ == MenuRow::GameOptions) {
        const int previous = option_;
        option_ = std::clamp(option_ + (direction < 0 ? -1 : 1), 0, 3);
        return option_ != previous;
    }
    const int previous = button_;
    const int step = direction < 0 ? -1 : 1;
    int candidate = button_;
    do {
        candidate = std::clamp(candidate + step, 0, 4);
        if (candidate == button_) break;
    } while (!buttonEnabled(candidate));
    button_ = candidate;
    return button_ != previous;
}

bool MainMenu::moveVertical(int direction) noexcept {
    const MenuRow target = direction > 0 ? MenuRow::FrontendButtons : MenuRow::GameOptions;
    if (target == row_) return false;
    row_ = target;
    return true;
}

bool MainMenu::hover(int psx_x, int psx_y) noexcept {
    if (psx_y >= 68 && psx_y < 190) {
        int best = 0;
        for (int i = 1; i < 4; ++i)
            if (std::abs(psx_x - kCardCenters[i]) < std::abs(psx_x - kCardCenters[best])) best = i;
        const bool changed = row_ != MenuRow::GameOptions || option_ != best;
        row_ = MenuRow::GameOptions;
        option_ = best;
        return changed;
    }
    if (psx_y >= 198 && psx_y < 232) {
        int best = 0;
        for (int i = 1; i < 5; ++i)
            if (std::abs(psx_x - kButtonCenters[i]) < std::abs(psx_x - kButtonCenters[best])) best = i;
        if (!buttonEnabled(best)) return false;
        const bool changed = row_ != MenuRow::FrontendButtons || button_ != best;
        row_ = MenuRow::FrontendButtons;
        button_ = best;
        return changed;
    }
    return false;
}

int MainMenu::selection() const noexcept {
    return row_ == MenuRow::GameOptions ? option_ : button_;
}

const char* MainMenu::selectedLabel() const noexcept {
    return row_ == MenuRow::GameOptions ? kOptionNames[option_] : kButtonNames[button_];
}

PshImage renderGameSetupMenu(const MainMenu& menu, const PshImage& title_source,
                             const PshFont& font, const MenuSpritePack& sprites,
                             const MenuCardPack& cards,
                             std::uint32_t elapsed_ms) {
    PshImage image;
    image.width = kWidth;
    image.height = kHeight;
    image.tag = "GSET";
    image.rgba.resize(static_cast<std::size_t>(kWidth) * kHeight * 4);

    if (!sprites.empty()) {
        blitAt(image, sprites, "Bkge", 0, 0);
        blitAt(image, sprites, "Bkgf", 128, 0);
        blitAt(image, sprites, "Bkgg", 256, 0);
        blitAt(image, sprites, "Bkgh", 384, 0);
    } else {
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const int grid = ((x / 24) ^ (y / 18)) & 1;
                putPixel(image, x, y, static_cast<std::uint8_t>(2 + grid * 3),
                         static_cast<std::uint8_t>(5 + grid * 5),
                         static_cast<std::uint8_t>(18 + (x + y) % 27));
            }
        }
        fillRect(image, 7, 7, 505, 8, 3, 5, 22);
        for (int y = 7; y < 198; ++y)
            fillRect(image, 8, y, 504, y + 1, 2, 5,
                     static_cast<std::uint8_t>(28 + (y * 3) % 30));
        outlineRect(image, 5, 5, 507, 235, 4, 14, 15, 80);
        outlineRect(image, 9, 9, 503, 231, 1, 36, 37, 126);
        fillRect(image, 10, 198, 502, 231, 4, 5, 18);
    }

    if (!sprites.empty()) {
        for (const auto& border : std::array<std::tuple<const char*, int, int>, 12>{{
            {"brte",0,5},{"brtf",128,5},{"brtg",256,5},{"brth",384,5},
            {"brle",0,65},{"brri",476,65},{"brba",0,185},{"brbb",128,185},
            {"brbc",256,185},{"brbd",384,185},{"brle",0,65},{"brri",476,65}}})
            blitBlueBorder(image, sprites, std::get<0>(border),
                           std::get<1>(border), std::get<2>(border));
        blitJiggledTitle(image, sprites, elapsed_ms);
        blitAt(image, sprites, "XXL1", 30, 16);
        blitAt(image, sprites, "XXR2", 404, 16);
    } else {
        blitScaled(image, title_source, 35, 20, 350, 155, 13, 10, 112, 58);
        drawCenteredText(image, font, "ea", 462, 25, 2, 210, 30, 35);
        drawCenteredText(image, font, "sports", 462, 48, 1, 185, 195, 225);
        drawCenteredText(image, font, "game setup", 273, 32, 2,
                         255, 218, 20, true, elapsed_ms);
    }

    if (!sprites.empty()) {
        constexpr std::array<int, 4> card_x{50, 150, 260, 350};
        constexpr std::array<int, 4> card_y{85, 80, 80, 85};
        constexpr std::array<const char*, 4> initial_cards{
            "c00a", "c05a", "c09a", "c13a"};
        constexpr std::array<const char*, 4> selected_overlays{
            "c04a", "c08a", "c12a", "c17a"};
        constexpr std::array<int, 4> portrait_x{61, 156, 272, 363};
        constexpr std::array<int, 4> portrait_y{99, 93, 94, 102};
        for (int i = 0; i < 4; ++i) {
            // FEONLY 0x80031F48 resolves each flags=0x20 blk1 record to one
            // unique 69x63 SHPP image from ZCARD.BIN. PS1 ordering-table
            // insertion makes the later plate records composite over it,
            // clipping the portrait beneath the header and value labels.
            if (!cards[static_cast<std::size_t>(i)].rgba.empty())
                blitScaled(image, cards[static_cast<std::size_t>(i)],
                           0, 0, 69, 63,
                           portrait_x[i], portrait_y[i], 69, 63);
            blitAt(image, sprites, initial_cards[i], card_x[i], card_y[i]);
            if (menu.row() == MenuRow::GameOptions && menu.selection() == i)
                blitAt(image, sprites, selected_overlays[i], card_x[i], card_y[i]);
        }
        constexpr std::array<const char*, 5> off{"o15a","o04a","o03a","o06a","o05a"};
        constexpr std::array<const char*, 5> on{"o15b","o04b","o03b","o06b","o05b"};
        constexpr std::array<int, 5> button_x{50,130,220,320,410};
        constexpr std::array<int, 5> button_y{198,198,198,200,198};
        for (int i = 0; i < 5; ++i) {
            if (!menu.buttonEnabled(i))
                blitDisabled(image, sprites, off[i], button_x[i], button_y[i]);
            else
                blitAt(image, sprites,
                       menu.row() == MenuRow::FrontendButtons && menu.selection() == i
                           ? on[i] : off[i], button_x[i], button_y[i]);
        }
        blitAt(image, sprites, "help", 235, 217);
        return image;
    }

    for (int i = 0; i < 4; ++i) {
        const int center = kCardCenters[i];
        const bool selected = menu.row() == MenuRow::GameOptions && menu.selection() == i;
        const std::uint8_t edge_r = selected ? 220 : 84;
        const std::uint8_t edge_g = selected ? 175 : 86;
        const std::uint8_t edge_b = selected ? 12 : 91;
        fillRect(image, center - 42, 70, center + 42, 190, 20, 21, 26);
        outlineRect(image, center - 43, 69, center + 43, 191,
                    selected ? 4 : 3, edge_r, edge_g, edge_b);
        drawCenteredText(image, font, kOptionNames[i], center, 83, 1,
                         selected ? 255 : 235, selected ? 235 : 235,
                         selected ? 120 : 235, false, 0, 86);
        drawCenteredText(image, font, kOptionValues[i], center, 174, 1,
                         245, 245, 245, false, 0, 82);
    }

    for (int i = 0; i < 5; ++i) {
        const bool selected = menu.row() == MenuRow::FrontendButtons && menu.selection() == i;
        const int center = kButtonCenters[i];
        fillRect(image, center - 35, 202, center + 35, 222,
                 selected ? 70 : 25, selected ? 57 : 25, selected ? 12 : 29);
        outlineRect(image, center - 36, 201, center + 36, 223, selected ? 3 : 2,
                    selected ? 232 : 95, selected ? 193 : 95, selected ? 24 : 100);
        drawCenteredText(image, font, kButtonNames[i], center, 214, 1,
                         selected ? 255 : 220, selected ? 235 : 220,
                         selected ? 100 : 220);
    }
    drawCenteredText(image, font, "a - help 1", 256, 232, 1, 240, 220, 20);
    return image;
}

PshImage renderSettingsMenu(const SettingsMenu& menu,
                            const FrontendSettings& settings,
                            const PshFont& font,
                            const MenuSpritePack& sprites,
                            std::uint32_t elapsed_ms) {
    PshImage image;
    image.width = kWidth;
    image.height = kHeight;
    image.tag = menu.page() == FrontendPage::Rules ? "RULE" : "OPTS";
    image.rgba.assign(static_cast<std::size_t>(kWidth) * kHeight * 4, 0);

    blitAt(image, sprites, "Bkge", 0, 0);
    blitAt(image, sprites, "Bkgf", 128, 0);
    blitAt(image, sprites, "Bkgg", 256, 0);
    blitAt(image, sprites, "Bkgh", 384, 0);
    for (const auto& border : std::array<std::tuple<const char*, int, int>, 10>{{
        {"brte",0,5},{"brtf",128,5},{"brtg",256,5},{"brth",384,5},
        {"brle",0,65},{"brri",476,65},{"brbe",0,185},{"brbf",128,185},
        {"brbg",256,185},{"brbh",384,185}}})
        blitBlueBorder(image, sprites, std::get<0>(border),
                       std::get<1>(border), std::get<2>(border));
    blitAt(image, sprites, menu.page() == FrontendPage::Rules ? "ba25" : "ba14",
           menu.page() == FrontendPage::Rules ? 200 : 180, 10);
    blitAt(image, sprites, "XXL1", 30, 16);
    blitAt(image, sprites, "XXR2", 404, 16);
    blitAt(image, sprites, "help", 235, 217);

    constexpr std::array<const char*, 15> rule_labels{
        "defensive fouls", "offensive fouls", "foul out", "out of bounds",
        "backcourt", "traveling", "goaltending", "illegal defense",
        "3 in the key", "5 second inbounding", "10 second half court",
        "shot clock", "fatigue", "injuries", "current style"};
    constexpr std::array<const char*, 11> option_labels{
        "sound", "music volume", "speech volume", "SF/X volume", "crowd volume",
        "automatic replay", "keep scores close", "slow motion dunks",
        "player indicator", "display indicator", "score overlay"};
    const bool rules = menu.page() == FrontendPage::Rules;
    const int label_x = rules ? 122 : 110;
    const int value_x = rules ? 210 : 230;
    const int first_y = rules ? 97 : 82;
    for (int visible = 0; visible < menu.visibleCount(); ++visible) {
        const int index = menu.firstVisible() + visible;
        if (index >= menu.count()) break;
        const int y = first_y + visible * 16;
        const bool selected = index == menu.selected();
        const std::uint8_t r = selected ? 255 : 205;
        const std::uint8_t g = selected ? 218 : 205;
        const std::uint8_t b = selected ? 35 : 205;
        const char* label = rules ? rule_labels[static_cast<std::size_t>(index)]
                                  : option_labels[static_cast<std::size_t>(index)];
        const std::string value = rules ? settings.ruleValue(index)
                                        : settings.optionValue(index);
        const int jiggle = selected ? static_cast<int>(std::lround(
            std::sin(elapsed_ms * 0.014) * 1.0)) : 0;
        // The generic FE text renderer uses its condensed menu width for these
        // columns. The recovered x fields (122/210 Rules, 110/230 Options) are
        // absolute left edges, not a free-form spacing hint.
        drawText(image, font, label, label_x + jiggle, y, 1, r, g, b,
                 selected, elapsed_ms, 66);
        const bool level_meter = (rules && index < 2) || (!rules && index >= 1 && index <= 5);
        if (level_meter) {
            const int level = rules ? settings.rule(index) : settings.option(index);
            for (int segment = 0; segment < 10; ++segment) {
                const bool lit = segment <= level;
                fillRect(image, value_x + segment * 8, y - 4,
                         value_x + segment * 8 + 6, y + 1,
                         lit ? r : 45, lit ? g : 48, lit ? b : 70);
            }
        } else {
            drawText(image, font, value, value_x + jiggle, y, 1, r, g, b,
                     false, 0, rules ? 72 : 66);
        }
    }

    // The recovered generic list displays scroll cues only while more rows
    // exist outside its six/seven-row viewport.
    if (menu.firstVisible() > 0) {
        for (int row = 0; row < 5; ++row)
            fillRect(image, 445 - row, 69 + row, 456 + row, 70 + row, 245, 210, 30);
    }
    if (menu.firstVisible() + menu.visibleCount() < menu.count()) {
        for (int row = 0; row < 5; ++row)
            fillRect(image, 445 + row, 193 + row, 456 - row, 194 + row, 245, 210, 30);
    }
    return image;
}

void RecoveredBottomMenu::open(FrontendPage page) noexcept {
    page_ = page;
    selected_ = 0;
}

void RecoveredBottomMenu::setSelected(int selected) noexcept {
    const int candidate = std::clamp(selected, 0, count() - 1);
    if (enabled(candidate)) selected_ = candidate;
}

void RecoveredBottomMenu::setRosterCapabilities(bool roster_modified,
                                                bool injuries_present) noexcept {
    roster_modified_ = roster_modified;
    injuries_present_ = injuries_present;
    if (!enabled(selected_)) selected_ = 4;
}

int RecoveredBottomMenu::count() const noexcept {
    if (page_ == FrontendPage::Rosters) return 8;
    if (page_ == FrontendPage::Card) return 3;
    return 1;
}

const char* RecoveredBottomMenu::selectedLabel() const noexcept {
    static constexpr std::array<const char*, 8> roster_labels{
        "trade players", "sign free agent", "release players", "reset rosters",
        "view rosters", "re-order rosters", "create players", "player injuries"};
    static constexpr std::array<const char*, 3> card_labels{
        "save settings", "load settings", "load game"};
    if (page_ == FrontendPage::Rosters) return roster_labels[static_cast<std::size_t>(selected_)];
    if (page_ == FrontendPage::Card) return card_labels[static_cast<std::size_t>(selected_)];
    return "team stats";
}

bool RecoveredBottomMenu::enabled(int index) const noexcept {
    if (index < 0 || index >= count()) return false;
    if (page_ != FrontendPage::Rosters) return true;
    // FUN_80057C48 enables Reset only after FUN_80058104 detects roster data
    // differing from the original defaults (unless a special FE state forces it
    // off). FUN_80057A98 also requires an active context before counting the
    // 536 recovered injury bytes; at least one must be non-zero.
    if (index == 3) return roster_modified_;
    if (index == 1) return sign_available_;
    if (index == 2) return release_available_;
    if (index == 7) return injuries_present_;
    return true;
}

bool RecoveredBottomMenu::move(int horizontal, int vertical) noexcept {
    const int previous = selected_;
    if (page_ == FrontendPage::Rosters) {
        if (horizontal) {
            const int direction = horizontal < 0 ? -1 : 1;
            const int row_start = selected_ < 4 ? 0 : 4;
            const int row_end = row_start + 3;
            for (int candidate = selected_ + direction;
                 candidate >= row_start && candidate <= row_end;
                 candidate += direction) {
                if (enabled(candidate)) {
                    selected_ = candidate;
                    break;
                }
            }
        }
        if (vertical) {
            const int candidate = selected_ + (vertical < 0 ? -4 : 4);
            if (enabled(candidate)) selected_ = candidate;
        }
    } else if (page_ == FrontendPage::Card && horizontal) {
        selected_ = std::clamp(selected_ + (horizontal < 0 ? -1 : 1), 0, 2);
    }
    return selected_ != previous;
}

bool RecoveredBottomMenu::hover(int psx_x, int psx_y) noexcept {
    int candidate = selected_;
    if (page_ == FrontendPage::Rosters && psx_y >= 63 && psx_y < 198) {
        const int column = std::clamp((psx_x - 45) / 105, 0, 3);
        const int row = psx_y >= 132 ? 1 : 0;
        candidate = row * 4 + column;
    } else if (page_ == FrontendPage::Card && psx_y >= 75 && psx_y < 195) {
        candidate = std::clamp((psx_x - 75) / 125, 0, 2);
    } else {
        return false;
    }
    if (!enabled(candidate)) return false;
    const bool changed = candidate != selected_;
    selected_ = candidate;
    return changed;
}

void RosterViewer::clamp(const RosterDatabase& database) noexcept {
    if(team_index_==29 && !free_agent_descriptor_.roster.empty()) {
        player_index_=(std::min)(player_index_,free_agent_descriptor_.roster.size()-1);
        if(player_index_<first_visible_player_)first_visible_player_=player_index_;
        if(player_index_>=first_visible_player_+6)first_visible_player_=player_index_-5;
        return;
    }
    if (database.teams().empty()) {
        team_index_ = player_index_ = first_visible_player_ = 0;
        return;
    }
    team_index_ = (std::min)(team_index_, database.teams().size() - 1);
    const auto& roster = database.teams()[team_index_].roster;
    if (roster.empty()) {
        player_index_ = first_visible_player_ = 0;
        return;
    }
    player_index_ = (std::min)(player_index_, roster.size() - 1);
    first_visible_player_ = (std::min)(first_visible_player_, roster.size() - 1);
    if (player_index_ < first_visible_player_)
        first_visible_player_ = player_index_;
    else if (player_index_ >= first_visible_player_ + 6)
        first_visible_player_ = player_index_ - 5;
}

void RosterViewer::open(const RosterDatabase& database) noexcept {
    RosterViewerRunContext context;
    context.saved = {static_cast<std::int16_t>(player_index_),
                     static_cast<std::int16_t>(first_visible_player_),
                     static_cast<std::int16_t>(team_index_)};
    runViewer(database, context);
}

void RosterViewer::constructViewer(const RosterDatabase& database,
                                   RosterViewerConstructionContext context) noexcept {
    nba97_semantic_trace_record(0x800590B8u);
    clamp(database);

    // DAT_800A4088 contains six 12-byte descriptor records. The constructor
    // copies each record's first short on every invocation.
    display_by_category_ = {0, 15, 32, 32, 44, 44};
    construction_layer_limit_ = 3;
    category_ = 2;
    if (context.special_stat_layer != 0 && context.special_roster_active) {
        category_ = context.special_stat_layer + 3;
        construction_layer_limit_ = category_;
    }
    if (category_ < 0 || category_ >= static_cast<int>(display_by_category_.size()))
        category_ = 2;

    construction_descriptor_count_ = 28;
    constructed_descriptor_index_ =
        display_by_category_[static_cast<std::size_t>(category_)];
    construction_object_flags_.fill(context.object_state_flag);
    construction_controller_phase_ = 0;
    layer_four_restricted_ = context.layer_four_restricted;
    stat_descriptor_last_index_ = category_ == 0 ? 13 : category_ == 1 ? 16 : 23;
    last_stat_layer_change_ = {};
    last_stat_layer_change_.previous_layer = category_;
    last_stat_layer_change_.current_layer = category_;
    last_stat_layer_change_.descriptor_table = category_ < 2 ? category_
        : category_ < 4 ? 2 : 3;
    last_stat_layer_change_.descriptor_last_index = stat_descriptor_last_index_;
    last_player_cycle_ = {};
    player_card_run_state_ = {};

    // Native state ownership for FUN_80039D88(&DAT_800A436C, 0x12,
    // 0x10004, 0x11, 7). The opaque PS1 object pool is replaced by the
    // viewer instance; the binding itself remains explicit and testable.
    construction_controller_ = {0x12, 0x10004u, 0x11, 7, true};
}

void RosterViewer::construct(const RosterDatabase& database,
                             RosterViewerConstructionContext context) noexcept {
    constructViewer(database, context);
}

void RosterViewer::runViewer(const RosterDatabase& database,
                             RosterViewerRunContext context) noexcept {
    nba97_semantic_trace_record(0x800592C4u);
    if (context.saved.player_index == -1) {
        context.saved.player_index = 0;
        context.saved.first_visible_player = 0;
    }
    if (context.saved.team_index == 29) {
        context.saved.team_index = context.special_stat_layer == 2
            ? static_cast<std::int16_t>(context.active_team_index) : 3;
    }
    player_index_ = context.saved.player_index < 0
        ? 0u : static_cast<std::size_t>(context.saved.player_index);
    first_visible_player_ = context.saved.first_visible_player < 0
        ? 0u : static_cast<std::size_t>(context.saved.first_visible_player);
    team_index_ = context.saved.team_index < 0
        ? 0u : static_cast<std::size_t>(context.saved.team_index);

    mode_ = RosterViewMode::TeamRoster;
    help_visible_ = false;
    first_visible_player_stat_ = 0;
    constructViewer(database, {context.special_stat_layer,
                               context.special_roster_active, false,
                               context.layer_four_restricted});

    // FUN_8003D930(DAT_80022088, 0x10, ..., FUN_80059034, 0).
    // Input arrives incrementally in the native window loop, but the state
    // and draw-callback boundary remain explicit here.
    run_input_state_ = 0x10;
    run_cancel_sentinel_ = 0x100;
    run_draw_callback_bound_ = true;
    entry_team_index_ = team_index_;
    entry_player_index_ = player_index_;
    entry_first_visible_player_ = first_visible_player_;
    palette_from_team_index_ = team_index_;
    palette_transition_start_ms_ = 0;
    scroll_from_first_player_ = first_visible_player_;
    scroll_transition_start_ms_ = 0;
}

bool RosterViewer::cycleCategory(int direction) noexcept {
    if (!direction) return false;
    nba97_semantic_trace_record(0x80059610u);

    // FUN_80059610 reads 0x1000 for L2; every other accepted invocation is
    // the forward/R2 path. It wraps against the constructor's inclusive
    // layer limit, not the six-entry backing descriptor array.
    RosterStatLayerChangeState change;
    change.input_mask = direction < 0 ? 0x1000 : 0x2000;
    change.previous_layer = category_;
    do {
        category_ = direction < 0
            ? (category_ == 0 ? construction_layer_limit_ : category_ - 1)
            : (category_ == construction_layer_limit_ ? 0 : category_ + 1);
        if (category_ == 4 && layer_four_restricted_)
            change.skipped_layer_four = true;
    } while (category_ == 4 && layer_four_restricted_);

    // FUN_8003B26C formats/redraws a typed text object. Object 0x1B is the
    // shared stat-layer label, NOT an audio slot (confirmed by Compare's
    // descriptor table and the callee itself).
    change.layer_label_object = 0x1b;
    change.current_layer = category_;
    change.descriptor_table = category_ < 2 ? category_
        : category_ < 4 ? 2 : 3;
    const int previous_last_index = stat_descriptor_last_index_;
    stat_descriptor_last_index_ = category_ == 0 ? 13 : category_ == 1 ? 16 : 23;
    change.descriptor_last_index = stat_descriptor_last_index_;
    change.descriptor_extent_changed = previous_last_index != stat_descriptor_last_index_;

    // FUN_800594F0 replaces every stat-label descriptor. A changed extent
    // additionally resets/animates the primary object group. FUN_80059610
    // refreshes every visible row even when the extent remains unchanged.
    change.primary_animation_reset = change.descriptor_extent_changed;
    change.primary_refresh_count = stat_visible_row_count_;
    change.secondary_layout = stat_layout_id_ == 0x23;
    change.secondary_animation_reset =
        change.secondary_layout && change.descriptor_extent_changed;
    change.secondary_refresh_count =
        change.secondary_layout ? stat_visible_row_count_ : 0;

    // The original temporarily sets the controller page byte to zero around
    // FUN_8003A224, then restores it exactly.
    change.saved_controller_page = stat_controller_page_;
    stat_controller_page_ = 0;
    change.controller_page_zeroed_for_rebuild = true;
    change.layout_rebuilt = true;
    stat_controller_page_ = change.saved_controller_page;
    last_stat_layer_change_ = change;
    if (change.descriptor_extent_changed) first_visible_player_stat_ = 0;
    return true;
}

std::size_t RosterViewer::playerStatCount() const noexcept {
    if (category_ == 0) return 14;
    if (category_ == 1) return 17;
    return 24;
}

bool RosterViewer::cycleDisplay(int direction) noexcept {
    if (!direction) return false;
    static constexpr std::array<int, 6> minimum{0, 15, 32, 32, 44, 44};
    static constexpr std::array<int, 6> maximum{14, 31, 43, 43, 55, 55};
    int& display = display_by_category_[static_cast<std::size_t>(category_)];
    if (direction < 0)
        display = display == minimum[static_cast<std::size_t>(category_)]
            ? maximum[static_cast<std::size_t>(category_)] : display - 1;
    else
        display = display == maximum[static_cast<std::size_t>(category_)]
            ? minimum[static_cast<std::size_t>(category_)] : display + 1;
    return true;
}

bool RosterViewer::scanTeam(int direction, const RosterDatabase& database,
                            std::uint32_t elapsed_ms) noexcept {
    if (!direction || database.teams().empty()) return false;
    // A Sign child carries descriptor29 as well as ordinary teams. 59ABC
    // wraps against the descriptor's bound, not a hard-coded team count.
    const std::size_t last=free_agent_descriptor_.roster.empty()?database.teams().size()-1:29;
    const std::size_t next_team = direction < 0
        ? (team_index_ == 0 ? last : team_index_ - 1)
        : (team_index_ >= last ? 0 : team_index_ + 1);
    const auto& roster = next_team==29?free_agent_descriptor_.roster:database.teams()[next_team].roster;
    // FUN_80059ABC / fragment 80059ACC keeps slots 0..14, resets larger
    // free-agent indices to zero, then walks BACK over signed -1 entries.
    // Do not reset the stat-list scroll: neither this callback nor 59808 does.
    // This path currently covers normal teams; free-agent/special eligibility
    // needs its own View asset/context integration, not a claim of full parity.
    std::size_t next_player = player_index_ > 14 ? 0 : player_index_;
    const auto occupied = [&](std::size_t slot) {
        // Catalogue loading/preparation already validates every live ID.
        // The original checks the -1 sentinel, not a second player lookup.
        return slot < roster.size() && roster[slot] != UINT16_MAX;
    };
    while (next_player && !occupied(next_player)) --next_player;
    // The original assumes a nonempty validated list. Refuse an empty native
    // list atomically instead of underflowing into the previous team's memory.
    if (!occupied(next_player)) return false;
    palette_from_team_index_ = team_index_;
    team_index_ = next_team;
    palette_transition_start_ms_ = elapsed_ms;
    player_index_ = next_player;
    clamp(database);
    return true;
}

const TeamRecord* RosterViewer::selectedTeam(const RosterDatabase& database) const noexcept {
    if(team_index_==29 && !free_agent_descriptor_.roster.empty()) return &free_agent_descriptor_;
    return team_index_ < database.teams().size() ? &database.teams()[team_index_] : nullptr;
}

const PlayerRecord* RosterViewer::selectedPlayer(const RosterDatabase& database) const noexcept {
    const TeamRecord* team = selectedTeam(database);
    return team && player_index_ < team->roster.size()
        ? database.player(team->roster[player_index_]) : nullptr;
}

bool RosterViewer::cyclePlayer(int direction, const RosterDatabase& database,
                               RosterPlayerCycleContext context) noexcept {
    if (!direction || mode_ != RosterViewMode::PlayerCard) return false;
    nba97_semantic_trace_record(0x80059928u);

    RosterPlayerCycleState change;
    change.input_mask = direction < 0 ? 8 : 4;
    change.previous_slot = player_index_;
    change.current_slot = player_index_;
    change.descriptor_page = context.active_page;

    // Descriptor29 is the free-agent list. Context+0x708 is derived count[29]
    // (0x6CE+29*2): when it is one, the original clears +0x1B and does not cycle.
    // The legacy adapter exposes that predicate as special_cycle_locked.
    if ((context.special_roster_descriptor && context.special_cycle_locked) ||
        (team_index_==29 && database.freeAgentCount()==1)) {
        change.blocked_special_roster = true;
        change.input_latch_cleared = true;
        last_player_cycle_ = change;
        return false;
    }

    const TeamRecord* team = selectedTeam(database);
    if (!team) {
        last_player_cycle_ = change;
        return false;
    }
    while (change.roster_count < team->roster.size() &&
           database.player(team->roster[change.roster_count]))
        ++change.roster_count;
    if (change.roster_count == 0) {
        last_player_cycle_ = change;
        return false;
    }

    if (direction < 0) {
        change.wrapped = player_index_ == 0;
        player_index_ = change.wrapped ? change.roster_count - 1 : player_index_ - 1;
    } else {
        change.wrapped = player_index_ + 1 == change.roster_count;
        player_index_ = change.wrapped ? 0 : player_index_ + 1;
    }
    change.current_slot = player_index_;
    const PlayerRecord* player = selectedPlayer(database);
    if (player) {
        change.resolved_player_id = player->id;
        change.global_player_id_updated = true;
    }

    // FUN_80039574(0, 2) presents two frontend frames before FUN_80059808
    // refreshes three page-dependent header descriptors and every visible
    // row descriptor.
    change.transition_frames = 2;
    change.header_refresh_start = context.active_page == 0 ? 0x18 : 0x1e;
    change.header_refresh_count = 3;
    change.visible_row_refresh_start = context.visible_row_start;
    change.visible_row_refresh_count = (std::max)(0, context.visible_row_count);

    // State 0x24 stops current speech and rebuilds the selected player's
    // available cool-fact choices. Crucially, FUN_80059928 never resets the
    // statistic-list scroll position.
    if (context.layout_id == 0x24) {
        change.cool_fact_stopped = true;
        change.player_card_refreshed = true;
    }
    change.stat_scroll_preserved = true;
    last_player_cycle_ = change;
    return true;
}

bool RosterViewer::move(int horizontal, int vertical, const RosterDatabase& database,
                        std::uint32_t elapsed_ms) noexcept {
    if (database.teams().empty()) return false;
    const std::size_t previous_team = team_index_;
    const std::size_t previous_player = player_index_;
    const std::size_t previous_first_visible = first_visible_player_;
    const std::size_t previous_stat = first_visible_player_stat_;
    bool player_cycle_processed = false;
    if (mode_ == RosterViewMode::TeamRoster && horizontal) {
        palette_from_team_index_ = team_index_;
        if (horizontal < 0) team_index_ = team_index_ == 0 ? database.teams().size() - 1 : team_index_ - 1;
        else team_index_ = (team_index_ + 1) % database.teams().size();
        palette_transition_start_ms_ = elapsed_ms;
    } else if (mode_ == RosterViewMode::PlayerCard) {
        if (horizontal) player_cycle_processed = cyclePlayer(horizontal, database);
        const std::size_t stat_count = playerStatCount();
        constexpr std::size_t visible_stats = 6;
        if (vertical < 0 && first_visible_player_stat_ > 0)
            --first_visible_player_stat_;
        else if (vertical > 0 && first_visible_player_stat_ + visible_stats < stat_count)
            ++first_visible_player_stat_;
    } else {
        const TeamRecord* team = selectedTeam(database);
        if (team && !team->roster.empty()) {
            if (vertical < 0 && player_index_ > 0)
                --player_index_;
            else if (vertical > 0 && player_index_ + 1 < team->roster.size() &&
                     database.player(team->roster[player_index_ + 1]))
                ++player_index_;
        }
    }
    clamp(database);
    if (first_visible_player_ != previous_first_visible) {
        scroll_from_first_player_ = previous_first_visible;
        scroll_transition_start_ms_ = elapsed_ms;
    }
    return player_cycle_processed || previous_team != team_index_ || previous_player != player_index_ ||
           previous_stat != first_visible_player_stat_;
}

bool RosterViewer::hover(int psx_x, int psx_y, const RosterDatabase& database) noexcept {
    if (mode_ != RosterViewMode::TeamRoster) return false;
    // FUN_800590B8 uses six generic-list rows at x=60, y=106 with a
    // 12-pixel pitch. Keep mouse selection on those same recovered rows.
    if (psx_y >= 100 && psx_y < 172 && psx_x >= 45 && psx_x < 440) {
        const TeamRecord* team = selectedTeam(database);
        if (!team || team->roster.empty()) return false;
        const std::size_t candidate = (std::min)(team->roster.size() - 1,
            first_visible_player_ + static_cast<std::size_t>((psx_y - 100) / 12));
        const bool changed = candidate != player_index_;
        player_index_ = candidate;
        clamp(database);
        return changed;
    }
    return false;
}

bool RosterViewer::openPlayerFromRoster(const RosterDatabase& database,
        RosterViewerPersistentState selection, PlayerCardRunContext context,const std::string& free_agent_name) {
    const bool free=selection.team_index==29;
    if (selection.team_index < 0 || selection.team_index>29 || (free && free_agent_name.empty()) ||
        selection.player_index < 0 || selection.player_index >= (free?100:15) || selection.first_visible_player < 0 ||
        selection.first_visible_player > (free?94:9) || selection.player_index < selection.first_visible_player ||
        selection.player_index >= selection.first_visible_player+6 || context.parent_active_page > 1 ||
        context.special_stat_layer < 0 || context.special_stat_layer > 2)
        return false;
    RosterViewer child;
    if(free || context.parent_frontend_state==14) {
        child.free_agent_descriptor_=TeamRecord({"",free_agent_name,"","",""});
        child.free_agent_descriptor_.id=29;
        for(auto id:database.freeAgentSlots()) {if(id==UINT16_MAX)break;child.free_agent_descriptor_.roster.push_back(id);}
    }
    child.team_index_=static_cast<std::size_t>(selection.team_index);
    child.player_index_=static_cast<std::size_t>(selection.player_index);
    child.first_visible_player_=static_cast<std::size_t>(selection.first_visible_player);
    child.palette_from_team_index_=child.entry_team_index_=child.team_index_;
    child.entry_player_index_=child.player_index_;
    child.scroll_from_first_player_=child.entry_first_visible_player_=child.first_visible_player_;
    if (!child.runPlayerCard(database,context)) return false;
    *this=child;
    return true;
}

bool RosterViewer::runPlayerCard(const RosterDatabase& database,
                                 PlayerCardRunContext context) noexcept {
    const PlayerRecord* player = selectedPlayer(database);
    if (!player) return false;
    nba97_semantic_trace_record(0x8005A538u);

    PlayerCardRunState run;
    // FUN_80030D14 / FUN_8003122C request the original private archives. The
    // native window owns actual local decoding; this state preserves the
    // recovered lifecycle without embedding or publishing their contents.
    run.portrait_archive_requested = true;
    run.cool_fact_archive_requested = true;
    run.portrait_index = "Z1PORT.IDX";
    run.portrait_archive = "Z1PORT.BIG";
    run.cool_fact_index = "Z1COOL.IDX";
    run.cool_fact_archive = "Z1COOL.BIG";

    // DAT_800A482C manager initialization.
    run.object_count = 0x1d;
    run.visible_row_count = 6;
    run.manager_byte_2 = 0x70;
    run.manager_short_20 = 0x36;
    run.manager_short_22 = 0xef;
    run.manager_mirror_flags = {1, 1};
    run.number_descriptor_bound = true;
    run.position_descriptor_bound = true;
    run.layout_id = 0x24;
    stat_layout_id_ = run.layout_id;
    stat_visible_row_count_ = run.visible_row_count;

    // FUN_8005A1EC chooses the normal layer 2/limit 3 pair or a special
    // layer+3 pair. Byte +0x2FC4 forces both to layer 5 only while special
    // mode is active.
    int layer = 2;
    int limit = 3;
    if (context.special_stat_layer != 0) {
        layer = context.special_stat_layer + 3;
        limit = layer;
        if (context.force_layer_five)
            layer = limit = 5;
    }
    category_ = layer;
    construction_layer_limit_ = limit;
    stat_descriptor_last_index_ = 0x17;
    run.current_stat_layer = layer;
    run.stat_layer_limit = limit;
    run.descriptor_last_index = 0x17;
    run.descriptor_end = 0x38;

    // For the 0x24 invocation, FUN_8005A074 selects the parent roster page,
    // initializes one team/player context, refreshes available cool facts,
    // and installs the recovered -1 sentinels.
    run.parent_roster_viewer_selected = context.parent_frontend_state == 0x10;
    run.parent_active_page = context.parent_active_page;
    run.selected_team = team_index_;
    run.selected_roster_slot = player_index_;
    run.selected_visible_row = player_index_ >= first_visible_player_
        ? player_index_ - first_visible_player_ : 0;
    run.selected_player_id = player->id;
    run.player_context_initialized = true;
    run.cool_fact_choices_refreshed = true;
    run.column_starts.fill(0);
    run.column_steps.fill(2);
    run.previous_fact_choice = -1;
    run.previous_fact_record = -1;

    // FUN_80039D88(&DAT_800A482C, 0x39, 0x10000, 0x10, 0x0D), followed by
    // the generic state -1 loop with distinct draw/action callbacks.
    run.controller = {0x39, 0x10000u, 0x10, 0x0d, true};
    run.frontend_state_during_loop = 0x11;
    run.input_state = -1;
    run.draw_callback = 0x8005A280u;
    run.action_callback = 0x8005A3FCu;
    run.input_loop_bound = true;
    run.frontend_state_after_loop = -1;
    run.teardown_scheduled = true;
    run.teardown_complete = false;

    mode_ = RosterViewMode::PlayerCard;
    first_visible_player_stat_ = 0;
    help_visible_ = false;
    player_card_run_state_ = run;
    return true;
}

void RosterViewer::activate(const RosterDatabase& database) noexcept {
    // FUN_80058F0C action 0x10 returns 2 for a valid slot.  The frontend then
    // pushes nested state 0x24 (FUN_8005A538) and returns to state 0x10.
    runPlayerCard(database);
}

void RosterViewer::returnToRoster() noexcept {
    if (player_card_run_state_.input_loop_bound) {
        player_card_run_state_.input_loop_bound = false;
        player_card_run_state_.teardown_complete = true;
    }
    mode_ = RosterViewMode::TeamRoster;
    first_visible_player_stat_ = 0;
    help_visible_ = false;
}

void RosterViewer::commit() noexcept {
    entry_team_index_ = team_index_;
    entry_player_index_ = player_index_;
    entry_first_visible_player_ = first_visible_player_;
}

void RosterViewer::cancel() noexcept {
    team_index_ = entry_team_index_;
    player_index_ = entry_player_index_;
    first_visible_player_ = entry_first_visible_player_;
    palette_from_team_index_ = team_index_;
    scroll_from_first_player_ = first_visible_player_;
}

void RosterViewer::finishRun(int result_code, int exit_status) noexcept {
    last_run_result_ = result_code;
    last_run_exit_status_ = exit_status;
    if (exit_status == run_cancel_sentinel_)
        cancel();
    else
        commit();
}

PshImage renderRecoveredBottomMenu(const RecoveredBottomMenu& menu,
                                   const PshFont& font,
                                   const MenuSpritePack& zset1,
                                   const MenuSpritePack& zset4,
                                   const MenuSpritePack& zset7,
                                   const RosterCardPack& roster_cards,
                                   std::uint32_t elapsed_ms,
                                   bool selected_overlay_visible) {
    const MenuSpritePack& sprites = menu.page() == FrontendPage::Rosters ? zset4 :
                                    menu.page() == FrontendPage::Users ? zset7 : zset1;
    if (menu.page() == FrontendPage::Rosters)
        nba97_semantic_trace_record(0x80057CE4u);
    PshImage image;
    image.width = kWidth;
    image.height = kHeight;
    image.tag = menu.page() == FrontendPage::Rosters ? "ROST" :
                menu.page() == FrontendPage::Users ? "USER" : "CARD";
    image.rgba.assign(static_cast<std::size_t>(kWidth) * kHeight * 4, 0);
    blitAt(image, sprites, "Bkge", 0, 0);
    blitAt(image, sprites, "Bkgf", 128, 0);
    blitAt(image, sprites, "Bkgg", 256, 0);
    blitAt(image, sprites, "Bkgh", 384, 0);
    for (const auto& border : std::array<std::tuple<const char*, int, int>, 10>{{
        {"brte",0,5},{"brtf",128,5},{"brtg",256,5},{"brth",384,5},
        {"brle",0,65},{"brri",476,65},{"brbe",0,185},{"brbf",128,185},
        {"brbg",256,185},{"brbh",384,185}}})
        blitBlueBorder(image, sprites, std::get<0>(border), std::get<1>(border), std::get<2>(border));
    blitAt(image, sprites, "XXL1", 30, 16);
    blitAt(image, sprites, "XXR2", 404, 16);
    blitAt(image, sprites, "help", 235, 217);

    if (menu.page() == FrontendPage::Rosters) {
        // ba24 follows the generic FUN_8003186C/FUN_80034A5C frontend title
        // deformation path. The original changes discrete corner records; it
        // never uses the old smooth scanline wave approximation.
        blitJumbledTitleSprite(image, sprites, "ba24", 166, 10, elapsed_ms);
        // FUN_80057CE4 gives each of the eight choices three consecutive
        // 0x6c-byte objects: normal plate, selected plate, then a flags=0x20
        // blk1 placeholder. FUN_80031F48 replaces that third object with a
        // unique 69x63 ZCARD image. The plates are already authored at their
        // display size; the old 2/3 scaling caused the bunched-up layout.
        // The recovered rows are four paired groups, not a flat grid. These
        // are the authored (x,y) records 16..39 from FEONLY's screen-9 table
        // at 0x80094ED4. In particular, each front plate begins only 35 pixels
        // below its back partner; treating the rows as a uniform grid leaves
        // a visibly incorrect gap between the paired cards.
        static constexpr std::array<int, 8> card_x{
            65, 165, 270, 365, 50, 150, 255, 345};
        static constexpr std::array<int, 8> card_y{
            81, 75, 75, 81, 116, 110, 110, 116};
        static constexpr std::array<int, 8> art_x{
            77, 171, 282, 379, 62, 156, 267, 359};
        static constexpr std::array<int, 8> art_y{
            95, 88, 89, 98, 130, 123, 124, 133};
        for (int i = 0; i < 8; ++i) {
            const int x = card_x[static_cast<std::size_t>(i)];
            const int y = card_y[static_cast<std::size_t>(i)];
            const bool selected = menu.selected() == i;
            const bool locked = !menu.enabled(i);
            const bool selected_phase = selected && selected_overlay_visible;
            std::string tag;
            if (locked && i == 3)
                tag = "c06r";
            else if (locked && i == 7)
                tag = "c14r";
            else
                tag = "c" + std::string(i * 2 < 10 ? "0" : "") +
                      std::to_string(i * 2 + (selected_phase ? 1 : 0)) + "d";
            // The image is inserted before its transparent plate, matching
            // the PS1 ordering-table composite. The eight blk1 descriptor
            // origins are individually authored; they are not one common
            // offset from their differently shaped plates.
            const auto& art = roster_cards[static_cast<std::size_t>(i)];
            if (!art.rgba.empty())
                blitScaled(image, art, 0, 0, art.width, art.height,
                           art_x[static_cast<std::size_t>(i)],
                           art_y[static_cast<std::size_t>(i)],
                           art.width, art.height);
            // FUN_8003F240 toggles between the normal and selected object; it
            // never removes the object's plate. Keeping the normal plate in
            // the off phase also contains the inserted ZCARD within its mask.
            blitAt(image, sprites, tag.c_str(), x, y);
        }
    } else if (menu.page() == FrontendPage::Card) {
        blitAt(image, sprites, "ba13", 132, 10);
        static constexpr std::array<const char*, 3> off{"c00g","c02g","c04g"};
        static constexpr std::array<const char*, 3> on{"c01g","c03g","c05g"};
        for (int i = 0; i < 3; ++i)
            blitAt(image, sprites, menu.selected() == i ? on[i] : off[i], 105 + i * 105, 82);
    } else {
        blitAt(image, sprites, "ba37", 154, 10);
        drawCenteredText(image, font, "no active user profiles", 256, 112, 1,
                         170, 170, 170, false, elapsed_ms, 72);
    }
    return image;
}

PshImage renderCreatePlayerMenu(const Nba97CreateMenu& menu,
                                const PshFont& font,
                                const MenuSpritePack& sprites,
                                const CreatePlayerCardPack& cards,
                                std::uint32_t elapsed_ms,
                                bool selected_overlay_visible) {
    PshImage image;
    image.width = kWidth;
    image.height = kHeight;
    image.tag = "CPLY";
    image.rgba.assign(static_cast<std::size_t>(kWidth) * kHeight * 4, 0);
    blitAt(image, sprites, "Bkge", 0, 0);
    blitAt(image, sprites, "Bkgf", 128, 0);
    blitAt(image, sprites, "Bkgg", 256, 0);
    blitAt(image, sprites, "Bkgh", 384, 0);
    for (const auto& border : std::array<std::tuple<const char*, int, int>, 10>{{
        {"brte",0,5},{"brtf",128,5},{"brtg",256,5},{"brth",384,5},
        {"brle",0,65},{"brri",476,65},{"brbe",0,185},{"brbf",128,185},
        {"brbg",256,185},{"brbh",384,185}}})
        blitBlueBorder(image, sprites, std::get<0>(border),
                       std::get<1>(border), std::get<2>(border));
    blitAt(image, sprites, "XXL1", 30, 16);
    blitAt(image, sprites, "XXR2", 404, 16);
    blitJumbledTitleSprite(image, sprites, "ba05", 139, 10, elapsed_ms);
    blitAt(image, sprites, "help", 235, 217);

    static constexpr std::array<int, 3> card_x{91, 212, 321};
    static constexpr std::array<int, 3> card_y{82, 80, 82};
    static constexpr std::array<int, 3> art_x{107, 222, 335};
    static constexpr std::array<int, 3> art_y{100, 99, 100};
    static constexpr std::array<const char*, 3> normal{"c00c","c02c","c04c"};
    static constexpr std::array<const char*, 3> selected{"c01c","c03c","c05c"};
    static constexpr std::array<const char*, 3> disabled{"c00r","c02c","c04r"};
    for (int index = 0; index < 3; ++index) {
        const auto& art = cards[static_cast<std::size_t>(index)];
        if (!art.rgba.empty())
            blitScaled(image, art, 0, 0, art.width, art.height,
                       art_x[static_cast<std::size_t>(index)],
                       art_y[static_cast<std::size_t>(index)], art.width, art.height);
        const bool focused = menu.selected == index && selected_overlay_visible;
        const char* tag = !menu.enabled[index] ? disabled[static_cast<std::size_t>(index)] :
                          focused ? selected[static_cast<std::size_t>(index)] :
                                    normal[static_cast<std::size_t>(index)];
        blitAt(image, sprites, tag, card_x[static_cast<std::size_t>(index)],
               card_y[static_cast<std::size_t>(index)]);
    }
    char status[96]{};
    nba97_create_menu_status(&menu, status, sizeof(status));
    drawCenteredText(image, font, status, 256, 201, 1, 220, 220, 220,
                     false, elapsed_ms, 74);
    return image;
}

PshImage renderCreatePlayerEditor(const Nba97CreateEditor& editor,
                                  const RosterDatabase& database,
                                  const PshFont& font,
                                  const MenuSpritePack& sprites,
                                  std::uint32_t elapsed_ms,
                                  const CreatePlayerPreview* preview,
                                  const Nba97CreateNameEditor* name_editor,
                                  const PshFont* control_font) {
    PshImage image;
    image.width = kWidth;
    image.height = kHeight;
    image.tag = "CPED";
    image.rgba.assign(static_cast<std::size_t>(kWidth) * kHeight * 4, 0);
    blitAt(image, sprites, "Bkge", 0, 0);
    blitAt(image, sprites, "Bkgf", 128, 0);
    blitAt(image, sprites, "Bkgg", 256, 0);
    blitAt(image, sprites, "Bkgh", 384, 0);
    // The retail ordering table places the 3D model behind the authored
    // frontend frame/help layer. Drawing it last let close-up legs overwrite
    // the bottom border and HELP strip even when every model packet matched.
    if (preview != nullptr) preview->draw(image, editor, elapsed_ms);
    for (const auto& border : std::array<std::tuple<const char*, int, int>, 10>{{
        {"brta",0,5},{"brtb",128,5},{"brtc",256,5},{"brtd",384,5},
        {"brle",0,65},{"brri",476,65},{"brbe",0,185},{"brbf",128,185},
        {"brbg",256,185},{"brbh",384,185}}})
        blitBlueBorder(image, sprites, std::get<0>(border),
                       std::get<1>(border), std::get<2>(border));
    blitJumbledTitleSprite(image, sprites, "ba05", 30, 10, elapsed_ms);
    blitAt(image, sprites, "help", 235, 217);

    /* FUN_8004DE08/FUN_8004DEC4 start a 20-vblank transition from the
       selector's neutral 0x808080 tint into the gold 0x786600 tint. The
       previous hard 136 ms on/off toggle was both too fast and too abrupt. */
    constexpr std::uint32_t pulse_ticks = 20;
    const auto pulse_tick = (elapsed_ms / 17u) % (pulse_ticks * 2u);
    const auto pulse_step = pulse_tick <= pulse_ticks ? pulse_tick :
        pulse_ticks * 2u - pulse_tick;
    const int pulse = static_cast<int>(pulse_step * 255u / pulse_ticks);
    const auto selected_channel = [pulse](int neutral, int gold) {
        return static_cast<std::uint8_t>((neutral * (255 - pulse) + gold * pulse) / 255);
    };
    const auto valueFor = [&](std::uint8_t field) {
        Nba97CreateEditor copy = editor;
        copy.selected_field = field;
        char value[64]{};
        nba97_create_editor_value(&copy, value, sizeof(value));
        if (field == NBA97_CREATE_TEAM) {
            if (const auto* team = database.team(editor.team))
                return team->displayName();
        }
        if (field == NBA97_CREATE_COLLEGE) {
            if (editor.college == 0) return std::string("n/a");
            std::vector<std::string> schools;
            schools.reserve(database.players().size());
            for (const auto& player : database.players())
                if (!player.school_name.empty() && player.school_name != "n/a")
                    schools.push_back(player.school_name);
            std::sort(schools.begin(), schools.end());
            schools.erase(std::unique(schools.begin(), schools.end()), schools.end());
            const auto index = static_cast<std::size_t>(editor.college - 1);
            if (index < schools.size()) return schools[index];
        }
        static constexpr std::array<const char*, 5> positions{
            "center", "power forward", "small forward", "shooting guard", "point guard"};
        if (field == NBA97_CREATE_POSITION && editor.position < positions.size())
            return std::string(positions[editor.position]);
        return std::string(value);
    };
    const auto drawRow = [&](std::uint8_t field, int y, int label_x = 62,
                             int value_x = 212) {
        const bool selected = !editor.rating_group_active && editor.selected_field == field;
        const std::uint8_t neutral = editor.rating_group_active && field >= NBA97_CREATE_FIELD_GOALS ? 112 : 225;
        const std::uint8_t red = selected ? selected_channel(225, 255) : neutral;
        const std::uint8_t green = selected ? selected_channel(225, 214) : neutral;
        const std::uint8_t blue = selected ? selected_channel(225, 24) : neutral;
        if (selected) drawText(image, font, ">", label_x - 17, y, 1, red, green, blue);
        drawText(image, font, std::string(nba97_create_field_name(field)) + ":",
                 label_x, y, 1, red, green, blue);
        const auto row_value=valueFor(field);
        if (name_editor != nullptr && name_editor->active &&
            name_editor->field == field && name_editor->cursor < row_value.size()) {
            // FUN_8004C488 does not open a child sprite or draw a caret. It
            // styles one character in the existing value text object and
            // temporarily renders the authored blank '_' as '='.
            const auto prefix=row_value.substr(0,name_editor->cursor);
            auto active=row_value.substr(name_editor->cursor,1);
            if(active=="_")active="=";
            const auto suffix=row_value.substr(name_editor->cursor+1);
            drawText(image,font,prefix,value_x,y,1,225,225,225);
            const int active_x=value_x+font.textWidth(prefix);
            drawText(image,font,active,active_x,y,1,red,green,blue);
            drawText(image,font,suffix,active_x+font.textWidth(active),y,1,225,225,225);
        } else {
            drawText(image, font, row_value, value_x, y, 1, red, green, blue);
        }
    };

    drawRow(NBA97_CREATE_FIRST_NAME, 90, 60, 210);
    drawRow(NBA97_CREATE_LAST_NAME, 106, 60, 210);
    const auto drawBank = [&](int first, int offset_y) {
        for (int row = 0; row < 4; ++row) {
            const int y = 133 + row * 15 + offset_y;
            const int field = first + row;
            if (y >= 133 && y <= 178 && field < NBA97_CREATE_FIELD_COUNT)
                drawRow(static_cast<std::uint8_t>(field), y, 62, 212);
        }
    };
    if (editor.scroll_ticks_remaining != 0) {
        const int progress = 6 - editor.scroll_ticks_remaining;
        const int direction = editor.visible_first_field >= editor.previous_visible_first_field ? 1 : -1;
        const int old_offset = -direction * progress * 60 / 6;
        const int new_offset = direction * (60 - progress * 60 / 6);
        drawBank(editor.previous_visible_first_field, old_offset);
        drawBank(editor.visible_first_field, new_offset);
    } else {
        drawBank(editor.visible_first_field, 0);
    }
    if (editor.selected_field >= NBA97_CREATE_FIELD_GOALS) {
        const auto average = [&](int first_rating) {
            int total = 0;
            for (int index = 0; index < 4; ++index)
                total += editor.ratings[first_rating + index];
            return total / 4;
        };
        static constexpr std::array<const char*, 4> summary{
            "scoring", "defense", "rebounds", "control"};
        for (int row = 0; row < 4; ++row) {
            const bool selected = editor.rating_group_active &&
                row == (editor.selected_field - NBA97_CREATE_FIELD_GOALS) / 4;
            const std::uint8_t r=selected ? selected_channel(225,255) : 225;
            const std::uint8_t g=selected ? selected_channel(225,214) : 225;
            const std::uint8_t b=selected ? selected_channel(225,24) : 225;
            if(selected) drawText(image,font,">",258,133+row*15,1,r,g,b);
            drawText(image, font, std::string(summary[row]) + ":", 275,
                     133 + row * 15, 1, r, g, b);
            drawText(image, font, std::to_string(average(1 + row * 4)), 428,
                     133 + row * 15, 1, r, g, b);
        }
    }
    const bool editing_name=name_editor!=nullptr&&name_editor->active;
    if(editing_name||editor.selected_field==NBA97_CREATE_FIRST_NAME||
       editor.selected_field==NBA97_CREATE_LAST_NAME) {
        const auto field=editing_name?name_editor->field:editor.selected_field;
        const auto* value=field==NBA97_CREATE_FIRST_NAME?
            editor.first_name:editor.last_name;
        const auto drawControllerPrompt=[&](unsigned char glyph,
                                             const std::string& suffix,
                                             const char* fallback) {
            const std::string prefix="press ";
            if(control_font==nullptr || control_font->glyph(static_cast<char>(glyph))==nullptr) {
                drawCenteredText(image,font,prefix+fallback+suffix,256,196,1,
                    225,225,225,false,elapsed_ms,100);
                return;
            }
            const std::string icon(1,static_cast<char>(glyph));
            const int gap=2;
            const int total=font.textWidth(prefix)+gap+control_font->textWidth(icon)+
                gap+font.textWidth(suffix);
            int x=256-total/2;
            drawText(image,font,prefix,x,196,1,225,225,225);
            x+=font.textWidth(prefix)+gap;
            draw_psh_text_centered(image,*control_font,icon,
                x+control_font->textWidth(icon)/2,196);
            x+=control_font->textWidth(icon)+gap;
            drawText(image,font,suffix,x,196,1,225,225,225);
        };
        if(editing_name)
            drawControllerPrompt(0x9f,std::string(" to accept ")+
                (field==NBA97_CREATE_FIRST_NAME?"first":"last")+" name.","START");
        else
            drawControllerPrompt(0x94,std::string(" to ")+(value[0]?"edit ":"enter ")+
                (field==NBA97_CREATE_FIRST_NAME?"first":"last")+" name.","X");
    }
    return image;
}

PshImage renderCreatedPlayerPicker(const Nba97CreatedPlayerPicker& picker,
                                   const Nba97CreatedPlayerCatalog& catalog,
                                   const RosterDatabase& database,
                                   const PshFont& font,
                                   const MenuSpritePack& sprites,
                                   std::uint32_t elapsed_ms) {
    PshImage image;
    image.width=kWidth; image.height=kHeight; image.tag="CPPK";
    image.rgba.assign(static_cast<std::size_t>(kWidth)*kHeight*4,0);
    blitAt(image,sprites,"Bkga",0,0); blitAt(image,sprites,"Bkgb",128,0);
    blitAt(image,sprites,"Bkgc",256,0); blitAt(image,sprites,"Bkgd",384,0);
    for(const auto& border:std::array<std::tuple<const char*,int,int>,10>{{
        {"brta",0,5},{"brtb",128,5},{"brtc",256,5},{"brtd",384,5},
        {"brle",0,65},{"brri",476,65},{"brba",0,185},{"brbb",128,185},
        {"brbc",256,185},{"brbd",384,185}}})
        blitBlueBorder(image,sprites,std::get<0>(border),std::get<1>(border),std::get<2>(border));
    blitAt(image,sprites,"XXL1",30,16); blitAt(image,sprites,"XXR2",404,16);
    blitJumbledTitleSprite(image,sprites,picker.frontend_state==0x21?"ba06":"ba07",143,10,elapsed_ms);
    blitAt(image,sprites,"help",235,217);
    const bool flash=((elapsed_ms/136u)&1u)==0;
    for(unsigned row=0;row<picker.visible_count;++row) {
        const unsigned item=picker.top+row;
        if(item>=picker.count)break;
        const int slot=picker.slots[item]; const auto& meta=catalog.metadata[slot];
        std::string line=std::to_string(slot+1)+"  "+meta.first_name+" "+meta.last_name;
        if(const auto* team=database.team(meta.team)) {
            line+="  "; line+=team->abbreviation;
        }
        const bool selected=item==picker.cursor;
        if(selected&&flash)drawText(image,font,">",63,78+static_cast<int>(row)*16,1,255,220,30);
        drawText(image,font,line,82,78+static_cast<int>(row)*16,1,
                 selected&&flash?255:225,selected&&flash?220:225,selected&&flash?30:225);
    }
    if(picker.top>0)drawText(image,font,"^",256,68,1,255,220,30);
    if(picker.top+picker.visible_count<picker.count)drawText(image,font,"v",256,194,1,255,220,30);
    return image;
}

PshImage renderUserProfileSetup(const UserProfileMenu& menu,
                                const UserProfileStore& store,
                                const PshFont& font,
                                const MenuSpritePack& sprites,
                                std::uint32_t elapsed_ms) {
    PshImage image;
    image.width = kWidth;
    image.height = kHeight;
    image.tag = "UPRF";
    image.rgba.assign(static_cast<std::size_t>(kWidth) * kHeight * 4, 0);
    blitAt(image, sprites, "Bkge", 0, 0);
    blitAt(image, sprites, "Bkgf", 128, 0);
    blitAt(image, sprites, "Bkgg", 256, 0);
    blitAt(image, sprites, "Bkgh", 384, 0);
    for (const auto& border : std::array<std::tuple<const char*, int, int>, 10>{{
        {"brte",0,5},{"brtf",128,5},{"brtg",256,5},{"brth",384,5},
        {"brle",0,65},{"brri",476,65},{"brbe",0,185},{"brbf",128,185},
        {"brbg",256,185},{"brbh",384,185}}})
        blitBlueBorder(image, sprites, std::get<0>(border), std::get<1>(border), std::get<2>(border));
    blitAt(image, sprites, "XXL1", 30, 16);
    blitAt(image, sprites, "XXR2", 404, 16);
    drawCenteredText(image, font, "user setup", 256, 31, 2, 255, 218, 24,
                     true, elapsed_ms, 76);

    const auto& profiles = store.profiles();
    const int first = menu.selected() >= 8 ? static_cast<int>(menu.selected()) - 7 : 0;
    const int final = (std::min)(static_cast<int>(profiles.size()), first + 8);
    for (int index = first; index <= final; ++index) {
        const bool start_new = index == static_cast<int>(profiles.size());
        const bool selected = menu.selected() == static_cast<std::size_t>(index);
        const int y = 74 + (index - first) * 15;
        const std::string label = start_new ? "start new" : profiles[static_cast<std::size_t>(index)].name;
        const int jiggle = selected ? static_cast<int>(std::lround(
            std::sin(elapsed_ms * 0.014) * 1.0)) : 0;
        drawText(image, font, selected ? ">" : " ", 142 + jiggle, y, 1,
                 selected ? 255 : 190, selected ? 220 : 190, selected ? 35 : 190);
        drawText(image, font, label, 165 + jiggle, y, 1,
                 selected ? 255 : 205, selected ? 220 : 205, selected ? 35 : 205,
                 selected, elapsed_ms, 78);
    }

    if (menu.editing()) {
        fillRect(image, 110, 181, 402, 211, 3, 5, 20);
        outlineRect(image, 110, 181, 402, 211, 2, 90, 92, 145);
        drawCenteredText(image, font, "enter user name", 256, 191, 1, 220, 220, 220, false, 0, 74);
        drawCenteredText(image, font, menu.draft() + "_", 256, 205, 1, 255, 220, 35, false, 0, 80);
    } else {
        drawCenteredText(image, font, "enter edit   delete remove   back return", 256, 203,
                         1, 195, 195, 205, false, 0, 62);
    }
    if (!menu.message().empty())
        drawCenteredText(image, font, menu.message(), 256, 218, 1,
                         menu.deletePending() ? 255 : 210, menu.deletePending() ? 90 : 210,
                         menu.deletePending() ? 45 : 60, false, 0, 65);
    else
        drawCenteredText(image, font, std::to_string(profiles.size()) + " / 20 users", 256, 218,
                         1, 210, 210, 60, false, 0, 72);
    return image;
}

PshImage renderRosterViewer(const RosterViewer& viewer,
                            const RosterDatabase& database,
                            const PshFont& font,
                            const MenuSpritePack& sprites,
                            std::uint32_t elapsed_ms,
                            const PshImage* player_portrait,
                            bool cool_facts_available,
                            const PshFont* control_font,
                            int stat_flash_direction,
                            bool cool_fact_overlay_visible,
                            bool player_city_strip_visible, const int16_t* title_corners) {
    if (viewer.mode() == RosterViewMode::TeamRoster)
        nba97_semantic_trace_record(0x80059034u);
    else
        nba97_semantic_trace_record(0x8005A538u);
    PshImage image;
    image.width = kWidth;
    image.height = kHeight;
    image.tag = viewer.mode() == RosterViewMode::TeamRoster ? "VROS" : "PLCR";
    image.rgba.assign(static_cast<std::size_t>(kWidth) * kHeight * 4, 0);
    const bool roster_screen = viewer.mode() == RosterViewMode::TeamRoster;
    const TeamRecord* team = viewer.selectedTeam(database);
    const PlayerRecord* selected = viewer.selectedPlayer(database);
    static constexpr std::array<const char*, 29> team_codes{
        "atl","bos","cha","chi","cle","dal","den","det","gol","hou",
        "ind","lac","lal","mia","mil","min","nwj","nwy","orl","phi",
        "pho","por","sac","san","sea","tor","uta","van","was"};
    if (roster_screen && team && team->id < team_codes.size()) {
        const std::string target_prefix = team_codes[team->id];
        const TeamRecord* from_team =
                viewer.paletteFromTeamIndex() < database.teams().size()
            ? &database.teams()[viewer.paletteFromTeamIndex()] : team;
        const std::string from_prefix = from_team->id < team_codes.size()
            ? team_codes[from_team->id] : target_prefix;
        // FUN_8002FF80 interpolates the patched 160-colour CLUT over 17
        // frontend ticks. The local 96 colours are identical in both decoded
        // images, so the same BGR555 interpolation leaves them unchanged.
        constexpr std::uint32_t palette_tick_ms = 17;
        const std::uint32_t transition_elapsed = elapsed_ms >= viewer.paletteTransitionStartMs()
            ? elapsed_ms - viewer.paletteTransitionStartMs() : 0;
        const std::uint32_t factor =
            (std::min)(16u, transition_elapsed / palette_tick_ms);
        static constexpr std::array<const char*, 4> roster_strips{
            "Bkga", "Bkgb", "Bkgc", "Bkgd"};
        const auto& strips = roster_strips;
        for (std::size_t index = 0; index < strips.size(); ++index) {
            const std::string from_tag = from_prefix + strips[index];
            const std::string target_tag = target_prefix + strips[index];
            const PshImage* from_image = sprite(sprites, from_tag.c_str());
            const PshImage* target_image = sprite(sprites, target_tag.c_str());
            if (factor < 16 && from_image && target_image)
                blitPaletteCrossfade(image, *from_image, *target_image,
                                     static_cast<int>(index) * 128, 0, factor);
            else
                blitAt(image, sprites, target_tag.c_str(),
                       static_cast<int>(index) * 128, 0);
        }
    } else {
        // State 0x24 exposes the second four background objects. Unlike the
        // roster list's team-paletted Bkga-d family, Bkge-h carries the blue
        // wavy jersey centre and dark side panels visible in the no$psx frame.
        blitAt(image, sprites, roster_screen ? "Bkga" : "Bkge", 0, 0);
        blitAt(image, sprites, roster_screen ? "Bkgb" : "Bkgf", 128, 0);
        blitAt(image, sprites, roster_screen ? "Bkgc" : "Bkgg", 256, 0);
        blitAt(image, sprites, roster_screen ? "Bkgd" : "Bkgh", 384, 0);
    }
    const std::array<std::tuple<const char*, int, int>, 10> roster_borders{{
        {"brte",0,5},{"brtf",128,5},{"brtg",256,5},{"brth",384,5},
        {"brle",0,65},{"brri",476,65},
        {"brbe",0,185},{"brbf",128,185},{"brbg",256,185},{"brbh",384,185}}};
    const std::array<std::tuple<const char*, int, int>, 10> player_borders{{
        {"brta",0,5},{"brtb",128,5},{"brtc",256,5},{"brtd",384,5},
        {"brle",0,65},{"brri",476,65},
        {"brba",0,185},{"brbb",128,185},{"brbc",256,185},{"brbd",384,185}}};
    for (const auto& border : roster_screen ? roster_borders : player_borders)
        // ZSET4-decoded already applies FEONLY screen 0x10's shared Pal0
        // CLUT to records 6..16, so preserve those exact decoded colours.
        blitAt(image, sprites, std::get<0>(border),
               std::get<1>(border), std::get<2>(border));
    if (roster_screen) {
        blitAt(image, sprites, "XXR2", 404, 16);
        // State 0x10 uses the same discrete four-corner frontend deformation
        // path as state 0x24. It shakes/jumbles; it is not a scanline wave.
        blitJumbledTitleSprite(image, sprites, "ba35", 142, 10, elapsed_ms, title_corners);
    }

    if (!team) return image;
    static constexpr std::array<const char*, 29> logo_tags{
        "atlR","bosR","chaR","chiR","cleR","dalR","denR","detR","golR","houR",
        "indR","lacR","lalR","miaR","milR","minR","nwjR","nwyR","orlR","phiR",
        "phoR","porR","sacR","sanR","seaR","torR","utaR","vanR","wasR"};
    if (viewer.mode() == RosterViewMode::TeamRoster) {
        // FE ordering-table insertion composites the team plate through the
        // authored frml aperture, then draws the frame over that plate.
        const PshImage* frame = sprite(sprites, "frml");
        if (frame && team->id < logo_tags.size()) {
            if (const PshImage* logo = sprite(sprites, logo_tags[team->id]))
                blitInsideFrame(image, *logo, *frame, 40, 16, 30, 15);
        }
        blitAt(image, sprites, "frml", 30, 15);

        // FUN_80059034 builds the centered team selector and both headings.
        const std::string team_name = team->displayName();
        drawCenteredText(image, font, team_name, 269, 66, 1, 245, 245, 245,
                         false, 0, 100);
        // FUN_8003D434 builds the selector from ZFONT control glyphs.  These
        // are permanent state-0x10 markers, not the tiny `tria` PSP sprite.
        drawCenteredText(image, font, std::string(1, static_cast<char>(0x8d)),
                         157, 66, 1, 255, 255, 255);
        drawCenteredText(image, font, std::string(1, static_cast<char>(0x8a)),
                         381, 66, 1, 255, 255, 255);

        drawCenteredText(image, font, rosterCategoryLabel(viewer.category()), 398, 80, 1,
                         238, 238, 238, false, 0, 100);
        drawCenteredText(image, font, rosterDisplayLabel(viewer.displayIndex()), 398, 92, 1,
                         238, 238, 238, false, 0, 100);

        // The original generic list in FUN_800590B8 is hard-coded to six
        // rows. Its configuration at DAT_800A436C supplies x=60, y=106 and
        // the font resolves to a 12-pixel row pitch.
        constexpr int list_x = 60;
        constexpr int first_y = 106;
        constexpr int row_pitch = 12;
        constexpr int visible_rows = 6;
        const std::size_t first = viewer.firstVisiblePlayer();
        const std::size_t end = (std::min)(team->roster.size(), first + visible_rows);
        constexpr std::uint32_t scroll_tick_ms = 17;
        const std::uint32_t scroll_elapsed = elapsed_ms >= viewer.scrollTransitionStartMs()
            ? elapsed_ms - viewer.scrollTransitionStartMs() : 0;
        const std::size_t scroll_from = viewer.scrollFromFirstPlayer();
        // FUN_8002B2AC/FUN_8002BA70 receive dy=+/-12 and duration=1.
        // For that single tick the seven-item object chain is shown at its
        // translated position; the following tick settles to six rows.
        const bool transition_tick = scroll_elapsed < scroll_tick_ms;
        const bool scrolling_down = transition_tick && first > scroll_from;
        const bool scrolling_up = transition_tick && first < scroll_from;
        const std::size_t render_first = scrolling_down ? scroll_from : first;
        const std::size_t render_count = (scrolling_down || scrolling_up) ? 7 : visible_rows;
        const std::size_t render_end = (std::min)(team->roster.size(),
                                                  render_first + render_count);
        const int scroll_offset = scrolling_down ? -row_pitch : 0;
        PshImage row_layer;
        row_layer.width = kWidth;
        row_layer.height = kHeight;
        row_layer.tag = "ROWS";
        row_layer.rgba.assign(static_cast<std::size_t>(kWidth) * kHeight * 4, 0);
        for (std::size_t index = render_first; index < render_end; ++index) {
            const std::size_t row = index - render_first;
            const PlayerRecord* player = database.player(team->roster[index]);
            if (!player) continue;
            const bool focused = index == viewer.playerIndex();
            const int y = first_y + static_cast<int>(row) * row_pitch + scroll_offset;
            static constexpr std::array<const char*, 5> short_positions{
                "c", "pf", "sf", "sg", "pg"};
            const char* position = player->position < short_positions.size()
                ? short_positions[player->position] : "-";
            // FUN_8002AB88 modulates the selected generic-list item from
            // neutral PS1 texture color 0x808080 to gold 0x786600 over 20
            // ticks, then back. PS1 0x80 is neutral rather than half-bright.
            const std::uint32_t pulse = (elapsed_ms / 17) % 40;
            const std::uint32_t blend = pulse <= 20 ? pulse : 40 - pulse;
            const auto selected_channel = [blend](int neutral, int gold) {
                return static_cast<std::uint8_t>(
                    (neutral * static_cast<int>(20 - blend) +
                     gold * static_cast<int>(blend)) / 20);
            };
            const std::uint8_t row_r = focused ? selected_channel(255, 240) : 255;
            const std::uint8_t row_g = focused ? selected_channel(255, 204) : 255;
            const std::uint8_t row_b = focused ? selected_channel(255, 0) : 255;
            // FUN_8003CF70 retains the identity object while FUN_8003B26C
            // replaces the dynamic stat object.  Reproduce the original
            // fixed fields instead of concatenating text with guessed spaces.
            const std::string position_text = position;
            const std::string number_text = player->jerseyNumberText();
            drawText(row_layer, font, position_text,
                     list_x + 28 - scaledTextWidth(font, position_text, 1, 100), y, 1,
                     row_r, row_g, row_b, false, elapsed_ms, 100);
            drawText(row_layer, font, number_text,
                     list_x + 58 - scaledTextWidth(font, number_text, 1, 100), y, 1,
                     row_r, row_g, row_b, false, elapsed_ms, 100);
            drawText(row_layer, font, player->last_name, list_x + 66, y, 1,
                     row_r, row_g, row_b, false, elapsed_ms, 100);

            const RosterDisplayValues values = rosterDisplayValues(
                *player, database, viewer.category(), viewer.displayIndex());
            if (values.paired) {
                drawCenteredText(row_layer, font, values.first, 313, y, 1,
                                 row_r, row_g, row_b, false, 0, 100);
                drawCenteredText(row_layer, font, values.second, 462, y, 1,
                                 row_r, row_g, row_b, false, 0, 100);
            } else {
                drawCenteredText(row_layer, font, values.first, 398, y, 1,
                                 row_r, row_g, row_b, false, 0, 100);
            }
        }
        // The sixth baseline is y=166 and its original glyphs extend below
        // y=172. The next (seventh) baseline starts at y=178, which recovers
        // the exact bottom clip boundary without exposing that extra row.
        blitScaled(image, row_layer, 0, 100, kWidth, 78, 0, 100, kWidth, 78);
        if (first > 0)
            drawCenteredText(image, font, std::string(1, static_cast<char>(0x8b)),
                             48, 108, 1, 255, 255, 255);
        if (end < team->roster.size())
            drawCenteredText(image, font, std::string(1, static_cast<char>(0x8c)),
                             48, 168, 1, 255, 255, 255);
        blitAt(image, sprites, "help", 235, 217);
    } else if (selected) {
        // no$psx framebuffer comparison shows state 0x24's body text uses a
        // slightly condensed horizontal transform. Keep the recovered
        // baselines/vertical glyph metrics intact and apply that transform
        // only to the player-card text objects.
        constexpr int player_text_width = 90;
        // Nested state 0x24 / graphics state 0x11. The photograph comes from
        // Z1PORT physical record playerId+1; record zero is the fallback.
        blitJumbledTitleSprite(image, sprites, "ba41", 40, 18, elapsed_ms, title_corners);
        // Layout36 slot18 remains behind the dynamic photo (depth13 vs3).
        // 800310D8 hides shot during a request; 80030E78 enables it on success.
        blitAt(image, sprites, "wait", 334, 35);
        if (player_portrait && player_portrait->width == 180 && player_portrait->height == 156)
            blitScaled(image, *player_portrait, 0, 0, 180, 156, 297, 35, 180, 156);
        // State 0x24's ZSET8 pack contains the exact 39x156 city wordmark and
        // crest composite for every team (atlZ, chiZ, ...). It overlays the
        // reserved left strip of the Z1PORT photograph at the recovered slot.
        if ((player_portrait || player_city_strip_visible) && team->id <= 29) {
            // Team29 is the original free-agent xfrZ wordmark, not xeaZ.
            // The xeaP palette mapping is separate from this graphic identity.
            // Confirmed by the original Sign -> View Alston reference.
            const std::string team_strip = team->id==29?"xfrZ":std::string(team_codes[team->id]) + "Z";
            blitAt(image, sprites, team_strip.c_str(), 296, 35);
        }
        drawText(image, font, selected->displayName(), 54, 63, 1, 255, 255, 255,
                 false, 0, player_text_width);
        drawText(image, font, "num.", 54, 75, 1, 238, 238, 238,
                 false, 0, player_text_width);
        drawText(image, font, selected->jerseyNumberText(), 108, 75, 1,
                 255, 255, 255, false, 0, player_text_width);
        drawText(image, font, "pos.", 54, 86, 1, 238, 238, 238,
                 false, 0, player_text_width);
        static constexpr std::array<const char*, 5> starting_positions{
            "starting C", "starting PF", "starting SF", "starting SG", "starting PG"};
        const std::string displayed_position = team->id!=29 && viewer.playerIndex() < starting_positions.size()
            ? starting_positions[viewer.playerIndex()]
            : positionName(selected->position);
        drawText(image, font, displayed_position, 108, 86, 1,
                 255, 255, 255, false, 0, player_text_width);
        drawCenteredText(image, font, rosterCategoryLabel(viewer.category()), 177, 99, 1,
                         245, 245, 245,
                         false, 0, player_text_width);
        static constexpr std::array<const char*, 24> stat_labels{
            "games played", "games started", "points", "points per game",
            "minutes played", "minutes per game", "field goals", "field goal %",
            "3 point fgs", "3 point %", "free throws", "free throw %",
            "off. rebounds", "def. rebounds", "total rebounds", "rebounds per game",
            "blocks", "blocks per game", "steals", "steals per game", "assists",
            "assists per game", "fouls", "ejections"};
        static constexpr std::array<std::int16_t, 24> stat_fields{
            22, 23, 6, 32, 8, 34, 0, 43, 1, 44, 2, 45,
            15, 16, 17, 37, 18, 38, 19, 39, 20, 40, 21, 25};
        static constexpr std::array<const char*, 14> attribute_labels{
            "nickname", "birthdate", "birthplace", "height", "weight", "hand",
            "school", "years pro", "draft year", "drafted by", "drafted",
            "overall", "acquired how?", "acquired from?"};
        static constexpr std::array<std::size_t, 14> attribute_fields{
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
        const std::size_t first_stat = viewer.firstVisiblePlayerStat();
        constexpr std::size_t visible_stats = 6;
        for (std::size_t row = 0; row < visible_stats; ++row) {
            const std::size_t index = first_stat + row;
            if (index >= viewer.playerStatCount()) break;
            // DAT_800A482C base y=112; FUN_8003D930 selector 1 resolves
            // through DAT_80024F6C to the original 14-pixel row pitch.
            const int y = 112 + static_cast<int>(row) * 14;
            std::string label;
            std::string value;
            if (viewer.category() == 0) {
                label = attribute_labels[index];
                value = database.playerAttribute(*selected, attribute_fields[index]);
            } else if (viewer.category() == 1) {
                label = index == 0 ? "overall rating" :
                    playerRatingName(static_cast<PlayerRating>(index - 1));
                value = std::to_string(index == 0 ? selected->overallRating() :
                    selected->ratings[index - 1]);
            } else {
                PlayerStatPeriod period = PlayerStatPeriod::Season1995_96;
                if (viewer.category() == 3) period = PlayerStatPeriod::Playoffs1995_96;
                else if (viewer.category() == 4) period = PlayerStatPeriod::CurrentSeason;
                else if (viewer.category() == 5) period = PlayerStatPeriod::CurrentPlayoffs;
                label = stat_labels[index];
                value = selected->stats(period).format(stat_fields[index]);
            }
            const bool flashed_row = stat_flash_direction < 0 ? row == 0 :
                                     stat_flash_direction > 0 ? row + 1 == visible_stats : false;
            const std::uint8_t row_r = flashed_row ? 255 : 245;
            const std::uint8_t row_g = flashed_row ? 218 : 245;
            const std::uint8_t row_b = flashed_row ? 35 : 245;
            drawText(image, font, label, 54, y, 1, row_r, row_g, row_b,
                     false, 0, player_text_width);
            drawText(image, font, value,
                     293 - scaledTextWidth(font, value, 1, player_text_width), y, 1,
                     row_r, row_g, row_b, false, 0, player_text_width);
        }
        // FUN_8003D930 receives x=48 for these markers, but the PS1 text
        // primitive applies the arrow glyph's left-side bearing before it is
        // rasterized.  Our decoded-font path has no bearing metadata, so use
        // the resulting visual center (x=42) to preserve the original gap
        // before the stat label at x=54.
        constexpr int player_stat_arrow_center_x = 42;
        if (first_stat > 0)
            drawCenteredText(image, font, std::string(1, static_cast<char>(0x8b)),
                             player_stat_arrow_center_x, 114, 1,
                             stat_flash_direction < 0 ? 255 : 255,
                             stat_flash_direction < 0 ? 218 : 255,
                             stat_flash_direction < 0 ? 35 : 255);
        if (first_stat + visible_stats < viewer.playerStatCount())
            drawCenteredText(image, font, std::string(1, static_cast<char>(0x8c)),
                             player_stat_arrow_center_x, 184, 1,
                             stat_flash_direction > 0 ? 255 : 255,
                             stat_flash_direction > 0 ? 218 : 255,
                             stat_flash_direction > 0 ? 35 : 255);
        drawCenteredText(image, font, team->displayName(),
                         177, 202, 1, 245, 245, 245, false, 0,
                         player_text_width);
        // ZSET8 provides both the exact controller marker and the original
        // Cool Facts plaque. State36 object20/o18a stays enabled beneath
        // object21/o18b, which 59EBC toggles for eight presents. Speech
        // duration does not determine the overlay's visibility.
        if (cool_facts_available) {
            blitAt(image, sprites, "cros", 336, 204);
            blitAt(image, sprites, "o18a", 356, 198);
            if(cool_fact_overlay_visible) blitAt(image, sprites, "o18b", 356, 198);
        }
        blitAt(image, sprites, "help", 235, 217);
    }
    if (viewer.helpVisible() && roster_screen) {
        fillRect(image, 78, 52, 434, 190, 3, 5, 20);
        outlineRect(image, 78, 52, 434, 190, 2, 90, 88, 190);
        drawCenteredText(image, font,
                         roster_screen ? "view rosters help" : "view player help",
                         256, 72, 1, 255, 218, 35);
        const std::array<const char*, 6> lines = roster_screen
            ? std::array<const char*, 6>{"left/right - scan through teams",
                "up/down - select player", "enter/click - view current player",
                "q/e - category", "z/c - stat field", "esc - cancel all changes"}
            : std::array<const char*, 6>{};
        for (std::size_t row = 0; row < lines.size(); ++row)
            drawText(image, font, lines[row], 100, 92 + static_cast<int>(row) * 14,
                     1, 235, 235, 235);
    } else if (viewer.helpVisible()) {
        // FUN_80040FCC state 0x24 -> descriptor 0x800B22F0. The original
        // controller-help page is an opaque green modal with angular green
        // panels, white instructions, and ZFONT1 controller glyphs.
        constexpr int modal_left = 45, modal_top = 14;
        constexpr int modal_right = 467, modal_bottom = 226;
        fillRect(image, modal_left, modal_top, modal_right, modal_bottom, 0, 145, 18);
        for (int y = modal_top; y < modal_bottom; ++y) {
            const int diagonal = (y - modal_top) * 2 / 3;
            fillRect(image, modal_left, y, 100 + diagonal, y + 1, 0, 105, 28);
            fillRect(image, 385 + diagonal / 3, y, modal_right, y + 1, 0, 177, 18);
        }
        outlineRect(image, modal_left, modal_top, modal_right, modal_bottom,
                    1, 0, 92, 22);
        static constexpr std::array<const char*, 8> instructions{
            "change stat layer", "scan through teams", "view player stats",
            "change player", "play cool fact", "stop cool fact", "continue", "continue"};
        for (std::size_t row = 0; row < instructions.size(); ++row) {
            const int y = 24 + static_cast<int>(row) * 25;
            if (control_font) {
                const auto control = [&](unsigned char glyph, int x) {
                    drawNativeControlGlyph(image, *control_font, glyph, x, y);
                };
                if (row == 0) { control(0x97, 67); control(0x99, 103); }
                else if (row == 1) { control(0x96, 67); control(0x98, 103); }
                else if (row == 2) { control(0x9c, 74); control(0x9d, 101); }
                else if (row == 3) { control(0x9a, 74); control(0x9b, 101); }
                else if (row == 4) control(0x94, 79);
                else if (row == 5) control(0x93, 79);
                else if (row == 6) control(0x9f, 67);
                else control(0x9e, 67);
            }
            drawText(image, font, instructions[row], 155, y + 2, 1, 245, 245, 245);
        }
    }
    return image;
}

PshImage renderCompareScreen(const Nba97CompareRefresh& refresh, const RosterDatabase& db,
        const CompareAssets& assets, const MenuSpritePack& sprites, const PshFont& font,
        const std::array<PshImage,2>& portraits, std::uint32_t elapsed_ms,
        const FrontendPaletteAssets& backgrounds, const Nba97FrontendPalette& palette,
        const std::array<Nba97ReorderTint,6>& arrows, const int16_t* title_corners) {
    const auto& s=refresh.text;
    if(!s.initialized || s.layer>3 || s.active_side>1 || s.team[0]>29 || s.team[1]>29 ||
       s.top+5u>nba97_compare_stat_count(&s)) throw std::runtime_error("invalid Compare render state");
    PshImage image; image.width=512; image.height=240; image.tag="COMPARE";
    image.rgba.assign(512*240*4,255);
    // 2FF80 changes raw dynamic CLUT words independently for each half.
    // Team29 is palette xeaP; the name helper's remap29->31 is unrelated.
    backgrounds.draw(image,palette);
    const std::array<std::tuple<const char*,int,int>,10> borders{{
        {"brte",0,5},{"brtf",128,5},{"brtg",256,5},{"brth",384,5},
        {"brle",0,65},{"brri",476,65},{"brbe",0,185},{"brbf",128,185},
        {"brbg",256,185},{"brbh",384,185}}};
    for(const auto& b:borders) blitAt(image,sprites,std::get<0>(b),std::get<1>(b),std::get<2>(b));
    blitJumbledTitleSprite(image,sprites,"ba02",170,15,elapsed_ms,title_corners);
    for(unsigned side=0;side<2;++side) {
        const auto* frame=sprite(sprites,side ? "frmr" : "frml");
        const auto* plate=sprite(sprites,side ? "111p" : "110p");
        const auto* player=db.player(s.player[side]);
        if(!frame || !plate || !player || portraits[side].width!=87 || portraits[side].height!=51)
            throw std::runtime_error("missing original Compare portrait/frame/player");
        blitReorderPlate(image,*plate,side ? 370 : 40,16,side);
        blitInsideFrame(image,portraits[side],*frame,side ? 386 : 54,22,side ? 368 : 30,15);
        blitAt(image,sprites,side ? "frmr" : "frml",side ? 368 : 30,15);
        // 5A280 stores object x256/512; 3B26C adds manager x-offset -128
        // for VALUES, while 3CF70 keeps the shared LABELS at x256.
        const int x=side ? 384 : 128;
        drawCenteredText(image,font,player->displayName(),x,82,1);
        drawCenteredText(image,font,player->jerseyNumberText(),x,92,1);
        const auto position=s.team[side]!=29 && s.slot[side]<5 ? assets.text(7+s.slot[side]) : positionName(player->position);
        drawCenteredText(image,font,position,x,102,1);
        const auto* team=db.team(s.team[side]);
        drawCenteredText(image,font,team ? team->displayName() : assets.text(12),x,212,1);
        for(unsigned row=0;row<5;++row)
            drawCenteredText(image,font,assets.value(db,*player,s.layer,nba97_compare_refresh_top(&refresh,side)+row),x,135+14*row,1);
    }
    for(unsigned i=0;i<3;++i) drawCenteredText(image,font,assets.text(i),256,82+10*i,1);
    drawCenteredText(image,font,assets.text(3+s.layer),256,116,1);
    for(unsigned row=0;row<5;++row)
        drawCenteredText(image,font,assets.label(s.layer,s.top+row),256,135+14*row,1);
    // 3D434 builds four arrows at118/135 and874/891. 39BA8 translates
    // their shared object +/-500 so only the active side's pair is on-screen.
    for(unsigned direction=0;direction<4;++direction) {
        if(direction==2 && !s.top) continue;
        if(direction==3 && s.top+5u>=nba97_compare_stat_count(&s)) continue;
        const unsigned char code=direction==0 ? 0x8d : direction==1 ? 0x8a : direction==2 ? 0x8b : 0x8c;
        const auto* glyph=font.glyph(static_cast<char>(code));
        if(!glyph) throw std::runtime_error("missing original Compare arrow glyph");
        const int center=(s.active_side ? 374 : 118)+17*direction;
        const int x=direction>=2 ? 246 : center-font.textWidth(std::string(1,static_cast<char>(code)))/2;
        const int y=(direction==2 ? 127 : direction==3 ? 203 : 116)-glyph->center_y;
        const auto& tint=arrows[direction>=2 ? direction+2 : s.active_side*2+direction];
        for(unsigned gy=0;gy<glyph->height;++gy) for(unsigned gx=0;gx<glyph->width;++gx) {
            const auto at=(gy*glyph->width+gx)*4;
            if(!glyph->rgba[at+3]) continue;
            // Original glyph pixels, per-channel neutral128 modulation. Do
            // not recolor through drawText's max-channel luminance shortcut.
            putPixel(image,x+gx,y+gy,
                static_cast<std::uint8_t>((std::min)(255u,unsigned(glyph->rgba[at])*tint.rgb[0]/128)),
                static_cast<std::uint8_t>((std::min)(255u,unsigned(glyph->rgba[at+1])*tint.rgb[1]/128)),
                static_cast<std::uint8_t>((std::min)(255u,unsigned(glyph->rgba[at+2])*tint.rgb[2]/128)));
        }
    }
    // 3A224 primary markers: x256-20+10, y135-8 /135+4*14+12.
    // 3AB64 synchronizes both lists; secondary markers are intentionally offscreen.
    blitAt(image,sprites,"help",235,217);
    return image;
}

PshImage renderReorderScreen(const Nba97ReorderScreen& screen,
        const MenuSpritePack& sprites, const PshFont& font,
        const std::array<PshImage, 2>& portraits, const PshImage& text_layer,
        std::uint32_t elapsed_ms, const int16_t* title_corners) {
    PshImage image;
    image.width = 512; image.height = 240; image.tag = "REORDER";
    image.rgba.assign(512 * 240 * 4, 255);
    static constexpr const char* teams[] = {"atl","bos","cha","chi","cle","dal","den","det",
        "gol","hou","ind","lac","lal","mia","mil","min","nwj","nwy","orl","phi",
        "pho","por","sac","san","sea","tor","uta","van","was"};
    if (screen.team < 0 || screen.team >= 29) throw std::runtime_error("invalid Re-order team");
    for (int i = 0; i < 4; ++i) {
        const std::string tag = std::string(teams[screen.team]) + "Bkg" + static_cast<char>('a'+i);
        if (!sprite(sprites, tag.c_str())) throw std::runtime_error("missing Re-order background " + tag);
        blitAt(image, sprites, tag.c_str(), i*128, 0);
    }
    // FEONLY graphics layout 80096BC4. Depth 4 border is behind title (3), portrait
    // (2), and aperture frame (1). There are NO NBA/EA logos on this screen.
    const std::array<std::tuple<const char*, int, int>, 10> borders{{
        {"brte",0,5},{"brtf",128,5},{"brtg",256,5},{"brth",384,5},
        {"brle",0,65},{"brri",476,65},{"brbe",0,185},{"brbf",128,185},
        {"brbg",256,185},{"brbh",384,185}}};
    for (const auto& b : borders) blitAt(image, sprites, std::get<0>(b), std::get<1>(b), std::get<2>(b));
    blitJumbledTitleSprite(image, sprites, "ba22", 156, 10, elapsed_ms,title_corners);
    for (int p = 0; p < 2; ++p) {
        const char* frame_tag = p ? "frmr" : "frml";
        const auto* frame = sprite(sprites, frame_tag);
        if (!frame) throw std::runtime_error("missing Re-order aperture");
        const int frame_x = p ? 368 : 30;
        // Runtime image objects 18/19 use the rectangular Z2PORT image at
        // 54/386,22. Objects 20/21 are the authored plate underneath it.
        const auto* plate = sprite(sprites, p ? "111p" : "110p");
        if (!plate) throw std::runtime_error("missing Re-order plate");
        blitReorderPlate(image, *plate, p ? 370 : 40, 16, p);
        blitInsideFrame(image, portraits[p], *frame, p ? 386 : 54, 22, frame_x, 15);
        blitAt(image, sprites, frame_tag, frame_x, 15);
    }
    // Both pairs persist. 8003A224 updates the active pair; it does not hide
    // the inactive page's arrows created by the 8003DD38 constructor loop.
    Nba97ReorderMarker markers[4]{};
    nba97_reorder_screen_markers(&screen, markers);
    for (const auto& marker : markers) if (marker.visible) {
        const std::string text(1, static_cast<char>(marker.glyph));
        draw_psh_text_centered(image, font, text,
            marker.x + font.textWidth(text)/2, marker.y);
    }
    const char* help_tag = nba97_reorder_screen_help_tag(&screen);
    if (!help_tag || !sprite(sprites, help_tag))
        throw std::runtime_error("missing source-selected Re-order Help footer");
    blitAt(image, sprites, help_tag, 235, 217);
    blitScaled(image, text_layer, 0, 0, 512, 240, 0, 0, 512, 240);
    return image;
}

PshImage renderTradeScreen(const Nba97TradeScreen& s,const MenuSpritePack& sprites,
        const PshFont& font,const std::array<PshImage,2>& portraits,const PshImage& labels,
        const FrontendPaletteAssets& backgrounds,const Nba97FrontendPalette& palette,
        std::uint32_t elapsed,const int16_t* corners) {
    PshImage im;im.width=512;im.height=240;im.tag="TRADE";im.rgba.assign(512*240*4,255);
    backgrounds.draw(im,palette);
    // 80096D24: original Trade layout, independently coloured background halves.
    for(const auto& b:std::array<std::tuple<const char*,int,int>,10>{{
        {"brte",0,5},{"brtf",128,5},{"brtg",256,5},{"brth",384,5},
        {"brle",0,65},{"brri",476,65},{"brbe",0,185},{"brbf",128,185},
        {"brbg",256,185},{"brbh",384,185}}}) {
        if(!sprite(sprites,std::get<0>(b))) throw std::runtime_error("missing original Trade border");
        blitAt(im,sprites,std::get<0>(b),std::get<1>(b),std::get<2>(b));
    }
    const bool sign=s.frontend_state==14;
    const bool release=s.frontend_state==17;
    const char* title=release?"ba23":sign?"ba30":"ba38"; // state17 layout80097104
    if(!sprite(sprites,title)) throw std::runtime_error("missing editor title");
    blitJumbledTitleSprite(im,sprites,title,release?140:sign?156:155,10,elapsed,corners);
    for(int p=0;p<2;++p) {
        const auto* frame=sprite(sprites,p?"frmr":"frml");
        const auto* plate=sprite(sprites,p?"111p":"110p");
        if(!frame || !plate) throw std::runtime_error("missing Trade portrait aperture");
        blitReorderPlate(im,*plate,p?370:40,16,p);
        blitInsideFrame(im,portraits[p],*frame,p?386:54,22,p?368:30,15);
        blitAt(im,sprites,p?"frmr":"frml",p?368:30,15);
        for(int down=0;down<2;++down) if(down?s.top[p]<(s.team[p]==29?94:9):s.top[p]>0) {
            const std::string glyph(1,char(down?0x8c:0x8b));
            draw_psh_text_centered(im,font,glyph,(p?270:60)-14+font.textWidth(glyph)/2,116+down*80);
        }
    }
    const char* help=release?"help":s.phase==NBA97_TRADE_SECOND?"hel2":"hel1";
    if(!sprite(sprites,help)) throw std::runtime_error("missing Trade footer");
    blitAt(im,sprites,help,235,217);
    blitScaled(im,labels,0,0,512,240,0,0,512,240);return im;
}
} // namespace nba97
