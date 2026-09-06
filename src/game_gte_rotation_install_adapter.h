#ifndef NBA97_GAME_GTE_ROTATION_INSTALL_ADAPTER_H
#define NBA97_GAME_GTE_ROTATION_INSTALL_ADAPTER_H

#include "recovered/game_camera_frame_transform.h"
#include "recovered/game_gte_rotation_install.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameGteRotationInstallCameraBinding {
  Nba97GameGteRotationInstallWord control[32];
  size_t operation_budget;
  Nba97GameGteRotationInstallAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameGteRotationInstallControlWrite *control_journal;
  size_t control_journal_capacity;
  Nba97GameCameraFrameTransformIo fallback;
  void *fallback_user;
  Nba97GameGteRotationInstallProgress progress;
  int result;
  size_t invocations;
} Nba97GameGteRotationInstallCameraBinding;

void nba97_game_gte_rotation_install_camera_binding_init(
    Nba97GameGteRotationInstallCameraBinding *,
    const Nba97GameGteRotationInstallWord *initial_control,
    size_t operation_budget, Nba97GameGteRotationInstallAccess *, size_t,
    Nba97GameGteRotationInstallControlWrite *, size_t,
    Nba97GameCameraFrameTransformIo fallback, void *fallback_user);

int nba97_game_gte_rotation_install_from_camera(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraFrameTransformEvent *,
    Nba97GameCameraFrameTransformMachine *);

#ifdef __cplusplus
}
#endif
#endif
