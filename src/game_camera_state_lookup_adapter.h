#ifndef NBA97_GAME_CAMERA_STATE_LOOKUP_ADAPTER_H
#define NBA97_GAME_CAMERA_STATE_LOOKUP_ADAPTER_H

#include "recovered/game_camera_elapsed_dispatch.h"
#include "recovered/game_camera_state_lookup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCameraStateLookupBinding {
  size_t operation_budget;
  Nba97GameCameraStateLookupAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameCameraElapsedDispatchEvent event;
  Nba97GameCameraStateLookupProgress progress;
  int result;
} Nba97GameCameraStateLookupBinding;

void nba97_game_camera_state_lookup_binding_init(
    Nba97GameCameraStateLookupBinding *, size_t);

int nba97_game_camera_state_lookup_from_elapsed_dispatch(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraElapsedDispatchEvent *,
    Nba97GameCameraElapsedDispatchMachine *);

int nba97_game_camera_elapsed_dispatch_with_state_lookup(
    const Nba97GameCameraElapsedDispatchContext *,
    Nba97GameCameraStateLookupBinding *,
    Nba97GameCameraElapsedDispatchProgress *);

#ifdef __cplusplus
}
#endif
#endif
