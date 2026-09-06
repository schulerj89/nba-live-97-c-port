#ifndef NBA97_GAME_RECTANGLE_NORMALIZE_ADAPTER_H
#define NBA97_GAME_RECTANGLE_NORMALIZE_ADAPTER_H

#include "recovered/game_rectangle_normalize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameRectangleNormalizeBinding {
  size_t operation_budget;
  Nba97GameRectangleNormalizeAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameRectangleUploadSubmitEvent event;
  Nba97GameRectangleNormalizeProgress progress;
  int result;
} Nba97GameRectangleNormalizeBinding;

int nba97_game_rectangle_normalize_from_rectangle_upload_submit(
    void *, const Nba97GameTextMemory *,
    const Nba97GameRectangleUploadSubmitEvent *,
    Nba97GameRectangleUploadSubmitMachine *);

int nba97_game_rectangle_upload_submit_with_rectangle_normalize(
    const Nba97GameRectangleUploadSubmitContext *,
    Nba97GameRectangleNormalizeBinding *,
    Nba97GameRectangleUploadSubmitProgress *);

#ifdef __cplusplus
}
#endif
#endif
