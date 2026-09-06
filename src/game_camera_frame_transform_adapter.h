#ifndef NBA97_GAME_CAMERA_FRAME_TRANSFORM_ADAPTER_H
#define NBA97_GAME_CAMERA_FRAME_TRANSFORM_ADAPTER_H

#include "recovered/game_camera_frame_transform.h"
#include "recovered/game_match_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATCH_FRAME_CHILD_INCOMPLETE = -21 };

typedef struct Nba97GameCameraFrameTransformMatchFrameBinding {
  /* The state-level match-frame callback has no GPR or retained-memory
   * payload. These are explicit independent inputs; the adapter validates the
   * source JAL return address and never manufactures a machine ABI. */
  Nba97GameTextMemory memory;
  Nba97GameCameraFrameTransformMachine entry_machine;
  size_t operation_budget;
  Nba97GameCameraFrameTransformIo io;
  void *user;
  Nba97GameCameraFrameTransformAccess *access_journal;
  size_t access_journal_capacity;
  Nba97MatchFrameIo fallback;
  void *fallback_user;
  Nba97GameCameraFrameTransformProgress progress;
  int result;
  size_t invocations;
} Nba97GameCameraFrameTransformMatchFrameBinding;

void nba97_game_camera_frame_transform_match_frame_binding_init(
    Nba97GameCameraFrameTransformMatchFrameBinding *binding,
    const Nba97GameTextMemory *memory,
    const Nba97GameCameraFrameTransformMachine *entry_machine,
    size_t operation_budget, Nba97GameCameraFrameTransformIo io, void *user,
    Nba97GameCameraFrameTransformAccess *access_journal,
    size_t access_journal_capacity, Nba97MatchFrameIo fallback,
    void *fallback_user);

int nba97_game_camera_frame_transform_from_match_frame(
    void *user, const Nba97MatchFrameCall *call, Nba97GamePeriodValue *value);

#ifdef __cplusplus
}
#endif
#endif
