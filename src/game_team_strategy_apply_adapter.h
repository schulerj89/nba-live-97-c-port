#ifndef NBA97_GAME_TEAM_STRATEGY_APPLY_ADAPTER_H
#define NBA97_GAME_TEAM_STRATEGY_APPLY_ADAPTER_H

#include "recovered/game_match_state_reset.h"
#include "recovered/game_team_strategy_apply.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameTeamStrategyApplyResetBinding {
  size_t operation_budget;
  Nba97GameTeamStrategyApplyIo io;
  void *user;
  Nba97GameTeamStrategyApplyAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameTeamStrategyApplyProgress progress;
  Nba97GameMatchStateResetEvent event;
  int result;
  size_t invocations;
  size_t completions;
  size_t fallback_invocations;
  Nba97GameMatchStateResetIo fallback;
  void *fallback_user;
} Nba97GameTeamStrategyApplyResetBinding;

void nba97_game_team_strategy_apply_reset_binding_init(
    Nba97GameTeamStrategyApplyResetBinding *, size_t operation_budget,
    Nba97GameTeamStrategyApplyIo, void *,
    Nba97GameTeamStrategyApplyAccess *, size_t access_journal_capacity,
    Nba97GameMatchStateResetIo fallback, void *fallback_user);

int nba97_game_team_strategy_apply_from_reset(
    void *, const Nba97GameTextMemory *, const Nba97GameMatchStateResetEvent *,
    Nba97GameMatchStateResetMachine *);

#ifdef __cplusplus
}
#endif
#endif
