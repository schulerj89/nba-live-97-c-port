#pragma once
#include "game_pregame_selection_screen_adapter.h"
#include <string>
namespace nba97 {
struct GamePregameSelectionScreenCapture {
  std::string receipt = "null";
  bool dispatch(const Nba97GameTextMemory *,
                const Nba97GamePeriodPresentationFinishEvent *,
                Nba97GamePeriodPresentationFinishMachine *);
};
}
