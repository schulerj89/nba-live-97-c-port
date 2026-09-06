#ifndef NBA97_GAME_TEAM_TACTICS_UPDATE_ADAPTER_H
#define NBA97_GAME_TEAM_TACTICS_UPDATE_ADAPTER_H

#include "recovered/game_match_tick.h"
#include "recovered/game_team_tactics_update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameTeamTacticsBinding {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameTeamTacticsMachine entry_machine;
  uint8_t entry_machine_ready;
  Nba97GameTeamTacticsIo io;
  void *io_user;
  Nba97GameTeamTacticsAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameTeamTacticsProgress progress;
  Nba97MatchTickCall event;
  int result;
  size_t invocations;
  size_t completions;
} Nba97GameTeamTacticsBinding;

/* The match-tick callback supplies no CPU registers.  The binding therefore
 * requires an independent full-machine snapshot after the 0x80068E2C delay,
 * including the JAL-produced ra=0x80068E30. */
int nba97_game_team_tactics_update_from_match_tick(void *,
                                                   const Nba97MatchTickCall *,
                                                   Nba97GamePeriodValue *);

#ifdef __cplusplus
}
#endif
#endif
