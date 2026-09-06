#ifndef NBA97_GAME_GRAPHICS_SUBMIT_ADAPTER_H
#define NBA97_GAME_GRAPHICS_SUBMIT_ADAPTER_H
#include "recovered/game_graphics_submit.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97GameGraphicsSubmitBinding {
  size_t operation_budget;
  Nba97GameGraphicsSubmitIo io;
  void *user;
  Nba97GameGraphicsSubmitAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations, completions, fallback_callbacks_completed;
  Nba97GameDrawEnvironmentEvent event;
  Nba97GameGraphicsSubmitProgress progress;
  int result;
} Nba97GameGraphicsSubmitBinding;
int nba97_game_graphics_submit_from_draw_environment(
    void *, const Nba97GameTextMemory *, const Nba97GameDrawEnvironmentEvent *,
    Nba97GameDrawEnvironmentMachine *);
int nba97_game_draw_environment_with_graphics_submit(
    const Nba97GameDrawEnvironmentContext *, Nba97GameGraphicsSubmitBinding *,
    Nba97GameDrawEnvironmentProgress *);
#ifdef __cplusplus
}
#endif
#endif
