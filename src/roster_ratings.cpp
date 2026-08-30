#include "roster_ratings.hpp"
#include <algorithm>
#include <stdexcept>
namespace nba97 {
Nba97TeamRanks calculateRosterRanks(const RosterDatabase& database,
        const std::array<int16_t,29>& adjustments,uint16_t (*scores)[29]) {
    std::array<Nba97TeamRatingInput,29> input{};
    for(unsigned team=0;team<29;++team) {
        const auto players=database.resolveTeamSlots(static_cast<int16_t>(team));
        bool empty=false;
        for(unsigned slot=0;slot<15;++slot) {
            if(!players[slot]) {empty=true;continue;}
            if(empty) throw std::runtime_error("ratings require resolved contiguous current rosters");
            ++input[team].count;
            std::copy(players[slot]->ratings.begin(),players[slot]->ratings.end(),input[team].ratings[slot]);
        }
    }
    uint16_t local[5][29];Nba97TeamRanks result{};
    if(!nba97_team_ratings(input.data(),adjustments.data(),scores ? scores:local,&result))
        throw std::runtime_error("ranking requires 8..15 resolved current players per regular team");
    return result;
}
}
