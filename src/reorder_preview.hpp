#pragma once
#include "psh_font.hpp"
#include "roster_database.hpp"

namespace nba97 {
// Asset-backed diagnostic label layer, NOT the finished original screen.
// No substitute font, background, portraits, or placeholder card art.
class ReorderLabelPreview {
public:
    explicit ReorderLabelPreview(const std::filesystem::path& asset_root);
    PshImage render(const Nba97ReorderSession& session, const RosterDatabase& database) const;
    // Native feedback surface, not the full original portrait/background screen.
    // modal_frame >= 0 grows from the original small rectangle; negative values
    // shrink the already-open rectangle. Caller owns frame progression.
    PshImage renderFeedback(const Nba97ReorderSession& session, const RosterDatabase& database,
                            std::uint16_t team_id, int modal_frame = 32,
                            bool discard_yes = false) const;
    const PshFont& font() const noexcept { return font_; }
private:
    PshFont font_;
    PshFont small_font_;
    struct Dialog { int x, y, width, height; std::vector<std::string> lines; };
    std::vector<Dialog> dialogs_;
    Dialog discard_{};
};
}
