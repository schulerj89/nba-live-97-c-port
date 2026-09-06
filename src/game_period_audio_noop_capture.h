#pragma once
#include "game_period_audio_noop_adapter.h"
#include <string>
namespace nba97 {
struct GamePeriodAudioNoopCapture {
  std::string receipt;
  bool dispatch(const Nba97GameTextMemory *,
                const Nba97GameFirstPeriodStartupEvent *,
                Nba97GameFirstPeriodStartupRegisters *);
};
}
