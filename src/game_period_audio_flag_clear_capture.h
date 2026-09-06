#pragma once
#include "game_period_audio_flag_clear_adapter.h"
#include <string>
namespace nba97 {
struct GamePeriodAudioFlagClearCapture {
  std::string receipt;
  bool dispatch(const Nba97GameTextMemory *,
                const Nba97GameFirstPeriodStartupEvent *,
                Nba97GameFirstPeriodStartupRegisters *);
};
}
