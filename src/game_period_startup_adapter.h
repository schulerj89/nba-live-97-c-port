#ifndef NBA97_GAME_PERIOD_STARTUP_ADAPTER_H
#define NBA97_GAME_PERIOD_STARTUP_ADAPTER_H

#include "recovered/game_match_tick.h"
#include "recovered/game_period_startup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GamePeriodStartupMatchTickContext {
    /* The legacy tick event has no GPR/SP payload. This complete owner context
     * must therefore come from an independent source-proven child entry. */
    const Nba97GamePeriodStartupContext* period;
    uint8_t entry_context_source_proven;
} Nba97GamePeriodStartupMatchTickContext;

typedef struct Nba97GamePeriodStartupAdapterProgress {
    size_t invocations;
    int owner_result;
    uint8_t source_context_used;
} Nba97GamePeriodStartupAdapterProgress;

enum Nba97GamePeriodStartupAdapterResult {
    NBA97_GAME_PERIOD_STARTUP_TICK_CONTEXT_REQUIRED = -20,
    NBA97_GAME_PERIOD_STARTUP_TICK_CHILD_REQUIRED = -21
};

/* Invoke 0x80067468 only for its natural match-tick event. The adapter never
 * derives registers from Nba97MatchTickCall: its args describe only a0/a1 and
 * do not establish sp, ra, AT, or the other live GPRs. The supplied period
 * context is copied so the caller's configuration is not rewritten. Returns
 * an NBA97_BODY_* compatible status (or one of the explicit adapter statuses). */
int nba97_game_period_startup_from_match_tick(
    const Nba97MatchTickCall*,
    const Nba97GamePeriodStartupMatchTickContext*,
    Nba97GamePeriodStartupProgress*,
    Nba97GamePeriodStartupAdapterProgress*);

#ifdef __cplusplus
}
#endif
#endif
