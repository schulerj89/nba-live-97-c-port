#pragma once
#include "game_camera_elapsed_dispatch_adapter.h"
#include <string>
namespace nba97 {
struct GameCameraElapsedDispatchCapture {
  std::string receipt;
  int dispatch(const Nba97GameTextMemory*,const Nba97GameCameraSelectEvent*,Nba97GameCameraSelectRegisters*);
};
}
