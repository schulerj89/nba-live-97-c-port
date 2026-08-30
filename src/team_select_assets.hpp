#pragma once
#include "frontend_help.hpp"
#include "frontend_palette_assets.hpp"
#include "roster_database.hpp"
#include "recovered/team_ratings.h"
#include "roster_ratings.hpp"
#include <array>

namespace nba97 {
struct TeamSelectLayout { int16_t x,y,z,flags; std::string tag; };
struct TeamSelectNames { std::string city,nickname,logo; };
class TeamSelectAssets {
public:
    explicit TeamSelectAssets(const std::filesystem::path& root);
    Nba97TeamRanks ranks(const RosterDatabase&, uint16_t (*scores)[29]=nullptr) const;
    const std::array<TeamSelectLayout,18>& layout() const { return layout_; }
    const TeamSelectNames& team(unsigned id) const { return teams_.at(id); }
    const std::string& criterion(unsigned index) const { return criteria_.at(index); }
    const std::string& heading() const { return heading_; }
    const FrontendPaletteAssets& backgrounds() const { return backgrounds_; }
    const FrontendHelpPack& help() const { return help_; }
    const std::array<uint32_t,6>& initialRng() const { return rng_; }
    const std::array<int16_t,29>& ratingAdjustments() const { return adjustments_; }
private:
    std::array<uint32_t,6> rng_{};
    std::array<int16_t,29> adjustments_{};
    std::array<TeamSelectNames,31> teams_;
    std::array<TeamSelectLayout,18> layout_;
    std::array<std::string,5> criteria_;
    std::string heading_;
    FrontendPaletteAssets backgrounds_;
    FrontendHelpPack help_;
};
}
