#ifndef NBA97_GAME_CAMERA_ELAPSED_DISPATCH_ADAPTER_H
#define NBA97_GAME_CAMERA_ELAPSED_DISPATCH_ADAPTER_H

#include "recovered/game_camera_elapsed_dispatch.h"
#include "recovered/game_camera_select.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCameraElapsedDispatchBinding {
  size_t operation_budget;
  Nba97GameCameraElapsedDispatchIo io;
  void *user;
  Nba97GameCameraElapsedDispatchAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  size_t typed_invocations;
  Nba97GameCameraSelectEvent event;
  Nba97GameCameraElapsedDispatchProgress progress;
  int result;
  int nested_result;
} Nba97GameCameraElapsedDispatchBinding;

void nba97_game_camera_elapsed_dispatch_binding_init(
    Nba97GameCameraElapsedDispatchBinding *, size_t);

int nba97_game_camera_elapsed_dispatch_from_camera_select(
    void *, const Nba97GameTextMemory *, const Nba97GameCameraSelectEvent *,
    Nba97GameCameraSelectRegisters *);

int nba97_game_camera_select_with_elapsed_dispatch(
    const Nba97GameCameraSelectContext *,
    Nba97GameCameraElapsedDispatchBinding *, Nba97GameCameraSelectProgress *);

#ifdef __cplusplus
}
#endif
#endif
