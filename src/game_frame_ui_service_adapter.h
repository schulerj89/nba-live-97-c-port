#ifndef NBA97_GAME_FRAME_UI_SERVICE_ADAPTER_H
#define NBA97_GAME_FRAME_UI_SERVICE_ADAPTER_H

#include "recovered/game_frame_ui_service.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameFrameUiServiceBinding {
  Nba97GameTextMemory memory;
  const Nba97GameFrameUiServiceMachine *explicit_caller_machine;
  size_t operation_budget;
  Nba97GameFrameUiServiceIo io;
  void *user;
  Nba97GameFrameUiServiceAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97MatchTickCall event;
  Nba97GameFrameUiServiceProgress progress;
  int result;
} Nba97GameFrameUiServiceBinding;

int nba97_game_frame_ui_service_from_match_tick(void *,
                                                const Nba97MatchTickCall *,
                                                Nba97GamePeriodValue *);

int nba97_game_match_tick_with_frame_ui_service(
    const Nba97MatchTickContext *, Nba97GameFrameUiServiceBinding *,
    Nba97MatchTickProgress *);

#ifdef __cplusplus
}
#endif
#endif
