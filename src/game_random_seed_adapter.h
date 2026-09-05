#ifndef NBA97_GAME_RANDOM_SEED_ADAPTER_H
#define NBA97_GAME_RANDOM_SEED_ADAPTER_H

#include "recovered/game_random_seed.h"
#include "recovered/game_scene_random_warmup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameRandomSeedAdapterProgress {
    Nba97GameRandomSeedProgress seed;
    int seed_result;
    size_t seed_invocations;
    size_t unresolved_callbacks_completed;
    Nba97GameSceneRandomWarmupEvent seed_event;
} Nba97GameRandomSeedAdapterProgress;

/* Execute the natural warm-up caller while replacing only its proven
 * 0x800802D0 -> 0x80093694 seed boundary with the recovered owner. */
int nba97_game_scene_random_warmup_with_random_seed(
    const Nba97GameSceneRandomWarmupContext*,
    const Nba97GameRandomSeedContext*,
    Nba97GameSceneRandomWarmupProgress*,
    Nba97GameRandomSeedAdapterProgress*);

/* Compose one actual full-GPR warm-up event; usable by retained native chains.
 * Returns TEXT status and always preserves the owner's register prefix. */
int nba97_game_random_seed_from_warmup(const Nba97GameTextMemory*,
    const Nba97GameSceneRandomWarmupEvent*, Nba97GameSceneRandomWarmupRegisters*,
    const Nba97GameRandomSeedContext*, Nba97GameRandomSeedAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
