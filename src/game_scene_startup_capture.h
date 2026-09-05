#ifndef NBA97_GAME_SCENE_STARTUP_CAPTURE_H
#define NBA97_GAME_SCENE_STARTUP_CAPTURE_H
#include "game_scene_startup_adapter.h"
#include <string>
namespace nba97 {
struct GameSceneStartupCapture {
    std::string receipt;
    int dispatch(const Nba97GameTextMemory*,const Nba97GameSceneLoadEvent*,
                 Nba97GameSceneLoadRegisters*);
};
}
#endif
