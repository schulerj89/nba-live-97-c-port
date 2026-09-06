#ifndef NBA97_GAME_DRAW_ENVIRONMENT_ADAPTER_H
#define NBA97_GAME_DRAW_ENVIRONMENT_ADAPTER_H

#include "recovered/game_draw_environment.h"
#include "recovered/game_scene_startup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Nba97GameDrawEnvironmentHiLoProvider)(
    void *, const Nba97GameSceneStartupEvent *, Nba97GameDrawEnvironmentWord *,
    Nba97GameDrawEnvironmentWord *);

typedef struct Nba97GameDrawEnvironmentSceneBinding {
  size_t operation_budget;
  Nba97GameDrawEnvironmentIo io;
  void *user;
  Nba97GameDrawEnvironmentHiLoProvider hi_lo;
  void *hi_lo_user;
  Nba97GameDrawEnvironmentAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameSceneStartupIo fallback;
  void *fallback_user;
  Nba97GameDrawEnvironmentProgress progress;
  int result;
  size_t invocations;
  size_t fallback_invocations;
} Nba97GameDrawEnvironmentSceneBinding;

void nba97_game_draw_environment_scene_binding_init(
    Nba97GameDrawEnvironmentSceneBinding *, size_t operation_budget,
    Nba97GameDrawEnvironmentIo, void *user,
    Nba97GameDrawEnvironmentHiLoProvider, void *hi_lo_user,
    Nba97GameDrawEnvironmentAccess *, size_t access_journal_capacity,
    Nba97GameSceneStartupIo fallback, void *fallback_user);

int nba97_game_draw_environment_from_scene(void *, const Nba97GameTextMemory *,
                                           const Nba97GameSceneStartupEvent *,
                                           Nba97GameSceneStartupRegisters *);

#ifdef __cplusplus
}
#endif
#endif
