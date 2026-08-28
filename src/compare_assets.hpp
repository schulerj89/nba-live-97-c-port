#pragma once
#include "roster_database.hpp"
#include <array>

namespace nba97 {
// Bounded text/field pack extracted from FEONLY, not baked original strings.
class CompareAssets {
public:
    explicit CompareAssets(const std::filesystem::path& path);
    const std::string& text(std::size_t i) const { return texts_.at(i); }
    const std::string& label(unsigned layer,unsigned row) const;
    std::string value(const RosterDatabase&,const PlayerRecord&,unsigned layer,unsigned row) const;
private:
    struct Field { std::int16_t id; std::string label; };
    std::array<std::string,13> texts_;
    std::array<std::vector<Field>,3> fields_;
};
}
