#ifndef NBA97_GAME_SPEECH_INITIALIZE_CAPTURE_H
#define NBA97_GAME_SPEECH_INITIALIZE_CAPTURE_H
#include "game_speech_initialize_adapter.h"
#include <string>
namespace nba97 {
struct GameSpeechInitializeCapture {
    std::string receipt;
    bool dispatch(const Nba97GameTextMemory*,const Nba97GameMatchInitializeEvent*,Nba97GameMatchInitializeRegisters*);
};
}
#endif
