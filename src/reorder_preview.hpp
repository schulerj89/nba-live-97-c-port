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
    const PshFont& font() const noexcept { return font_; }
private:
    PshFont font_;
};
}
