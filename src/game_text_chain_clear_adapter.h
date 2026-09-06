#ifndef NBA97_GAME_TEXT_CHAIN_CLEAR_ADAPTER_H
#define NBA97_GAME_TEXT_CHAIN_CLEAR_ADAPTER_H

#include "recovered/game_text_chain_clear.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameTextChainClearBinding {
  size_t operation_budget;
  Nba97GameTextChainClearAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameCountdownUiUpdateEvent event;
  Nba97GameTextChainClearProgress progress;
  int result;
} Nba97GameTextChainClearBinding;

int nba97_game_text_chain_clear_from_countdown_ui_update(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCountdownUiUpdateEvent *,
    Nba97GameCountdownUiUpdateMachine *);

int nba97_game_countdown_ui_update_with_text_chain_clear(
    const Nba97GameCountdownUiUpdateContext *, Nba97GameTextChainClearBinding *,
    Nba97GameCountdownUiUpdateProgress *);

#ifdef __cplusplus
}
#endif
#endif
