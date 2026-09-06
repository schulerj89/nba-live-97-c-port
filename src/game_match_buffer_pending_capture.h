#pragma once
#include "game_match_buffer_pending_adapter.h"
#include <string>
#include <vector>
namespace nba97 {
struct GameMatchBufferPendingCapture {
 std::vector<std::string> calls;
 bool dispatch(const Nba97GameTextMemory*,const Nba97GamePeriodStartupEvent*,Nba97GamePeriodStartupRegisters*);
 std::string receipt() const;
};
}
