#pragma once
#include "recovered/game_match_state_reset.h"
#include <string>
#include <vector>
namespace nba97 { struct GameTeamHeaderInitializeCapture { std::string receipt; std::vector<std::string> calls; bool dispatch(const Nba97GameTextMemory*,const Nba97GameMatchStateResetEvent*,Nba97GameMatchStateResetMachine*); }; }
