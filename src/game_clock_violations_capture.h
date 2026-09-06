#ifndef NBA97_GAME_CLOCK_VIOLATIONS_CAPTURE_H
#define NBA97_GAME_CLOCK_VIOLATIONS_CAPTURE_H
#include "game_clock_violations_adapter.h"
#include <string>
namespace nba97 {
struct GameClockViolationsCapture {
    std::string receipt;
    int dispatch(const Nba97GameTextMemory*,const Nba97MatchTickCall*,const Nba97GameMatchClocksProgress*);
};
}
#endif
