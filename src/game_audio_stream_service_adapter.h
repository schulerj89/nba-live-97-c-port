#ifndef NBA97_GAME_AUDIO_STREAM_SERVICE_ADAPTER_H
#define NBA97_GAME_AUDIO_STREAM_SERVICE_ADAPTER_H

#include "game_audio_stream_status_adapter.h"
#include "recovered/game_audio_stream_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameAudioStreamServiceAdapterProgress {
    Nba97GameAudioStreamServiceProgress service;
    int service_result;
    size_t service_invocations;
    size_t service_completions;
    size_t unresolved_callbacks_completed;
    Nba97GameAudioStreamPumpEvent service_event;
} Nba97GameAudioStreamServiceAdapterProgress;

/* Bind either natural V call site, 0x80083F78 or 0x80084034, to the complete
 * Y owner. A started nested prefix replaces the caller's live full GPRs. */
int nba97_game_audio_stream_service_from_stream_pump(
    const Nba97GameTextMemory*, const Nba97GameAudioStreamPumpEvent*,
    Nba97GameAudioStreamPumpRegisters*,
    const Nba97GameAudioStreamServiceContext*,
    Nba97GameAudioStreamServiceAdapterProgress*);

/* Execute V with the already recovered X status leaf and this Y service
 * wrapper bound at their real call sites. Every other V child stays on the
 * pump context's explicit typed callback. */
int nba97_game_audio_stream_pump_with_stream_status_and_service(
    const Nba97GameAudioStreamPumpContext*,
    const Nba97GameAudioStreamStatusContext*,
    const Nba97GameAudioStreamServiceContext*,
    Nba97GameAudioStreamPumpProgress*,
    Nba97GameAudioStreamStatusAdapterProgress*,
    Nba97GameAudioStreamServiceAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
