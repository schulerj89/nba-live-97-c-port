#ifndef NBA97_GAME_STAMINA_HANDICAP_ADAPTER_H
#define NBA97_GAME_STAMINA_HANDICAP_ADAPTER_H

#include "recovered/game_match_tick.h"
#include "recovered/game_stamina_handicap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameStaminaHandicapBinding {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameStaminaHandicapMachine entry_machine;
  uint8_t entry_machine_ready;
  Nba97GameStaminaHandicapAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameStaminaHandicapProgress progress;
  Nba97MatchTickCall event;
  int result;
  size_t invocations;
  size_t completions;
} Nba97GameStaminaHandicapBinding;

/* Match tick exposes no CPU register file. The binding therefore requires an
 * independently proven full machine at 0x80068E60 after its delay slot, with
 * JAL-produced ra=0x80068E68. */
int nba97_game_stamina_handicap_from_match_tick(void *,
                                                const Nba97MatchTickCall *,
                                                Nba97GamePeriodValue *);

#ifdef __cplusplus
}
#endif
#endif
