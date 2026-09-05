#ifndef NBA97_GAME_SCENE_RESOURCES_CAPTURE_H
#define NBA97_GAME_SCENE_RESOURCES_CAPTURE_H
#include "game_scene_resources_adapter.h"
#include <string>
namespace nba97 {
struct GameSceneResourcesCapture {
    std::string receipt;
    int dispatch(const Nba97GameTextMemory*,const Nba97GameSceneStartupEvent*,
                 Nba97GameSceneStartupRegisters*);
};
}
#endif
