#ifndef NBA97_GAME_TEAM_HEADER_INITIALIZE_ADAPTER_H
#define NBA97_GAME_TEAM_HEADER_INITIALIZE_ADAPTER_H

#include "recovered/game_team_header_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameTeamHeaderInitializeBinding {
  size_t operation_budget[2];
  Nba97GameTeamHeaderInitializeAccess *access_journal[2];
  size_t access_journal_capacity[2];
  size_t invocations;
  size_t completions;
  Nba97GameMatchStateResetEvent event[2];
  Nba97GameTeamHeaderInitializeProgress progress[2];
  int result[2];
} Nba97GameTeamHeaderInitializeBinding;

/* Intercept either exact 0x800655B0 call made by GAMEONLY 0x800659F0 and
 * execute the full mapped owner on the callback-live machine. */
int nba97_game_team_header_initialize_from_match_state_reset(
    void *, const Nba97GameTextMemory *, const Nba97GameMatchStateResetEvent *,
    Nba97GameMatchStateResetMachine *);

/* Execute the recovered match-state reset while routing both exact team-header
 * calls through this owner and preserving its other typed child callback. */
int nba97_game_match_state_reset_with_team_header_initialize(
    const Nba97GameMatchStateResetContext *,
    Nba97GameTeamHeaderInitializeBinding *, Nba97GameMatchStateResetProgress *);

#ifdef __cplusplus
}
#endif
#endif
