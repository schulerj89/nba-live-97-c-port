#ifndef NBA97_GAME_GTE_TRANSLATION_INSTALL_ADAPTER_H
#define NBA97_GAME_GTE_TRANSLATION_INSTALL_ADAPTER_H

#include "recovered/game_camera_frame_transform.h"
#include "recovered/game_gte_translation_install.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameGteTranslationInstallCameraBinding {
  Nba97GameGteTranslationInstallWord control[32];
  size_t operation_budget;
  Nba97GameGteTranslationInstallAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameGteTranslationInstallControlWrite *control_journal;
  size_t control_journal_capacity;
  Nba97GameCameraFrameTransformIo fallback;
  void *fallback_user;
  Nba97GameGteTranslationInstallProgress progress;
  int result;
  size_t invocations;
} Nba97GameGteTranslationInstallCameraBinding;

void nba97_game_gte_translation_install_camera_binding_init(
    Nba97GameGteTranslationInstallCameraBinding *,
    const Nba97GameGteTranslationInstallWord *initial_control,
    size_t operation_budget, Nba97GameGteTranslationInstallAccess *, size_t,
    Nba97GameGteTranslationInstallControlWrite *, size_t,
    Nba97GameCameraFrameTransformIo fallback, void *fallback_user);

int nba97_game_gte_translation_install_from_camera(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraFrameTransformEvent *,
    Nba97GameCameraFrameTransformMachine *);

#ifdef __cplusplus
}
#endif
#endif
