#pragma once
#include "recovered/game_match_state_reset.h"
#include <string>
#include <vector>
namespace nba97 { struct GameMatchBufferInitializeCapture { std::string receipt; bool dispatch(const Nba97GameTextMemory*,const Nba97GameMatchStateResetEvent*,Nba97GameMatchStateResetMachine*); }; }
