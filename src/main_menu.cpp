#include "main_menu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>

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

void fillCircle(PshImage& image, int center_x, int center_y, int radius,
                std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    for (int y = -radius; y <= radius; ++y)
        for (int x = -radius; x <= radius; ++x)
            if (x * x + y * y <= radius * radius)
                putPixel(image, center_x + x, center_y + y, r, g, b);
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

void blitJiggledTitle(PshImage& destination, const MenuSpritePack& sprites,
                      std::uint32_t elapsed_ms) {
    const PshImage* item = sprite(sprites, "ba09");
    if (!item) return;
    for (int y = 0; y < item->height; ++y) {
        const int wave_x = static_cast<int>(std::lround(
            std::sin(elapsed_ms * 0.011 + y * 0.24) * 1.5));
        blitScaled(destination, *item, 0, y, item->width, 1,
                   148 + wave_x, 10 + y, item->width, 1);
    }
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

bool RecoveredBottomMenu::move(int horizontal, int vertical) noexcept {
    const int previous = selected_;
    if (page_ == FrontendPage::Rosters) {
        if (horizontal) selected_ = std::clamp(selected_ + (horizontal < 0 ? -1 : 1), 0, 7);
        if (vertical) selected_ = std::clamp(selected_ + (vertical < 0 ? -4 : 4), 0, 7);
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
    const bool changed = candidate != selected_;
    selected_ = candidate;
    return changed;
}

PshImage renderRecoveredBottomMenu(const RecoveredBottomMenu& menu,
                                   const PshFont& font,
                                   const MenuSpritePack& zset1,
                                   const MenuSpritePack& zset4,
                                   const MenuSpritePack& zset7,
                                   std::uint32_t elapsed_ms) {
    const MenuSpritePack& sprites = menu.page() == FrontendPage::Rosters ? zset4 :
                                    menu.page() == FrontendPage::Users ? zset7 : zset1;
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
        blitAt(image, sprites, "ba24", 166, 10);
        for (int i = 0; i < 8; ++i) {
            const int x = 49 + (i % 4) * 105;
            const int y = 57 + (i / 4) * 68;
            const std::string tag = "c" + std::string(i * 2 < 10 ? "0" : "") +
                                    std::to_string(i * 2 + (menu.selected() == i ? 1 : 0)) + "d";
            const PshImage* card = sprite(sprites, tag.c_str());
            if (card)
                blitScaled(image, *card, 0, 0, card->width, card->height,
                           x, y, card->width * 2 / 3, card->height * 2 / 3);
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

} // namespace nba97
