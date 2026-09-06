#ifndef NBA97_GAME_PREGAME_SELECTION_SCREEN_ADAPTER_H
#define NBA97_GAME_PREGAME_SELECTION_SCREEN_ADAPTER_H

#include "recovered/game_period_presentation_finish.h"
#include "recovered/game_pregame_selection_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GamePregameSelectionScreenPresentationBinding {
  size_t operation_budget;
  Nba97GamePregameSelectionScreenIo io;
  void *user;
  Nba97GamePregameSelectionScreenAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GamePregameSelectionScreenProgress progress;
  Nba97GamePeriodPresentationFinishEvent event;
  int result;
  size_t invocations;
  size_t completions;
  size_t fallback_invocations;
  Nba97GamePeriodPresentationFinishIo fallback;
  void *fallback_user;
} Nba97GamePregameSelectionScreenPresentationBinding;

void nba97_game_pregame_selection_screen_presentation_binding_init(
    Nba97GamePregameSelectionScreenPresentationBinding *, size_t,
    Nba97GamePregameSelectionScreenIo, void *,
    Nba97GamePregameSelectionScreenAccess *, size_t,
    Nba97GamePeriodPresentationFinishIo, void *);

/* Compose only BZ's exact 0x8002DE14 child. Any assigned kind, entry, call PC,
 * delay PC, or return address is claimed before full metadata validation. */
int nba97_game_pregame_selection_screen_from_presentation_finish(
    void *, const Nba97GameTextMemory *,
    const Nba97GamePeriodPresentationFinishEvent *,
    Nba97GamePeriodPresentationFinishMachine *);

int nba97_game_pregame_selection_screen_from_presentation_finish(
    void *, const Nba97GameTextMemory *,
    const Nba97GamePeriodPresentationFinishEvent *,
    Nba97GamePeriodPresentationFinishMachine *);

#ifdef __cplusplus
}
#endif
#endif
