#ifndef NBA97_GAME_AUDIO_STREAM_STATUS_ADAPTER_H
#define NBA97_GAME_AUDIO_STREAM_STATUS_ADAPTER_H

#include "recovered/game_audio_stream_pump.h"
#include "recovered/game_audio_stream_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameAudioStreamStatusAdapterProgress {
    Nba97GameAudioStreamStatusProgress status;
    int status_result;
    size_t status_invocations;
    size_t status_completions;
    size_t unresolved_callbacks_completed;
    Nba97GameAudioStreamPumpEvent status_event;
} Nba97GameAudioStreamStatusAdapterProgress;

/* Compose the natural stream-pump 0x80083F00 -> 0x8008472C event while
 * retaining the exact full-GPR prefix when the nested leaf stops. */
int nba97_game_audio_stream_status_from_stream_pump(
    const Nba97GameTextMemory*, const Nba97GameAudioStreamPumpEvent*,
    Nba97GameAudioStreamPumpRegisters*,
    const Nba97GameAudioStreamStatusContext*,
    Nba97GameAudioStreamStatusAdapterProgress*);

/* Execute the actual recovered V owner with X bound at its natural event.
 * V's other four services remain its caller-supplied typed fixtures. */
int nba97_game_audio_stream_pump_with_stream_status(
    const Nba97GameAudioStreamPumpContext*,
    const Nba97GameAudioStreamStatusContext*,
    Nba97GameAudioStreamPumpProgress*,
    Nba97GameAudioStreamStatusAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
