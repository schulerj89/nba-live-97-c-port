#ifndef NBA97_GAME_CAMERA_SELECT_CAPTURE_H
#define NBA97_GAME_CAMERA_SELECT_CAPTURE_H
#include "game_camera_select_adapter.h"
#include <string>
namespace nba97 {
struct GameCameraSelectCapture {
    std::string receipt;
    int dispatch(const Nba97GameTextMemory*,const Nba97GameCameraStartupEvent*,Nba97GameCameraStartupRegisters*);
};
}
#endif
