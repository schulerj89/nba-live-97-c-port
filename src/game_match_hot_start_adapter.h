#ifndef NBA97_GAME_MATCH_HOT_START_ADAPTER_H
#define NBA97_GAME_MATCH_HOT_START_ADAPTER_H

#include "recovered/game_match_hot_start.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NBA97_MATCH_HOT_START_TICK_INCOMPLETE = -20
};

typedef struct Nba97GameMatchHotStartTickAdapter {
    /* Required explicit entry state. The existing tick call record contains
     * neither the source stack pointer nor the full live GPR set, so this may
     * never be synthesized from Nba97MatchTickCall. */
    Nba97GameMatchHotStartContext* hot_start_context;
    Nba97GameMatchHotStartProgress* hot_start_progress;
    Nba97MatchTickService fallback_service;
    void* fallback_user;
    int hot_start_result;
    size_t hot_start_invocations;
    size_t fallback_invocations;
} Nba97GameMatchHotStartTickAdapter;

/* Route the natural tick's exact 0x80068C24 -> 0x80066F88 boundary through
 * the recovered owner. Other services are forwarded or explicitly refused.
 * The adapter returns BODY-compatible status and retains the exact TEXT result
 * in hot_start_result when the owner does not complete. */
int nba97_game_match_hot_start_dispatch_tick(
    Nba97GameMatchHotStartTickAdapter*, const Nba97MatchTickCall*,
    Nba97GamePeriodValue*);

#ifdef __cplusplus
}
#endif
#endif
