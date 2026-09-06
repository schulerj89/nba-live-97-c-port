#ifndef NBA97_GAME_MATCH_CLOCKS_CAPTURE_H
#define NBA97_GAME_MATCH_CLOCKS_CAPTURE_H
#include "game_match_clocks_adapter.h"
#include <string>
namespace nba97 {
struct GameMatchClocksCapture {
    std::string receipt;
    Nba97GameMatchClocksProgress progress{};
    int dispatch(const Nba97GameTextMemory*,const Nba97MatchTickCall*,unsigned phase);
};
}
#endif
