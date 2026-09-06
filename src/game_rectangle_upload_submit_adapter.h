#ifndef NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_ADAPTER_H
#define NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_ADAPTER_H

#include "recovered/game_rectangle_upload_submit.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameRectangleUploadSubmitBinding {
  size_t operation_budget;
  Nba97GameRectangleUploadSubmitIo io;
  void *user;
  Nba97GameRectangleUploadSubmitAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameImageRecordUploadEvent event;
  Nba97GameRectangleUploadSubmitProgress progress;
  int result;
} Nba97GameRectangleUploadSubmitBinding;

int nba97_game_rectangle_upload_submit_from_image_record_upload(
    void *, const Nba97GameTextMemory *,
    const Nba97GameImageRecordUploadEvent *,
    Nba97GameImageRecordUploadMachine *);

int nba97_game_image_record_upload_with_rectangle_upload_submit(
    const Nba97GameImageRecordUploadContext *,
    Nba97GameRectangleUploadSubmitBinding *,
    Nba97GameImageRecordUploadProgress *);

#ifdef __cplusplus
}
#endif
#endif
