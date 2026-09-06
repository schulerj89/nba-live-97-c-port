#ifndef NBA97_GAME_SPEECH_STARTUP_ADAPTER_H
#define NBA97_GAME_SPEECH_STARTUP_ADAPTER_H

#include "game_random_seed_adapter.h"
#include "recovered/game_scene_random_warmup.h"
#include "recovered/game_speech_startup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameSpeechStartupAdapterProgress {
    Nba97GameSpeechStartupProgress speech;
    Nba97GameRandomSeedProgress seed;
    int speech_result;
    int seed_result;
    size_t speech_invocations;
    size_t seed_invocations;
    size_t unresolved_callbacks_completed;
    Nba97GameSceneRandomWarmupEvent speech_event;
    Nba97GameSceneRandomWarmupEvent seed_event;
} Nba97GameSpeechStartupAdapterProgress;

/* Compose one source-proven 0x800802B4 warm-up event with this recovered
 * owner. The callback/register prefix is returned even when the child stops. */
int nba97_game_speech_startup_from_warmup(const Nba97GameTextMemory*,
    const Nba97GameSceneRandomWarmupEvent*,
    Nba97GameSceneRandomWarmupRegisters*,
    const Nba97GameSpeechStartupContext*,
    Nba97GameSpeechStartupAdapterProgress*);

/* Execute the natural full-GPR 0x800802AC caller. Its first 0x800802B4 event
 * uses the speech owner and its existing 0x80093694 seed event uses the seed
 * owner; random and step events remain explicit through the warm-up callback. */
int nba97_game_scene_random_warmup_with_speech_startup(
    const Nba97GameSceneRandomWarmupContext*,
    const Nba97GameSpeechStartupContext*,
    const Nba97GameRandomSeedContext*,
    Nba97GameSceneRandomWarmupProgress*,
    Nba97GameSpeechStartupAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
