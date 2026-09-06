#ifndef NBA97_GAME_CONTROLLER_FRAME_RESET_CAPTURE_H
#define NBA97_GAME_CONTROLLER_FRAME_RESET_CAPTURE_H
#include "game_controller_frame_reset_adapter.h"
#include "recovered/game_late_period_limits.h"
#include <string>
namespace nba97 {
struct GameControllerFrameResetCapture {
    std::string receipt;
    int dispatch(const Nba97GameTextMemory*,const Nba97MatchTickCall*,const Nba97GameLatePeriodLimitsProgress*);
};
}
#endif
