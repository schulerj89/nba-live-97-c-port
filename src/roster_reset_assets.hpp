#pragma once
#include "psh_font.hpp"
#include "recovered/roster_reset.h"
namespace nba97 {
// Exact private 800AEDD2 descriptor; no original dialog strings in source.
class RosterResetAssets {
public:
    explicit RosterResetAssets(const std::filesystem::path& root);
    explicit RosterResetAssets(const std::vector<std::uint8_t>& bytes);
    Nba97HelpRect rect() const noexcept {return rect_;}
    const PshFont& font() const noexcept {return font_;}
    void draw(PshImage&,const Nba97ResetPrompt&,std::uint32_t ticks) const;
private:
    Nba97HelpRect rect_{};
    std::vector<std::string> lines_;
    PshFont font_;
};
}
