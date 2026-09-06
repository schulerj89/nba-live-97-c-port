#ifndef NBA97_GAME_CAMERA_OVERRIDE_END_ADAPTER_H
#define NBA97_GAME_CAMERA_OVERRIDE_END_ADAPTER_H

#include "recovered/game_camera_override_end.h"
#include "recovered/game_controller_selection.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCameraOverrideEndSelectionBinding {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCameraOverrideEndMachine entry_machine;
  uint8_t entry_machine_ready;
  Nba97GameCameraOverrideEndIo io;
  void *user;
  Nba97GameCameraOverrideEndAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameCameraOverrideEndProgress progress;
  Nba97GameSelectionEffects selection_effects;
  int selection_result;
  int tail_result;
  size_t invocations;
} Nba97GameCameraOverrideEndSelectionBinding;

/* Execute the actual normalized controller-selection owner, apply its ordered
 * writes, execute 0x8007A36C only when requested using an explicitly supplied
 * full machine, then publish the tail-state write. The normalized caller does
 * not provide a live GPR/SP bridge, so entry_machine_ready is mandatory. */
int nba97_game_camera_override_end_from_selection(
    Nba97GameCameraOverrideEndSelectionBinding *,
    Nba97GameSelectionInput *);

#ifdef __cplusplus
}
#endif
#endif
