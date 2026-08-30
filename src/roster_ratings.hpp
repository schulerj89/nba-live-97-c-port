#pragma once
#include "roster_database.hpp"
#include "recovered/team_ratings.h"
namespace nba97 {
// Shared by Team Select and match capture: always current accepted rosters.
Nba97TeamRanks calculateRosterRanks(const RosterDatabase&,
    const std::array<int16_t,29>& adjustments,uint16_t (*scores)[29]=nullptr);
}
