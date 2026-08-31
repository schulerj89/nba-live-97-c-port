#pragma once
#include "match_runtime.hpp"
#include "recovered/game_player_update.h"

namespace nba97 {
struct MatchPlayerUpdateResult {
    int result=NBA97_PLAYER_UPDATE_ARGUMENT;
    int dependency_result=0,pending_owner=-1;
    bool published=false;
    std::string detail;
    Nba97GamePlayerUpdateReceipt receipt{};
    std::array<MatchRuntimeAnimationResult,10> animation;
    std::array<Nba97GamePhysicsReceipt,10> physics{};
};
// Complete6801C with actual579FC/6CFE0/706E4. Reads the ten live table entries,
// retaining aliases, captured entity identity and original team-context order.
// Missing rule/audio callbacks stop at the first reached owner; they are never
// treated as successful no-ops. The prefix is diagnostic, not a continuation.
// Only a complete update publishes its staged fields. This native transaction
// does not claim the original game rolled back earlier writes on a source trap.
MatchPlayerUpdateResult updateMatchRuntimePlayers(MatchRuntimeState&,
                                                   const GameplayAnimationResource&);
}
