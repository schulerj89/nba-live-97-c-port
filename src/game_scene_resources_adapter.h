#ifndef NBA97_GAME_SCENE_RESOURCES_ADAPTER_H
#define NBA97_GAME_SCENE_RESOURCES_ADAPTER_H

#include "recovered/game_scene_resources.h"
#include "recovered/game_scene_startup.h"
#include "recovered/game_resource_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_GAME_SCENE_RESOURCES_LOADER_CALLS_MAX = 7 };

typedef struct Nba97GameSceneResourcesBinding {
    size_t operation_budget;
    Nba97GameSceneResourcesIo io; /* Unresolved-child fallback. */
    void* user;
    size_t resource_loader_operation_budget;
    Nba97GameResourceLoaderIo resource_loader_io;
    void* resource_loader_user;
    Nba97GameSceneResourcesAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GameSceneResourcesProgress progress;
    Nba97GameResourceLoaderProgress
        resource_loader[NBA97_GAME_SCENE_RESOURCES_LOADER_CALLS_MAX];
    int resource_loader_result[NBA97_GAME_SCENE_RESOURCES_LOADER_CALLS_MAX];
    size_t resource_loader_invocations;
    size_t unresolved_callbacks_completed;
    int result;
    size_t invocations;
} Nba97GameSceneResourcesBinding;

/* Compose the complete resource owner at scene-startup's 0x80048E94 child.
 * Compatible 0x80029BFC events additionally use its recovered retry-loader;
 * partial inputs fall back to the full-GPR callback. Exact completed or
 * refused prefix registers flow back to the caller. */
int nba97_game_scene_resources_from_scene_startup(void*,
    const Nba97GameTextMemory*, const Nba97GameSceneStartupEvent*,
    Nba97GameSceneStartupRegisters*);

#ifdef __cplusplus
}
#endif
#endif
