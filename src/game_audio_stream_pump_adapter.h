#ifndef NBA97_GAME_AUDIO_STREAM_PUMP_ADAPTER_H
#define NBA97_GAME_AUDIO_STREAM_PUMP_ADAPTER_H

#include "recovered/game_audio_stream_pump.h"
#include "recovered/game_speech_startup.h"
#include "recovered/game_controller_frame_reset.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameAudioStreamPumpAdapterProgress {
    Nba97GameAudioStreamPumpProgress pump;
    int pump_result;
    size_t pump_invocations;
    size_t pump_completions;
    size_t unresolved_callbacks_completed;
    Nba97GameSpeechStartupEvent pump_event[2]; /* 0x801E4, 0x8021C. */
} Nba97GameAudioStreamPumpAdapterProgress;

/* Compose either source-proven speech-startup call with this owner. The live
 * full-GPR prefix is returned even when the nested owner stops. */
int nba97_game_audio_stream_pump_from_speech_startup(
    const Nba97GameTextMemory*, const Nba97GameSpeechStartupEvent*,
    Nba97GameSpeechStartupRegisters*,
    const Nba97GameAudioStreamPumpContext*,
    Nba97GameAudioStreamPumpAdapterProgress*);

/* Compose the controller-reset owner's complete 0x8006764C JAL event. */
int nba97_game_audio_stream_pump_from_controller_reset(
    const Nba97GameTextMemory*, const Nba97GameControllerFrameResetEvent*,
    Nba97GameControllerFrameResetRegisters*, const Nba97GameAudioStreamPumpContext*,
    Nba97GameAudioStreamPumpProgress*);

/* Execute the recovered natural caller. Its 0x800801E4 and repeated
 * 0x8008021C events use the stream-pump owner; all other children remain the
 * caller's explicit typed services. */
int nba97_game_speech_startup_with_audio_stream_pump(
    const Nba97GameSpeechStartupContext*,
    const Nba97GameAudioStreamPumpContext*,
    Nba97GameSpeechStartupProgress*,
    Nba97GameAudioStreamPumpAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
