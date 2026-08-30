#pragma once
#include "recovered/match_controls.h"
#include "user_profiles.hpp"
#include <array>
#include <vector>

namespace nba97 {
struct MatchControlResult {
    Nba97MatchControls controls{};
    std::array<uint8_t,8> provenance{};
    std::array<uint64_t,8> profile_ids{}; // Zero for negative or cleared slots.
};
// Read-only fixed-slot adapter. Caller supplies live maps/defaults explicitly;
// this function never initializes missing maps, reads assets or writes saves.
// A missing saved slot models a cleared record (valid0), including neutral
// controllers whose selector survives another controller's accepted deletion.
MatchControlResult finalizeMatchControls(const Nba97MatchControls& live,
    const std::array<int8_t,8>& selectors,const std::vector<UserProfile>& profiles,
    const std::array<uint8_t,59>& defaults,bool force_defaults=false);
}
