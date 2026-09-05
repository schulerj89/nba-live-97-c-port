#ifndef NBA97_GAME_SCENE_RANDOM_WARMUP_ADAPTER_H
#define NBA97_GAME_SCENE_RANDOM_WARMUP_ADAPTER_H

#include "recovered/game_scene_load.h"
#include "recovered/game_scene_random_warmup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameSceneRandomWarmupAdapterProgress {
    Nba97GameSceneRandomWarmupProgress warmup;
    int warmup_result;
    size_t warmup_invocations;
    size_t unresolved_callbacks_completed;
    Nba97GameSceneLoadEvent warmup_event;
} Nba97GameSceneRandomWarmupAdapterProgress;

/* Execute the scene wrapper while routing only its proven 0x800802AC child
 * to the recovered warm-up owner. The wrapper's other child stays explicit. */
int nba97_game_scene_load_with_random_warmup(
    const Nba97GameSceneLoadContext*,
    const Nba97GameSceneRandomWarmupContext*,
    Nba97GameSceneLoadProgress*,
    Nba97GameSceneRandomWarmupAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
