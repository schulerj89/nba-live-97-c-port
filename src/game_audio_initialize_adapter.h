#ifndef NBA97_GAME_AUDIO_INITIALIZE_ADAPTER_H
#define NBA97_GAME_AUDIO_INITIALIZE_ADAPTER_H

#include "recovered/game_audio_initialize.h"
#include "recovered/game_resource_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameAudioInitializeDependencies {
    size_t resource_loader_operation_budget;
    Nba97GameResourceLoaderIo resource_loader_io;
    void* resource_loader_user;
} Nba97GameAudioInitializeDependencies;

typedef struct Nba97GameAudioInitializeAdapterProgress {
    Nba97GameResourceLoaderProgress resource_loader[2];
    int resource_loader_result[2];
    size_t resource_loader_invocations;
    size_t unresolved_callbacks_completed;
} Nba97GameAudioInitializeAdapterProgress;

/* Route compatible 0x80029BFC calls to its complete recovered owner. Calls
 * whose input knownness cannot fit that narrower API, and every other child,
 * retain context->io/user. Unexposed child scratch GPRs become unknown rather
 * than receiving invented values. The existing 0x80090698 API excludes active
 * stack aliases and does not expose stack/output GPRs, so releases deliberately
 * remain at the typed callback boundary. */
int nba97_game_audio_initialize_with_recovered_dependencies(
    const Nba97GameAudioInitializeContext*,
    const Nba97GameAudioInitializeDependencies*,
    Nba97GameAudioInitializeProgress*,
    Nba97GameAudioInitializeAdapterProgress*);

/* Validate the natural 0x8002DBD0 parent call and copy its already-identical
 * full register representation for direct owner composition. */
int nba97_game_audio_initialize_registers_from_match_initialize(
    const Nba97GameMatchInitializeEvent*,
    const Nba97GameMatchInitializeRegisters*,
    Nba97GameAudioInitializeRegisters*);

#ifdef __cplusplus
}
#endif
#endif
