#ifndef NBA97_GAME_PERIOD_EXPIRY_CAPTURE_H
#define NBA97_GAME_PERIOD_EXPIRY_CAPTURE_H
#include "game_period_expiry_adapter.h"
#include "game_clock_violations_adapter.h"
#include <string>
namespace nba97 {
struct GamePeriodExpiryCapture {
    std::string receipt;
    Nba97GamePeriodExpiryProgress progress{};
    int dispatch(const Nba97GameTextMemory*,const Nba97MatchTickCall*,Nba97GamePeriodValue*,const Nba97GameClockViolationsProgress*);
};
}
#endif
