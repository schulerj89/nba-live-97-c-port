#ifndef NBA97_GAME_PREGAME_MATCH_CARD_ADAPTER_H
#define NBA97_GAME_PREGAME_MATCH_CARD_ADAPTER_H

#include "recovered/game_pregame_match_card.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GamePregameMatchCardBinding {
  size_t operation_budget;
  Nba97GamePregameMatchCardIo io;
  void *user;
  Nba97GamePregameMatchCardAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GamePeriodPresentationFinishEvent event;
  Nba97GamePregameMatchCardProgress progress;
  int result;
} Nba97GamePregameMatchCardBinding;

int nba97_game_pregame_match_card_from_period_presentation_finish(
    void *, const Nba97GameTextMemory *,
    const Nba97GamePeriodPresentationFinishEvent *,
    Nba97GamePeriodPresentationFinishMachine *);

int nba97_game_period_presentation_finish_with_pregame_match_card(
    const Nba97GamePeriodPresentationFinishContext *,
    Nba97GamePregameMatchCardBinding *,
    Nba97GamePeriodPresentationFinishProgress *);

#ifdef __cplusplus
}
#endif
#endif
