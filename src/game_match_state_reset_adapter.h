#ifndef NBA97_GAME_MATCH_STATE_RESET_ADAPTER_H
#define NBA97_GAME_MATCH_STATE_RESET_ADAPTER_H

#include "recovered/game_match_state_reset.h"
#include "recovered/game_memory_zero.h"
#include "recovered/game_roster_bindings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Nba97GameMatchStateResetHiLoProvider)(
    void *, const Nba97GameTextMemory *, const Nba97GameMatchInitializeEvent *,
    Nba97GameMatchStateResetWord *, Nba97GameMatchStateResetWord *);

typedef struct Nba97GameMatchStateResetBinding {
  size_t operation_budget;
  size_t zero_operation_budget[4];
  size_t roster_operation_budget;
  Nba97GameMatchStateResetIo io;
  void *user;
  Nba97GameMatchStateResetHiLoProvider hi_lo_provider;
  void *hi_lo_user;
  Nba97GameMatchStateResetAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameRosterBindingsAccess *roster_journal;
  size_t roster_journal_capacity;
  size_t invocations;
  size_t completions;
  size_t unresolved_callbacks_completed;
  size_t zero_invocations;
  size_t roster_invocations;
  Nba97GameMatchInitializeEvent event;
  Nba97GameMatchStateResetProgress progress;
  Nba97GameMemoryZeroProgress zero_progress[4];
  Nba97GameRosterBindingsProgress roster_progress;
  int result;
  int nested_result;
  int zero_result[4];
  int roster_result;
} Nba97GameMatchStateResetBinding;

/* Compose one exact 0x800A3A74 event through the recovered zero owner while
 * retaining the source-proved full-machine scratch prefix. */
int nba97_game_match_state_reset_compose_zero(
    void *, const Nba97GameTextMemory *, const Nba97GameMatchStateResetEvent *,
    Nba97GameMatchStateResetMachine *);

/* Intercept only the initializer's proven 0x8002DBF8 call. A null HI/LO
 * provider creates explicit unknown HI/LO words; no ABI value is invented. */
int nba97_game_match_state_reset_from_match_initialize(
    void *, const Nba97GameTextMemory *, const Nba97GameMatchInitializeEvent *,
    Nba97GameMatchInitializeRegisters *);

/* Execute the recovered initializer while routing its exact 0x800659F0 child
 * through the reset owner and preserving the parent's other typed children. */
int nba97_game_match_initialize_with_state_reset(
    const Nba97GameMatchInitializeContext *, Nba97GameMatchStateResetBinding *,
    Nba97GameMatchInitializeProgress *);

#ifdef __cplusplus
}
#endif
#endif
