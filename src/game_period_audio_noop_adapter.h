#ifndef NBA97_GAME_PERIOD_AUDIO_NOOP_ADAPTER_H
#define NBA97_GAME_PERIOD_AUDIO_NOOP_ADAPTER_H

#include "recovered/game_first_period_startup.h"
#include "recovered/game_period_audio_noop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GamePeriodAudioNoopBinding {
  Nba97GameFirstPeriodStartupIo fallback;
  void *fallback_user;
  Nba97GamePeriodAudioNoopProgress progress;
  Nba97GameFirstPeriodStartupEvent event;
  Nba97GameFirstPeriodStartupRegisters entry_registers;
  int result;
  size_t invocations;
  size_t completions;
} Nba97GamePeriodAudioNoopBinding;

/* Compose the no-op only for first-period startup's exact 0x80067434 call.
 * Any assigned identifier is claimed before exact validation; unrelated
 * children use fallback. The GPR-only parent supplies explicitly unknown
 * HI/LO, which remain untouched and are not copied back. */
int nba97_game_period_audio_noop_from_first_period_startup(
    void *, const Nba97GameTextMemory *,
    const Nba97GameFirstPeriodStartupEvent *,
    Nba97GameFirstPeriodStartupRegisters *);

#ifdef __cplusplus
}
#endif
#endif
