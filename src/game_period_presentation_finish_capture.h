#pragma once
#include "game_period_presentation_finish_adapter.h"
#include <string>
namespace nba97 {
struct GamePeriodPresentationFinishCapture {
  std::string receipt = "null";
  bool dispatch(const Nba97GameTextMemory *,
                const Nba97GameFirstPeriodStartupEvent *,
                Nba97GameFirstPeriodStartupRegisters *);
};
}
