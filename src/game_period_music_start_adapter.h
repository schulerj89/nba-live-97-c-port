#ifndef NBA97_GAME_PERIOD_MUSIC_START_ADAPTER_H
#define NBA97_GAME_PERIOD_MUSIC_START_ADAPTER_H

#include "recovered/game_first_period_startup.h"
#include "recovered/game_period_music_start.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GamePeriodMusicStartFirstPeriodBinding {
  size_t operation_budget;
  Nba97GamePeriodMusicStartIo io;
  void *user;
  Nba97GamePeriodMusicStartAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GamePeriodMusicStartProgress progress;
  Nba97GameFirstPeriodStartupEvent event;
  int result;
  size_t invocations;
  size_t completions;
  size_t fallback_invocations;
  Nba97GameFirstPeriodStartupIo fallback;
  void *fallback_user;
} Nba97GamePeriodMusicStartFirstPeriodBinding;

void nba97_game_period_music_start_first_period_binding_init(
    Nba97GamePeriodMusicStartFirstPeriodBinding *, size_t,
    Nba97GamePeriodMusicStartIo, void *, Nba97GamePeriodMusicStartAccess *,
    size_t, Nba97GameFirstPeriodStartupIo, void *);

/* Compose only first-period startup's 0x800673F8 call. Its legacy parent
 * transports GPRs but no HI/LO, so the bridge passes explicit unknown words
 * and never invents the absent machine state. */
int nba97_game_period_music_start_from_first_period(
    void *, const Nba97GameTextMemory *,
    const Nba97GameFirstPeriodStartupEvent *,
    Nba97GameFirstPeriodStartupRegisters *);

#ifdef __cplusplus
}
#endif
#endif
