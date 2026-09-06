#ifndef NBA97_GAME_ACTOR_INPUT_ADAPTER_H
#define NBA97_GAME_ACTOR_INPUT_ADAPTER_H

#include "recovered/game_actor_input.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameActorInputBinding {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameActorInputMachine entry_machine;
  uint8_t entry_machine_ready;
  Nba97GameActorInputIo io;
  void *user;
  Nba97GameActorInputAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameActorInputProgress progress;
  int result;
  size_t invocations;
} Nba97GameActorInputBinding;

/* Compose only at match tick's natural 0x80068E8C call. The narrow tick
 * service event contains no GPR/SP/HI/LO state, so its owner must supply and
 * mark ready the independently captured 0x800686B8 entry machine. */
int nba97_game_actor_input_from_match_tick(void *, const Nba97MatchTickCall *,
                                           Nba97GamePeriodValue *);

#ifdef __cplusplus
}
#endif
#endif
