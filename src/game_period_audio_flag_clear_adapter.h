#ifndef NBA97_GAME_PERIOD_AUDIO_FLAG_CLEAR_ADAPTER_H
#define NBA97_GAME_PERIOD_AUDIO_FLAG_CLEAR_ADAPTER_H

#include "recovered/game_first_period_startup.h"
#include "recovered/game_period_audio_flag_clear.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GamePeriodAudioFlagClearBinding {
  size_t operation_budget;
  Nba97GamePeriodAudioFlagClearAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameFirstPeriodStartupIo fallback;
  void *fallback_user;
  size_t invocations;
  size_t completions;
  Nba97GameFirstPeriodStartupEvent event;
  Nba97GamePeriodAudioFlagClearProgress progress;
  int result;
} Nba97GamePeriodAudioFlagClearBinding;

void nba97_game_period_audio_flag_clear_binding_init(
    Nba97GamePeriodAudioFlagClearBinding *, size_t,
    Nba97GamePeriodAudioFlagClearAccess *, size_t,
    Nba97GameFirstPeriodStartupIo, void *);

int nba97_game_period_audio_flag_clear_from_first_period(
    void *, const Nba97GameTextMemory *,
    const Nba97GameFirstPeriodStartupEvent *,
    Nba97GameFirstPeriodStartupRegisters *);

int nba97_game_first_period_startup_with_audio_flag_clear(
    const Nba97GameFirstPeriodStartupContext *,
    Nba97GamePeriodAudioFlagClearBinding *,
    Nba97GameFirstPeriodStartupProgress *);

#ifdef __cplusplus
}
#endif
#endif
