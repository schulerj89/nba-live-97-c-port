#ifndef NBA97_GAME_MATCH_AUDIO_SERVICE_ADAPTER_H
#define NBA97_GAME_MATCH_AUDIO_SERVICE_ADAPTER_H

#include "recovered/game_audio_stream_status.h"
#include "recovered/game_clock_read.h"
#include "recovered/game_match_audio_service.h"
#include "recovered/game_match_service_publish.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMatchAudioServiceCallerEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    uint8_t argument_count;
} Nba97GameMatchAudioServiceCallerEvent;

typedef struct Nba97GameMatchAudioServiceAdapterProgress {
    Nba97GameClockReadProgress clock_read;
    Nba97GameAudioStreamStatusProgress stream_status;
    Nba97GameMatchAudioServiceEvent clock_read_event;
    Nba97GameMatchAudioServiceEvent stream_status_event;
    Nba97GameMatchAudioServiceCallerEvent caller_event;
    int clock_read_result;
    int stream_status_result;
    int service_result;
    size_t clock_read_invocations;
    size_t clock_read_completions;
    size_t stream_status_invocations;
    size_t stream_status_completions;
    size_t unresolved_callbacks_completed;
    size_t caller_invocations;
    size_t caller_completions;
} Nba97GameMatchAudioServiceAdapterProgress;

/* Execute AB while binding its exact 0x8002A270 event to the existing AC
 * clock-read owner and 0x8002A2DC to the existing X status owner. All other
 * nine children remain the caller's full-machine services.
 * In particular, 0x80083EEC remains typed: V's current API exposes its GPRs
 * but cannot carry HI/LO mutations made by V's children, so composing it here
 * would either lose state or require explicitly unknown HI/LO outputs. */
int nba97_game_match_audio_service_with_stream_status(
    const Nba97GameMatchAudioServiceContext*,
    const Nba97GameClockReadContext*,
    const Nba97GameAudioStreamStatusContext*,
    Nba97GameMatchAudioServiceProgress*,
    Nba97GameMatchAudioServiceAdapterProgress*);

/* Bind the source-proven AA JAL metadata at 0x8002DE5C. JAL ra/delay state
 * must already be reflected in machine; the adapter validates ra=0x8002DE64
 * and the no-argument event. */
int nba97_game_match_audio_service_from_8002de5c(
    const Nba97GameTextMemory*,
    const Nba97GameMatchAudioServiceCallerEvent*,
    Nba97GameMatchAudioServiceMachine*,
    const Nba97GameMatchAudioServiceContext*,
    const Nba97GameClockReadContext*,
    const Nba97GameAudioStreamStatusContext*,
    Nba97GameMatchAudioServiceProgress*,
    Nba97GameMatchAudioServiceAdapterProgress*);

/* Typed bridge used by the now-recovered AA owner. It accepts only AA's
 * actual 0x8002DE5C child event and preserves the complete shared machine. */
int nba97_game_match_audio_service_from_match_service_publish(
    const Nba97GameTextMemory*,
    const Nba97GameMatchServicePublishEvent*,
    Nba97GameMatchServicePublishMachine*,
    const Nba97GameMatchAudioServiceContext*,
    const Nba97GameClockReadContext*,
    const Nba97GameAudioStreamStatusContext*,
    Nba97GameMatchAudioServiceProgress*,
    Nba97GameMatchAudioServiceAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
