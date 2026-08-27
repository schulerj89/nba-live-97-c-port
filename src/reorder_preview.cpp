#include "reorder_preview.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace nba97 {
ReorderLabelPreview::ReorderLabelPreview(const std::filesystem::path& asset_root)
    : font_(load_psh_font(asset_root / "fonts" / "ZFONT0.PSH", 10, 1)),
      small_font_(load_psh_font(asset_root / "fonts" / "ZFONT1.PSH", 10, 1)) {
    std::ifstream input(asset_root / "reorder/dialogs.n97ui", std::ios::binary);
    if (!input) throw std::runtime_error("missing private reorder/dialogs.n97ui; run extract_reorder_dialogs.py");
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
    const std::vector<unsigned char> magic{'N','9','7','D',1,0,0,0};
    if (bytes.size() < 8 || !std::equal(magic.begin(), magic.end(), bytes.begin()))
        throw std::runtime_error("invalid Re-order dialog pack");
    std::size_t at = 8;
    for (int record = 0; record < 2; ++record) {
        if (at + 4 > bytes.size()) throw std::runtime_error("truncated dialog size");
        const std::size_t size = bytes[at] | (bytes[at+1] << 8) |
            (static_cast<std::uint32_t>(bytes[at+2]) << 16) | (static_cast<std::uint32_t>(bytes[at+3]) << 24);
        at += 4;
        if (size < 10 || size > bytes.size() - at) throw std::runtime_error("truncated dialog record");
        const auto end = at + size;
        auto half = [&](int offset) { return bytes[at+offset] | (bytes[at+offset+1] << 8); };
        Dialog d{half(0), half(2), half(4), bytes[at+6], {}};
        const int lines = bytes[at+8];
        if (bytes[at+7] != 1 || bytes[at+9] || lines < 1 || lines > 4 ||
            d.x + d.width > 512 || d.y + d.height > 240 || !d.width || !d.height)
            throw std::runtime_error("unsupported original dialog layout");
        at += 10;
        for (int line = 0; line < lines; ++line) {
            if (at >= end || bytes[at++] != 1) throw std::runtime_error("invalid dialog alignment");
            std::string text;
            while (at < end && bytes[at]) text += static_cast<char>(bytes[at++]);
            if (at == end) throw std::runtime_error("unterminated dialog line");
            ++at;
            d.lines.push_back(text);
        }
        if (at != end) throw std::runtime_error("unexpected dialog payload");
        dialogs_.push_back(std::move(d));
    }
    if (at != bytes.size()) throw std::runtime_error("unexpected trailing dialog data");
    std::ifstream discard_input(asset_root / "reorder/discard.n97ui", std::ios::binary);
    if (discard_input) {
        const std::vector<unsigned char> d((std::istreambuf_iterator<char>(discard_input)), {});
        if (d.size() < 10 || d[7] != 1 || d[8] != 4 || d[9] != 2)
            throw std::runtime_error("invalid private discard descriptor");
        discard_ = {d[0] | (d[1]<<8), d[2] | (d[3]<<8), d[4] | (d[5]<<8), d[6], {}};
        std::size_t pos = 10;
        for (int line = 0; line < 6; ++line) {
            if (pos >= d.size() || d[pos++] != 1) throw std::runtime_error("invalid discard line");
            std::string value;
            while (pos < d.size() && d[pos]) value += static_cast<char>(d[pos++]);
            if (pos == d.size()) throw std::runtime_error("truncated discard line");
            ++pos; discard_.lines.push_back(value);
        }
        if (pos != d.size() || discard_.x != 141 || discard_.y != 80 ||
            discard_.width != 230 || discard_.height != 100)
            throw std::runtime_error("unsupported discard geometry");
    }
}

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

PshImage ReorderLabelPreview::renderFeedback(const Nba97ReorderSession& s,
        const RosterDatabase& db, std::uint16_t team_id, int modal_frame, bool discard_yes) const {
    PshImage image;
    image.tag = "reorder-feedback-diagnostic";
    image.width = 512; image.height = 240;
    image.rgba.assign(512 * 240 * 4, 0);
    const auto* team = db.team(team_id);
    if (!team) throw std::runtime_error("invalid Re-order feedback team");
    // FUN_80055068 redraws 0x73: type 0x0F, team city + nickname, not an
    // invented 'choose replacement' caption. FUN_80056494 places it at 256,70.
    draw_psh_text_centered(image, font_, team->city + " " + team->nickname, 256, 70);
    static const char* positions[] = {"c", "pf", "sf", "sg", "pg", "ir"};
    for (int column = 0; column < 2; ++column) {
        if (s.top[column] > 9 || s.cursor[column] >= 15) throw std::runtime_error("invalid Re-order viewport");
        const int origin = column == 0 ? 60 : 270;
        const auto* selected = db.player(s.selected_ids[column]);
        // 0x8002654C is an underscore, NOT an empty string. Bench entries use it.
        std::string natural = "_";
        if (s.cursor[column] < 5 && selected && selected->position < 6)
            natural = std::string("natural pos: ") + positions[selected->position];
        draw_psh_text_centered(image, small_font_, natural,
            origin + small_font_.textWidth(natural) / 2, 95);
        for (int row = 0; row < 6; ++row) {
            const int slot = s.top[column] + row;
            const auto* p = db.player(s.row_ids[column][slot]);
            if (!p && s.row_ids[column][slot] != UINT16_MAX)
                throw std::runtime_error("unknown Re-order row player");
            PshImage layer = image;
            std::fill(layer.rgba.begin(), layer.rgba.end(), std::uint8_t{0});
            auto text = [&](const std::string& value, int x) {
                for (char c : value) if (c != ' ' && !font_.glyph(c))
                    throw std::runtime_error("missing original Re-order glyph");
                draw_psh_text_centered(layer, font_, value, x + font_.textWidth(value)/2, 112 + row*16);
            };
            if (!p) text("empty", origin + 45);
            else {
                std::string pos = positions[std::min<unsigned>(p->position, 5)];
                if (slot < 5) {
                    pos = positions[slot];
                    for (char& c : pos) c = static_cast<char>(c - ('a'-'A'));
                }
                // 55CC4/55E10: position right edge +28, jersey +58, surname +66.
                text(pos, origin + 28 - font_.textWidth(pos));
                const auto number = p->jerseyNumberText();
                text(number, origin + 58 - font_.textWidth(number));
                text(p->last_name, origin + 66);
            }
            for (std::size_t i = 0; i < layer.rgba.size(); i += 4) if (layer.rgba[i+3]) {
                for (int c = 0; c < 3; ++c)
                    image.rgba[i+c] = static_cast<std::uint8_t>(std::min(255,
                        layer.rgba[i+c] * s.tint[column][slot].rgb[c] / 128));
                image.rgba[i+3] = 255;
            }
        }
    }
    if (s.modal != NBA97_REORDER_MODAL_NONE || s.phase == NBA97_REORDER_DISCARD_PROMPT) {
        const bool discard = s.phase == NBA97_REORDER_DISCARD_PROMPT;
        const auto& d = discard ? discard_ : dialogs_.at(s.modal == NBA97_REORDER_MODAL_EMPTY ? 0 : 1);
        if (discard && d.lines.empty()) throw std::runtime_error("missing private reorder/discard.n97ui");
        const int frame = std::clamp(modal_frame, -64, 64);
        // 30430/30784/309DC: expand from (246,110,20,10), ±(9,4,18,8)
        // per UI update, clamped independently. Not a full-screen help overlay.
        const int x = frame >= 0 ? std::max(d.x, 246-9*frame) : std::min(246, d.x-9*frame);
        const int y = frame >= 0 ? std::max(d.y, 110-4*frame) : std::min(110, d.y-4*frame);
        const int w = frame >= 0 ? std::min(d.width, 20+18*frame) : std::max(20, d.width+18*frame);
        const int h = frame >= 0 ? std::min(d.height, 10+8*frame) : std::max(10, d.height+8*frame);
        for (int yy = 0; yy < h; ++yy) for (int xx = 0; xx < w; ++xx) {
            // Original warning G4: dark red TL/BR, brighter red TR/BL.
            const int mix = std::min((xx * 256 / w + yy * 256 / h),
                                    512 - (xx * 256 / w + yy * 256 / h));
            const auto at = ((y+yy)*512 + x+xx)*4;
            image.rgba[at] = static_cast<std::uint8_t>(20 + 80*mix/256);
            image.rgba[at+1] = image.rgba[at+2] = static_cast<std::uint8_t>(10 - 10*mix/256);
            image.rgba[at+3] = 255;
        }
        if (frame >= 0 && x == d.x && y == d.y && w == d.width && h == d.height) {
            int baseline = d.y + 15;
            int row = 0;
            for (auto line : d.lines) {
                const auto marker = line.find("%s");
                if (marker != std::string::npos)
                    line.replace(marker, 2, s.modal == NBA97_REORDER_MODAL_VIEW_EMPTY ? "view player" : "compare players");
                const bool choice = discard && row >= 4;
                if (discard && row == 4) baseline += 6;
                if (choice && (row == 4) == discard_yes)
                    draw_psh_text_centered(image, small_font_, std::string(1, static_cast<char>(static_cast<unsigned char>(0x8a))), d.x + 12, baseline);
                draw_psh_text_centered(image, small_font_, line, 256, baseline);
                ++row;
                baseline += 12;
            }
            // 40A1C inserts 6 extra pixels before this final line.
            if (!discard) draw_psh_text_centered(image, small_font_, "Hit any button to continue", 256, baseline+6);
        }
    }
    return image;
}
}
