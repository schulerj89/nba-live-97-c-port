#ifndef NBA97_GAME_CAMERA_STARTUP_CAPTURE_H
#define NBA97_GAME_CAMERA_STARTUP_CAPTURE_H
#include "game_camera_startup_adapter.h"
#include "recovered/game_match_hot_start.h"
#include <string>
namespace nba97 {
struct GameCameraStartupCapture {
    std::string receipt;
    int dispatch(const Nba97GameTextMemory*,const Nba97MatchTickCall*,
        const Nba97GameMatchHotStartProgress*);
};
}
#endif
