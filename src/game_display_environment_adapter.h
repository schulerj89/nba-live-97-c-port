#ifndef NBA97_GAME_DISPLAY_ENVIRONMENT_ADAPTER_H
#define NBA97_GAME_DISPLAY_ENVIRONMENT_ADAPTER_H
#include "recovered/game_display_environment.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97GameDisplayEnvironmentSceneIndex {
  NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_48F20 = 0,
  NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_48F78,
  NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_COUNT
};
typedef int (*Nba97GameDisplayEnvironmentHiLoProvider)(
    void *, const Nba97GameSceneStartupEvent *, size_t,
    Nba97GameDisplayEnvironmentWord *, Nba97GameDisplayEnvironmentWord *);
typedef struct Nba97GameDisplayEnvironmentSceneBinding {
  size_t operation_budget;
  Nba97GameDisplayEnvironmentHiLoProvider hi_lo_provider;
  void *hi_lo_user;
  Nba97GameDisplayEnvironmentIo io;
  void *user;
  Nba97GameDisplayEnvironmentAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations, completions, fallback_callbacks_completed,
      call_count[NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_COUNT];
  Nba97GameSceneStartupEvent event[NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_COUNT];
  Nba97GameDisplayEnvironmentProgress
      progress[NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_COUNT];
  int result[NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_COUNT];
  Nba97GameDisplayEnvironmentWord
      final_hi[NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_COUNT],
      final_lo[NBA97_GAME_DISPLAY_ENVIRONMENT_SCENE_COUNT];
} Nba97GameDisplayEnvironmentSceneBinding;
int nba97_game_display_environment_from_scene_startup(
    void *, const Nba97GameTextMemory *, const Nba97GameSceneStartupEvent *,
    Nba97GameSceneStartupRegisters *);
int nba97_game_scene_startup_with_display_environment(
    const Nba97GameSceneStartupContext *,
    Nba97GameDisplayEnvironmentSceneBinding *, Nba97GameSceneStartupProgress *);
#ifdef __cplusplus
}
#endif
#endif
