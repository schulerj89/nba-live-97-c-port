#ifndef NBA97_GAME_VIDEO_MODE_ADAPTER_H
#define NBA97_GAME_VIDEO_MODE_ADAPTER_H

#include "recovered/game_display_environment.h"
#include "recovered/game_video_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameVideoModeDisplaySite {
  NBA97_GAME_VIDEO_MODE_DISPLAY_RECTANGLE = 0,
  NBA97_GAME_VIDEO_MODE_DISPLAY_MODE,
  NBA97_GAME_VIDEO_MODE_DISPLAY_SITE_COUNT
};

typedef struct Nba97GameVideoModeCallConfig {
  size_t operation_budget;
  Nba97GameVideoModeAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameVideoModeCallConfig;

typedef struct Nba97GameVideoModeDisplayBinding {
  Nba97GameVideoModeCallConfig config[NBA97_GAME_VIDEO_MODE_DISPLAY_SITE_COUNT];
  Nba97GameDisplayEnvironmentIo fallback;
  void *fallback_user;
  size_t invocations;
  size_t completions;
  size_t call_count[NBA97_GAME_VIDEO_MODE_DISPLAY_SITE_COUNT];
  Nba97GameDisplayEnvironmentEvent
      event[NBA97_GAME_VIDEO_MODE_DISPLAY_SITE_COUNT];
  Nba97GameVideoModeProgress progress[NBA97_GAME_VIDEO_MODE_DISPLAY_SITE_COUNT];
  int result[NBA97_GAME_VIDEO_MODE_DISPLAY_SITE_COUNT];
} Nba97GameVideoModeDisplayBinding;

void nba97_game_video_mode_display_binding_init(
    Nba97GameVideoModeDisplayBinding *, const Nba97GameVideoModeCallConfig *,
    Nba97GameDisplayEnvironmentIo fallback, void *fallback_user);

int nba97_game_video_mode_from_display(void *, const Nba97GameTextMemory *,
                                       const Nba97GameDisplayEnvironmentEvent *,
                                       Nba97GameDisplayEnvironmentMachine *);

#ifdef __cplusplus
}
#endif
#endif
