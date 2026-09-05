#ifndef NBA97_GAME_AUDIO_INITIALIZE_CAPTURE_H
#define NBA97_GAME_AUDIO_INITIALIZE_CAPTURE_H
#include "recovered/game_audio_initialize.h"
#include <string>
namespace nba97 {
struct GameAudioInitializeCapture {
    std::string receipt;
    bool dispatch(const Nba97GameTextMemory*,const Nba97GameMatchInitializeEvent*,
                  Nba97GameMatchInitializeRegisters*);
};
}
#endif
