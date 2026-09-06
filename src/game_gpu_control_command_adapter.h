#ifndef NBA97_GAME_GPU_CONTROL_COMMAND_ADAPTER_H
#define NBA97_GAME_GPU_CONTROL_COMMAND_ADAPTER_H

#include "recovered/game_gpu_control_command.h"
#include "recovered/game_display_mask_set.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameGpuControlCommandDisplayIndex {
  NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_99D6C = 0,
  NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_99F78,
  NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_99FA4,
  NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_9A114,
  NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_COUNT
};

typedef struct Nba97GameGpuControlCommandBinding {
  size_t operation_budget;
  Nba97GameGpuControlCommandAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  size_t fallback_callbacks_completed;
  size_t call_count[NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_COUNT];
  Nba97GameDisplayEnvironmentEvent
      event[NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_COUNT];
  Nba97GameGpuControlCommandProgress
      progress[NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_COUNT];
  int result[NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_COUNT];
} Nba97GameGpuControlCommandBinding;

int nba97_game_gpu_control_command_from_display_environment(
    void *, const Nba97GameTextMemory *,
    const Nba97GameDisplayEnvironmentEvent *,
    Nba97GameDisplayEnvironmentMachine *);

int nba97_game_display_environment_with_gpu_control_command(
    const Nba97GameDisplayEnvironmentContext *,
    Nba97GameGpuControlCommandBinding *, Nba97GameDisplayEnvironmentProgress *);

/* The older SetDispMask boundary carries only A0, SP, S0/S1 and RA.
 * Source 0x800994D4..0x800994F3 consumes the leaf V0 and saved frame after it;
 * the leaf preserves SP/S0/S1 and never reads the omitted CPU/HI/LO words.
 * Those omitted words remain explicitly unknown in the child receipt. */
int nba97_game_gpu_control_command_from_display_mask(
    const Nba97GameTextMemory *, const Nba97GameDisplayMaskSetEvent *,
    size_t operation_budget, Nba97GameGpuControlCommandProgress *,
    Nba97GameDisplayMaskSetValue *);

#ifdef __cplusplus
}
#endif

#endif
