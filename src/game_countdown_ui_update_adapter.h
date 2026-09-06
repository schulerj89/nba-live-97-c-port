#ifndef NBA97_GAME_COUNTDOWN_UI_UPDATE_ADAPTER_H
#define NBA97_GAME_COUNTDOWN_UI_UPDATE_ADAPTER_H

#include "recovered/game_countdown_ui_update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCountdownUiUpdateBinding {
  size_t operation_budget;
  Nba97GameCountdownUiUpdateIo io;
  void *user;
  Nba97GameCountdownUiUpdateAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameFrameUiServiceEvent event;
  Nba97GameCountdownUiUpdateProgress progress;
  int result;
} Nba97GameCountdownUiUpdateBinding;

int nba97_game_countdown_ui_update_from_frame_ui_service(
    void *, const Nba97GameTextMemory *, const Nba97GameFrameUiServiceEvent *,
    Nba97GameFrameUiServiceMachine *);

int nba97_game_frame_ui_service_with_countdown_ui_update(
    const Nba97GameFrameUiServiceContext *, Nba97GameCountdownUiUpdateBinding *,
    Nba97GameFrameUiServiceProgress *);

#ifdef __cplusplus
}
#endif
#endif
