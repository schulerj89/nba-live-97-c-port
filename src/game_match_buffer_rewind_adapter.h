#ifndef NBA97_GAME_MATCH_BUFFER_REWIND_ADAPTER_H
#define NBA97_GAME_MATCH_BUFFER_REWIND_ADAPTER_H

#include "recovered/game_match_buffer_initialize.h"
#include "recovered/game_match_buffer_rewind.h"
#include "recovered/game_memory_zero.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMatchBufferRewindBinding {
  Nba97GameMatchBufferInitializeEvent buffer_event;
  size_t operation_budget;
  size_t zero_operation_budget;
  Nba97GameMatchBufferRewindAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  size_t zero_invocations;
  size_t zero_completions;
  Nba97GameMatchStateResetEvent event;
  Nba97GameMatchBufferRewindProgress progress;
  Nba97GameMemoryZeroProgress zero_progress;
  int result;
  int nested_result;
  int zero_result;
} Nba97GameMatchBufferRewindBinding;

/* Compose the exact length-four 0x800A3A74 event through the recovered zero
 * owner while retaining its full-machine scratch and failure prefixes. */
int nba97_game_match_buffer_rewind_compose_zero(
    void *, const Nba97GameTextMemory *,
    const Nba97GameMatchBufferRewindEvent *,
    Nba97GameMatchBufferRewindMachine *);

/* Intercept only the exact mode-98 match-state-reset call at 0x80065AE8. */
int nba97_game_match_buffer_rewind_from_match_state_reset(
    void *, const Nba97GameTextMemory *, const Nba97GameMatchStateResetEvent *,
    Nba97GameMatchStateResetMachine *);

/* Exact non-98 buffer initializer call; composes the same rewind and zero
 * owners. */
int nba97_game_match_buffer_rewind_from_match_buffer_initialize(
    void *, const Nba97GameTextMemory *,
    const Nba97GameMatchBufferInitializeEvent *,
    Nba97GameMatchBufferInitializeMachine *);

/* Execute the real reset owner with its mode-98 rewind call composed and all
 * other reset children preserved through the parent's typed callback. */
int nba97_game_match_state_reset_with_match_buffer_rewind(
    const Nba97GameMatchStateResetContext *,
    Nba97GameMatchBufferRewindBinding *, Nba97GameMatchStateResetProgress *);

#ifdef __cplusplus
}
#endif
#endif
