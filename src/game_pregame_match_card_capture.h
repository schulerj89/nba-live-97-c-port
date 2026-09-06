#pragma once
#include "game_pregame_match_card_adapter.h"
#include <string>
namespace nba97 {
struct GamePregameMatchCardCapture {
  std::string receipt = "null";
  bool dispatch(const Nba97GameTextMemory *,
                const Nba97GamePeriodPresentationFinishEvent *,
                Nba97GamePeriodPresentationFinishMachine *);
};
}
