#ifndef NBA97_GAME_SCENE_LOAD_ADAPTER_H
#define NBA97_GAME_SCENE_LOAD_ADAPTER_H

#include "recovered/game_match_session.h"
#include "recovered/game_scene_load.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Build the live register subset proved by match-session's 0x8002DA84 event.
 * Registers absent from the caller's older boundary API remain unknown. */
int nba97_game_scene_load_registers_from_session(
    const Nba97GameMatchSessionEvent*, Nba97GameSceneLoadRegisters*);

#ifdef __cplusplus
}
#endif
#endif
