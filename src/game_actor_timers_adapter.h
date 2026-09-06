#ifndef NBA97_GAME_ACTOR_TIMERS_ADAPTER_H
#define NBA97_GAME_ACTOR_TIMERS_ADAPTER_H

#include "recovered/game_actor_timers.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameActorTimersBinding {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameActorTimersMachine entry_machine;
  uint8_t entry_machine_ready;
  Nba97GameActorTimersAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameActorTimersProgress progress;
  Nba97MatchTickCall event;
  int result;
  size_t invocations;
  size_t completions;
} Nba97GameActorTimersBinding;

/* Bind only match tick's 0x80068E38 event. Its legacy service interface has
 * no CPU state, so entry_machine_ready must identify an independently proven
 * full machine whose JAL-produced ra is 0x80068E40. */
int nba97_game_actor_timers_from_match_tick(void *, const Nba97MatchTickCall *,
                                            Nba97GamePeriodValue *);

#ifdef __cplusplus
}
#endif
#endif
