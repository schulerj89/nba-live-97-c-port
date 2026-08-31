#pragma once
#include "match_runtime.hpp"
#include "recovered/game_player_input.h"

namespace nba97 {
struct MatchInputCamera {
    Nba97GamePeriodValue direction_d8eec{},mode_fc99c{},flip_fa378{};
};
struct MatchInputEdgeResult {
    int result=NBA97_INPUT_ARGUMENT;
    bool published=false;
    std::string detail;
    Nba97GameInputReceipt receipt{};
};
// Actual700E4 and internal7A498 on one explicitly owned controller. mappedMask
// is the FULL result of original2D2DC, not a host button constant. The source
// returned edge mask also remains full32, independent of its low16 record store.
// Camera values are explicit source inputs; neutral direction8 does not need
// them. No source camera initialization or device mapping is invented here.
// Only receipt stores are published, retaining unmodified partial knownness,
// table aliases and the two controller2A writes. Refusal retains its diagnostic
// receipt but publishes no records; this does not claim source rollback.
// Does not run686B8,61760, jump, countdown, or a frame of the game loop.
MatchInputEdgeResult updateMatchRuntimeInputEdges(MatchRuntimeState&,
    Nba97GamePeriodReference controller,std::uint32_t mappedMask,const MatchInputCamera&);
}
