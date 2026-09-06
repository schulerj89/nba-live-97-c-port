#ifndef NBA97_GAME_SPEECH_INITIALIZE_ADAPTER_H
#define NBA97_GAME_SPEECH_INITIALIZE_ADAPTER_H

#include "recovered/game_resource_loader.h"
#include "recovered/game_speech_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameSpeechInitializeDependencies {
    size_t resource_loader_operation_budget;
    Nba97GameResourceLoaderIo resource_loader_io;
    void* resource_loader_user;
} Nba97GameSpeechInitializeDependencies;

typedef struct Nba97GameSpeechInitializeAdapterProgress {
    size_t resource_loader_invocations;
    size_t unresolved_callbacks_completed;
    int resource_loader_result[3];
    Nba97GameResourceLoaderProgress resource_loader[3];
} Nba97GameSpeechInitializeAdapterProgress;

int nba97_game_speech_initialize_with_recovered_dependencies(
    const Nba97GameSpeechInitializeContext*,
    const Nba97GameSpeechInitializeDependencies*,
    Nba97GameSpeechInitializeProgress*,
    Nba97GameSpeechInitializeAdapterProgress*);

int nba97_game_speech_initialize_registers_from_match_initialize(
    const Nba97GameMatchInitializeEvent*,
    const Nba97GameMatchInitializeRegisters*,
    Nba97GameSpeechInitializeRegisters*);

#ifdef __cplusplus
}
#endif
#endif
