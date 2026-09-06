#pragma once
#include "game_period_music_start_adapter.h"
#include <string>
namespace nba97 {
struct GamePeriodMusicStartCapture {
  std::string receipt;
  bool dispatch(const Nba97GameTextMemory *,
                const Nba97GameFirstPeriodStartupEvent *,
                Nba97GameFirstPeriodStartupRegisters *, unsigned initial_flag);
};
}
