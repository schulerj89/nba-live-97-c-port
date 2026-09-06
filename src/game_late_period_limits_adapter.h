#ifndef NBA97_GAME_LATE_PERIOD_LIMITS_ADAPTER_H
#define NBA97_GAME_LATE_PERIOD_LIMITS_ADAPTER_H

#include "recovered/game_late_period_limits.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NBA97_GAME_LATE_PERIOD_LIMITS_TICK_CONTEXT_REQUIRED = -20
};

typedef struct Nba97GameLatePeriodLimitsTickBinding {
    /* Nba97MatchTickCall does not contain the caller's 32 GPRs or retained
     * memory. This complete entry context must be independently source-proven. */
    const Nba97GameLatePeriodLimitsContext* limits;
    uint8_t entry_context_source_proven;
    Nba97MatchTickService fallback_service;
    void* fallback_user;
    Nba97GameLatePeriodLimitsProgress progress;
    int owner_result;
    size_t invocations;
} Nba97GameLatePeriodLimitsTickBinding;

/* Match-tick service adapter for exactly 0x80068CEC -> 0x80067550. Earlier
 * services and the next 0x80068CF4 -> 0x800675E4 boundary are forwarded. */
int nba97_game_late_period_limits_from_match_tick(void*,
    const Nba97MatchTickCall*, Nba97GamePeriodValue*);

#ifdef __cplusplus
}
#endif
#endif
