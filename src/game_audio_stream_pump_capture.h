#ifndef NBA97_GAME_AUDIO_STREAM_PUMP_CAPTURE_H
#define NBA97_GAME_AUDIO_STREAM_PUMP_CAPTURE_H
#include "game_audio_stream_pump_adapter.h"
#include <string>
#include <vector>
namespace nba97 {
struct GameAudioStreamPumpCapture {
    std::vector<std::string> receipts;
    int fromSpeech(const Nba97GameTextMemory*,const Nba97GameSpeechStartupEvent*,Nba97GameSpeechStartupRegisters*);
    int fromController(const Nba97GameTextMemory*,const Nba97GameControllerFrameResetEvent*,Nba97GameControllerFrameResetRegisters*);
};
}
#endif
