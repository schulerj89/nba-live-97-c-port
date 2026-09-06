#ifndef NBA97_GAME_SPEECH_STARTUP_CAPTURE_H
#define NBA97_GAME_SPEECH_STARTUP_CAPTURE_H
#include "game_speech_startup_adapter.h"
#include <string>
namespace nba97 {
struct GameSpeechStartupCapture {
    std::string receipt;
    int dispatch(const Nba97GameTextMemory*,const Nba97GameSceneRandomWarmupEvent*,
        Nba97GameSceneRandomWarmupRegisters*);
};
}
#endif
