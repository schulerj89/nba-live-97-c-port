#ifndef NBA97_GAME_PERIOD_PRESENTATION_FINISH_ADAPTER_H
#define NBA97_GAME_PERIOD_PRESENTATION_FINISH_ADAPTER_H

#include "recovered/game_first_period_startup.h"
#include "recovered/game_period_presentation_finish.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GamePeriodPresentationFinishBinding {
  size_t operation_budget;
  Nba97GamePeriodPresentationFinishIo io;
  void *user;
  Nba97GameFirstPeriodStartupIo fallback;
  void *fallback_user;
  Nba97GamePeriodPresentationFinishAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GamePeriodPresentationFinishProgress progress;
  Nba97GameFirstPeriodStartupEvent event;
  int result;
  size_t invocations;
  size_t completions;
} Nba97GamePeriodPresentationFinishBinding;

/* Compose 0x8002DDCC only at first-period startup's exact 0x80067424 call.
 * Any assigned identifier is claimed before exact validation; unrelated
 * children use fallback. The GPR-only parent leaves HI/LO explicitly unknown,
 * and only a valid GPR prefix is copied back after owner execution. */
int nba97_game_period_presentation_finish_from_first_period_startup(
    void *, const Nba97GameTextMemory *,
    const Nba97GameFirstPeriodStartupEvent *,
    Nba97GameFirstPeriodStartupRegisters *);

#ifdef __cplusplus
}
#endif
#endif
