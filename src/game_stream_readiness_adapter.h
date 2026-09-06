#ifndef NBA97_GAME_STREAM_READINESS_ADAPTER_H
#define NBA97_GAME_STREAM_READINESS_ADAPTER_H

#include "game_match_audio_service_adapter.h"
#include "recovered/game_stream_readiness.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameStreamReadinessAdapterProgress {
    Nba97GameMatchAudioServiceAdapterProgress audio_service;
    Nba97GameStreamReadinessProgress readiness;
    Nba97GameMatchAudioServiceEvent readiness_event;
    int readiness_result;
    int audio_service_result;
    size_t readiness_invocations;
    size_t readiness_completions;
    size_t unresolved_callbacks_completed;
} Nba97GameStreamReadinessAdapterProgress;

/* Bind only AB's exact no-argument 0x8002A2EC -> 0x80088D0C JAL. The parent
 * machine already contains JAL ra=0x8002A2F4 and the completed NOP delay. */
int nba97_game_stream_readiness_from_match_audio_service(
    const Nba97GameTextMemory*, const Nba97GameMatchAudioServiceEvent*,
    Nba97GameMatchAudioServiceMachine*,
    const Nba97GameStreamReadinessContext*,
    Nba97GameStreamReadinessProgress*);

/* Execute the actual recovered AB owner with AC clock read, X stream status,
 * and AD readiness composed at their exact natural events. AB's remaining
 * children and AD's sole child retain the supplied typed full-machine I/O. */
int nba97_game_match_audio_service_with_stream_readiness(
    const Nba97GameMatchAudioServiceContext*,
    const Nba97GameClockReadContext*,
    const Nba97GameAudioStreamStatusContext*,
    const Nba97GameStreamReadinessContext*,
    Nba97GameMatchAudioServiceProgress*,
    Nba97GameStreamReadinessAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
