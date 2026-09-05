#ifndef NBA97_GAME_LOOP_ENTRY_ADAPTER_H
#define NBA97_GAME_LOOP_ENTRY_ADAPTER_H

#include "recovered/game_loop_entry.h"
#include "recovered/game_match_session.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameLoopEntryMatchTickServices {
    Nba97MatchTickService service;
    Nba97MatchTickPlayerUpdate player_update;
    Nba97MatchTickBallSimulation ball_simulation;
    Nba97MatchTickNetTransform net_transform;
    Nba97MatchTickMatchFrame match_frame;
    void* user;
    size_t operation_budget;
} Nba97GameLoopEntryMatchTickServices;

typedef struct Nba97GameLoopEntryAdapterProgress {
    Nba97MatchTickProgress match_tick;
    int match_tick_result;
    size_t match_tick_invocations;
    uint8_t output_registers_available;
} Nba97GameLoopEntryAdapterProgress;

/* Route the wrapper's sole child into the existing complete match-tick owner.
 * The same retained byte memory supplies tick reads/writes. The tick API is
 * non-resumable and exposes no output GPRs or guest stack state, so every
 * invocation refuses wrapper continuation even when the tick itself completes.
 * Its exact prefix/result remains in adapter progress and retained memory. */
int nba97_game_loop_entry_with_match_tick(
    const Nba97GameLoopEntryContext*,
    const Nba97GameLoopEntryMatchTickServices*,
    Nba97GameLoopEntryProgress*, Nba97GameLoopEntryAdapterProgress*);

/* Build the register subset exposed at match-session's natural 0x8002DA8C
 * call. Registers absent from that older event API remain explicitly unknown. */
int nba97_game_loop_entry_registers_from_session(
    const Nba97GameMatchSessionEvent*, Nba97GameMatchInitializeRegisters*);

#ifdef __cplusplus
}
#endif
#endif
