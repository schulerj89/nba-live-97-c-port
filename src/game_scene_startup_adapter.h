#ifndef NBA97_GAME_SCENE_STARTUP_ADAPTER_H
#define NBA97_GAME_SCENE_STARTUP_ADAPTER_H

#include "recovered/game_scene_load.h"
#include "recovered/game_scene_startup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameSceneStartupBinding {
    size_t operation_budget;
    Nba97GameSceneStartupIo io;
    void* user;
    Nba97GameSceneStartupAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GameSceneStartupProgress progress;
    int result;
    size_t invocations;
} Nba97GameSceneStartupBinding;

/* Narrow callback for the frozen scene-load owner's 0x8002DB78 child event.
 * The owner's exact prefix registers are returned even when a nested service
 * refuses, while callback success means the whole 0x80048D5C owner returned. */
int nba97_game_scene_startup_from_scene_load(void*,
    const Nba97GameTextMemory*, const Nba97GameSceneLoadEvent*,
    Nba97GameSceneLoadRegisters*);

#ifdef __cplusplus
}
#endif
#endif
