#ifndef NBA97_GAME_TEXT_SUBMISSION_ADAPTER_H
#define NBA97_GAME_TEXT_SUBMISSION_ADAPTER_H

#include "recovered/game_clear_ordering_table.h"
#include "recovered/game_text_submission.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameTextSubmissionBinding {
  size_t operation_budget;
  Nba97GameTextSubmissionIo io;
  void *user;
  Nba97GameTextSubmissionAccess *access_journal;
  size_t access_journal_capacity;
  size_t clear_operation_budget;
  Nba97GameClearOrderingTableIo clear_io;
  void *clear_user;
  Nba97GameClearOrderingTableAccess *clear_access_journal;
  size_t clear_access_journal_capacity;
  size_t invocations;
  size_t completions;
  size_t clear_invocations;
  size_t clear_completions;
  Nba97GameCountdownUiUpdateEvent event;
  Nba97GameTextSubmissionProgress progress;
  Nba97GameClearOrderingTableProgress clear_progress[2];
  int result;
  int clear_result[2];
} Nba97GameTextSubmissionBinding;

int nba97_game_text_submission_from_countdown_ui_update(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCountdownUiUpdateEvent *,
    Nba97GameCountdownUiUpdateMachine *);

int nba97_game_countdown_ui_update_with_text_submission(
    const Nba97GameCountdownUiUpdateContext *, Nba97GameTextSubmissionBinding *,
    Nba97GameCountdownUiUpdateProgress *);

#ifdef __cplusplus
}
#endif
#endif
