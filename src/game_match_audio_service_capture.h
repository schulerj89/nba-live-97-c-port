#ifndef NBA97_GAME_MATCH_AUDIO_SERVICE_CAPTURE_H
#define NBA97_GAME_MATCH_AUDIO_SERVICE_CAPTURE_H
#include "game_match_audio_service_adapter.h"
#include <string>
namespace nba97 {
struct GameMatchAudioServiceCapture {
    std::string receipt;
    int dispatch(const Nba97GameTextMemory*,const Nba97GameMatchServicePublishEvent*,
        Nba97GameMatchServicePublishMachine*,unsigned mode);
};
}
#endif
