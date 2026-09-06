#ifndef NBA97_GAME_CAMERA_PHASE_SELECT_ADAPTER_H
#define NBA97_GAME_CAMERA_PHASE_SELECT_ADAPTER_H

#include "recovered/game_camera_phase_select.h"
#include "recovered/game_camera_select.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCameraPhaseSelectBinding {
  size_t operation_budget;
  size_t camera_operation_budget;
  Nba97GameCameraPhaseSelectIo io;
  void *user;
  Nba97GameCameraPhaseSelectAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameCameraSelectIo camera_io;
  void *camera_user;
  Nba97GameCameraSelectAccess *camera_access_journal;
  size_t camera_access_journal_capacity;
  size_t invocations;
  size_t completions;
  size_t camera_invocations;
  size_t typed_invocations;
  Nba97GameCameraSelectEvent event;
  Nba97GameCameraPhaseSelectProgress progress;
  Nba97GameCameraSelectProgress camera_progress;
  int result;
  int nested_result;
} Nba97GameCameraPhaseSelectBinding;

void nba97_game_camera_phase_select_binding_init(
    Nba97GameCameraPhaseSelectBinding *, size_t, size_t);

int nba97_game_camera_phase_select_from_camera_select(
    void *, const Nba97GameTextMemory *, const Nba97GameCameraSelectEvent *,
    Nba97GameCameraSelectRegisters *);

int nba97_game_camera_select_with_phase_select(
    const Nba97GameCameraSelectContext *, Nba97GameCameraPhaseSelectBinding *,
    Nba97GameCameraSelectProgress *);

#ifdef __cplusplus
}
#endif
#endif
