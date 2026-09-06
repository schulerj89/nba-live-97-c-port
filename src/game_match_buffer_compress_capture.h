#pragma once
#include "game_match_buffer_compress_adapter.h"
#include <string>
namespace nba97 { struct GameMatchBufferCompressCapture {
 std::string receipt;
 bool dispatch(const Nba97GameTextMemory*,const Nba97GameMatchBufferRecordEvent*,Nba97GameMatchBufferRecordMachine*);
}; }
