#ifndef NBA97_GAME_CONTROLLER_PROFILE_RESET_ADAPTER_H
#define NBA97_GAME_CONTROLLER_PROFILE_RESET_ADAPTER_H

#include "recovered/game_controller_profile_reset.h"
#include "recovered/game_match_state_reset.h"
#include "recovered/game_memory_zero.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameControllerProfileResetBinding {
  size_t operation_budget;
  size_t zero_operation_budget;
  Nba97GameControllerProfileResetAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  size_t zero_invocations;
  Nba97GameMatchStateResetEvent event;
  Nba97GameControllerProfileResetProgress progress;
  Nba97GameMemoryZeroProgress zero_progress;
  int result;
  int nested_result;
} Nba97GameControllerProfileResetBinding;

void nba97_game_controller_profile_reset_binding_init(
    Nba97GameControllerProfileResetBinding *, size_t, size_t);

/* Intercept only BN's exact 0x80065A38 controller-reset event. */
int nba97_game_controller_profile_reset_from_match_state_reset(
    void *, const Nba97GameTextMemory *, const Nba97GameMatchStateResetEvent *,
    Nba97GameMatchStateResetMachine *);

/* Run the recovered BN parent with its 0x80083490 child routed through BP;
 * all other BN children continue through the caller's typed fallback. */
int nba97_game_match_state_reset_with_controller_profile_reset(
    const Nba97GameMatchStateResetContext *,
    Nba97GameControllerProfileResetBinding *,
    Nba97GameMatchStateResetProgress *);

#ifdef __cplusplus
}
#endif
#endif
