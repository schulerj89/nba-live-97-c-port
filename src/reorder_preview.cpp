#include "reorder_preview.hpp"
#include <stdexcept>

namespace nba97 {
ReorderLabelPreview::ReorderLabelPreview(const std::filesystem::path& asset_root)
    : font_(load_psh_font(asset_root / "fonts" / "ZFONT0.PSH", 10, 1)) {}

PshImage ReorderLabelPreview::render(const Nba97ReorderSession& session,
                                     const RosterDatabase& database) const {
    PshImage image;
    image.tag = "reorder-label-diagnostic";
    image.width = 512;
    image.height = 240;
    image.rgba.assign(512 * 240 * 4, 0); // Transparent: no invented background.
    for (int column = 0; column < 2; ++column) {
        if (session.top[column] > 9) throw std::runtime_error("invalid Re-order viewport");
        for (int row = 0; row < 6; ++row) {
            const auto id = session.slots[session.top[column] + row];
            if (id == UINT16_MAX) continue;
            const auto* player = database.player(id);
            if (!player) throw std::runtime_error("Re-order label refers to unknown player");
            // Diagnostic surnames use the real database and original glyphs.
            // Full original row composition remains scoped to the screen slice.
            for (char ch : player->last_name)
                if (ch != ' ' && !font_.glyph(ch))
                    throw std::runtime_error("missing original Re-order name glyph");
            const int x = column == 0 ? 60 : 270;
            draw_psh_text_centered(image, font_, player->last_name,
                x + font_.textWidth(player->last_name) / 2, 96 + row * 16);
        }
    }
    return image;
}
}
