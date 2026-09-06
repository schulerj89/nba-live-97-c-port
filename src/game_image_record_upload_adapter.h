#ifndef NBA97_GAME_IMAGE_RECORD_UPLOAD_ADAPTER_H
#define NBA97_GAME_IMAGE_RECORD_UPLOAD_ADAPTER_H

#include "recovered/game_image_record_upload.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameImageRecordUploadBinding {
  size_t operation_budget;
  Nba97GameImageRecordUploadIo io;
  void *user;
  Nba97GameImageRecordUploadAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameCountdownUiUpdateEvent event;
  Nba97GameImageRecordUploadProgress progress;
  int result;
} Nba97GameImageRecordUploadBinding;

int nba97_game_image_record_upload_from_countdown_ui_update(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCountdownUiUpdateEvent *,
    Nba97GameCountdownUiUpdateMachine *);

int nba97_game_countdown_ui_update_with_image_record_upload(
    const Nba97GameCountdownUiUpdateContext *,
    Nba97GameImageRecordUploadBinding *, Nba97GameCountdownUiUpdateProgress *);

#ifdef __cplusplus
}
#endif
#endif
