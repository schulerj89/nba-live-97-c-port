#include "create_player_delete_assets.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace nba97 {
namespace {
constexpr std::uint32_t kFreeAgent = 0x800AF352;
constexpr std::uint32_t kBench = 0x800AF3D6;
constexpr std::uint32_t kStarter = 0x800AF460;

std::vector<std::uint8_t> readPack(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 8 || input.tellg() > 4096)
        throw std::runtime_error("missing/invalid private Create Player Delete pack; run extract_assetpacks.ps1");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(input.tellg()));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("truncated Create Player Delete pack");
    return bytes;
}

std::string substitute(std::string value, const std::string& team) {
    for (auto at = value.find("%s"); at != std::string::npos; at = value.find("%s", at + team.size()))
        value.replace(at, 2, team);
    return value;
}

void drawText(PshImage& image, const PshFont& font, const std::string& value,
              int x, int y, const std::uint8_t* tint = nullptr) {
    for (const char character : value) {
        if (character == ' ') { x += font.spaceWidth(); continue; }
        const auto* glyph = font.glyph(character);
        if (!glyph) throw std::runtime_error("missing original Create Player Delete glyph");
        for (int gy = 0; gy < glyph->height; ++gy) for (int gx = 0; gx < glyph->width; ++gx) {
            const int px = x + gx, py = y - glyph->center_y + gy;
            const auto source = static_cast<std::size_t>((gy * glyph->width + gx) * 4);
            if (px < 0 || px >= 512 || py < 0 || py >= 240 || !glyph->rgba[source + 3]) continue;
            const auto target = static_cast<std::size_t>((py * 512 + px) * 4);
            for (int channel = 0; channel < 3; ++channel)
                image.rgba[target + channel] = static_cast<std::uint8_t>(std::min(
                    255u, unsigned(glyph->rgba[source + channel]) * (tint ? tint[channel] : 128) / 128));
            image.rgba[target + 3] = 255;
        }
        x += std::max(0, int(glyph->width) - font.kerning());
    }
}
}

CreatePlayerDeleteAssets::CreatePlayerDeleteAssets(const std::filesystem::path& root)
    : CreatePlayerDeleteAssets(readPack(root / "create_player/delete.n97ui")) {
    font_ = load_psh_font(root / "fonts/ZFONT1.PSH", 10, 1);
}

CreatePlayerDeleteAssets::CreatePlayerDeleteAssets(const std::vector<std::uint8_t>& bytes) {
    auto fail = [] { throw std::runtime_error("invalid private Create Player Delete pack"); };
    if (bytes.size() < 8 || bytes.size() > 4096 ||
        !std::equal(bytes.begin(), bytes.begin() + 4, "N97D")) fail();
    auto half = [&](std::size_t at) { return unsigned(bytes[at]) | (unsigned(bytes[at + 1]) << 8); };
    auto word = [&](std::size_t at) { return std::uint32_t(half(at)) | (std::uint32_t(half(at + 2)) << 16); };
    if (half(4) != 1 || half(6) != 3) fail();
    std::size_t at = 8;
    for (unsigned record = 0; record < 3; ++record) {
        if (at + 8 > bytes.size()) fail();
        const auto address = word(at), size = word(at + 4); at += 8;
        if (!size || size > bytes.size() - at || dialogs_.count(address)) fail();
        const auto end = at + size;
        if (size < 10) fail();
        const unsigned x = half(at), y = half(at + 2), width = half(at + 4);
        const unsigned height = bytes[at + 6], style = bytes[at + 7];
        const unsigned lines = bytes[at + 8], choices = bytes[at + 9]; at += 10;
        if (x > 246 || y > 110 || width < 20 || x + width > 512 || height < 10 ||
            y + height > 240 || style != 1 || lines < 4 || lines > 5 || choices != 2) fail();
        Dialog dialog{{static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
                       static_cast<std::int16_t>(width), static_cast<std::int16_t>(height)}, {}, {}};
        auto string = [&]() {
            if (at >= end || bytes[at++] != 1) fail();
            std::string value;
            while (at < end && bytes[at]) {
                if (bytes[at] < 32 || bytes[at] > 126 || value.size() >= 128) fail();
                value += static_cast<char>(bytes[at++]);
            }
            if (at >= end) fail(); ++at; return value;
        };
        for (unsigned line = 0; line < lines; ++line) dialog.body.push_back(string());
        for (unsigned choice = 0; choice < choices; ++choice) dialog.choices.push_back(string());
        if (at != end) fail();
        dialogs_.emplace(address, std::move(dialog));
    }
    if (at != bytes.size() || !dialogs_.count(kFreeAgent) || !dialogs_.count(kBench) ||
        !dialogs_.count(kStarter)) fail();
    const auto shape = [&](std::uint32_t address, int x, int y, int w, int h, std::size_t lines) {
        const auto& d = dialogs_.at(address);
        if (d.rect.x != x || d.rect.y != y || d.rect.width != w || d.rect.height != h ||
            d.body.size() != lines || d.choices.size() != 2) fail();
    };
    shape(kFreeAgent, 141, 75, 230, 100, 4);
    shape(kBench, 130, 75, 250, 100, 4);
    shape(kStarter, 130, 70, 250, 110, 5);
}

Nba97HelpRect CreatePlayerDeleteAssets::rect(std::uint32_t address) const {
    return dialogs_.at(address).rect;
}

void CreatePlayerDeleteAssets::draw(PshImage& image, std::uint32_t address,
                                    const std::string& team, const Nba97ResetPrompt& prompt) const {
    const auto& dialog = dialogs_.at(address);
    auto modal = prompt.modal;
    const bool text_visible = nba97_help_text_visible(&modal) != 0;
    modal.phase = nba97_help_visible(&modal) ? NBA97_HELP_GROWING : NBA97_HELP_CLOSED;
    FrontendHelpDescriptor background{};
    background.rect = dialog.rect; background.style = 1;
    FrontendHelpPack::draw(image, font_, background, modal);
    if (!text_visible) return;
    int y = dialog.rect.y + 10;
    for (const auto& source : dialog.body) {
        const auto line = substitute(source, team);
        drawText(image, font_, line, 256 - font_.textWidth(line) / 2, y); y += 12;
    }
    y += 6;
    for (unsigned choice = 0; choice < 2; ++choice) {
        drawText(image, font_, dialog.choices[choice],
                 256 - font_.textWidth(dialog.choices[choice]) / 2, y,
                 prompt.tint[choice].rgb);
        y += 12;
        if (choice != prompt.initial_choice) y += 4;
    }
}

} // namespace nba97
