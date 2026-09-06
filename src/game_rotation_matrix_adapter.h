#ifndef NBA97_GAME_ROTATION_MATRIX_ADAPTER_H
#define NBA97_GAME_ROTATION_MATRIX_ADAPTER_H

#include "recovered/game_camera_frame_transform.h"
#include "recovered/game_rotation_matrix.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameRotationMatrixBinding {
  size_t operation_budget;
  Nba97GameRotationMatrixAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameRotationMatrixProgress progress;
  int result;
  size_t invocations;
} Nba97GameRotationMatrixBinding;

void nba97_game_rotation_matrix_binding_init(Nba97GameRotationMatrixBinding *,
                                             size_t,
                                             Nba97GameRotationMatrixAccess *,
                                             size_t);

int nba97_game_rotation_matrix_from_camera_frame_transform(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraFrameTransformEvent *,
    Nba97GameCameraFrameTransformMachine *);

#ifdef __cplusplus
}
#endif
#endif
