#ifndef NBA97_GAME_MATCH_BUFFER_INITIALIZE_ADAPTER_H
#define NBA97_GAME_MATCH_BUFFER_INITIALIZE_ADAPTER_H

#include "recovered/game_match_buffer_initialize.h"
#include "recovered/game_match_state_reset.h"
#include "recovered/game_memory_zero.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMatchBufferInitializeBinding {
  size_t operation_budget;
  size_t zero_operation_budget;
  Nba97GameMatchBufferInitializeIo io;
  void *user;
  Nba97GameMatchStateResetIo fallback;
  void *fallback_user;
  Nba97GameMatchBufferInitializeAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameMatchBufferInitializeProgress progress;
  Nba97GameMemoryZeroProgress zero_progress;
  Nba97GameMatchStateResetEvent event;
  int result;
  int nested_result;
  int zero_result;
  size_t invocations;
  size_t completions;
  size_t zero_invocations;
  size_t child_80076AD0_invocations;
} Nba97GameMatchBufferInitializeBinding;

/* Intercept only BN's exact non-mode-98 0x80065AF8 event. Other reset
 * services, including the mode-98 0x80076AD0 branch, use fallback unchanged. */
int nba97_game_match_buffer_initialize_from_match_state_reset(
    void *, const Nba97GameTextMemory *, const Nba97GameMatchStateResetEvent *,
    Nba97GameMatchStateResetMachine *);

#ifdef __cplusplus
}
#endif
#endif
