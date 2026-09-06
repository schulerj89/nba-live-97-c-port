#pragma once
#include "recovered/game_match_initialize.h"
#include <string>
namespace nba97 { struct GameMatchStateResetCapture { std::string receipt; bool dispatch(const Nba97GameTextMemory*,const Nba97GameMatchInitializeEvent*,Nba97GameMatchInitializeRegisters*); }; }
