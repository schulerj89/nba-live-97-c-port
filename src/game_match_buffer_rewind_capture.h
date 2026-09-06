#pragma once
#include "recovered/game_match_buffer_initialize.h"
#include "recovered/game_match_state_reset.h"
#include <string>
namespace nba97 {
struct GameMatchBufferRewindCapture {
 std::string receipt;
 bool dispatchBuffer(const Nba97GameTextMemory*,const Nba97GameMatchBufferInitializeEvent*,Nba97GameMatchBufferInitializeMachine*);
 bool dispatchReset(const Nba97GameTextMemory*,const Nba97GameMatchStateResetEvent*,Nba97GameMatchStateResetMachine*);
};
}
