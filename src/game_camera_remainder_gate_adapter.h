#ifndef NBA97_GAME_CAMERA_REMAINDER_GATE_ADAPTER_H
#define NBA97_GAME_CAMERA_REMAINDER_GATE_ADAPTER_H

#include "game_camera_state_lookup_adapter.h"
#include "recovered/game_camera_elapsed_dispatch.h"
#include "recovered/game_camera_remainder_gate.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCameraRemainderGateBinding {
  size_t operation_budget;
  Nba97GameCameraRemainderGateAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameCameraElapsedDispatchEvent event;
  Nba97GameCameraRemainderGateProgress progress;
  int result;
} Nba97GameCameraRemainderGateBinding;

void nba97_game_camera_remainder_gate_binding_init(
    Nba97GameCameraRemainderGateBinding *, size_t);

int nba97_game_camera_remainder_gate_from_elapsed_dispatch(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraElapsedDispatchEvent *,
    Nba97GameCameraElapsedDispatchMachine *);

int nba97_game_camera_elapsed_dispatch_with_remainder_gate(
    const Nba97GameCameraElapsedDispatchContext *,
    Nba97GameCameraRemainderGateBinding *, Nba97GameCameraStateLookupBinding *,
    Nba97GameCameraElapsedDispatchProgress *);

#ifdef __cplusplus
}
#endif
#endif
