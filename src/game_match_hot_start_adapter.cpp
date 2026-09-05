#include "game_match_hot_start_adapter.h"

int nba97_game_match_hot_start_dispatch_tick(
    Nba97GameMatchHotStartTickAdapter* adapter,
    const Nba97MatchTickCall* call, Nba97GamePeriodValue* result) {
    if (!adapter || !call)
        return NBA97_BODY_ARGUMENT;
    if (call->pc == UINT32_C(0x80068c24) &&
        call->entry == UINT32_C(0x80066f88)) {
        if (call->count != 0 || result || !adapter->hot_start_context ||
            !adapter->hot_start_progress)
            return NBA97_BODY_ARGUMENT;
        ++adapter->hot_start_invocations;
        adapter->hot_start_result = nba97_game_match_hot_start(
            adapter->hot_start_context, adapter->hot_start_progress);
        return adapter->hot_start_result == NBA97_TEXT_COMPLETE ?
            NBA97_BODY_OK : NBA97_MATCH_HOT_START_TICK_INCOMPLETE;
    }
    if (!adapter->fallback_service)
        return NBA97_MATCH_TICK_SERVICE_REQUIRED;
    ++adapter->fallback_invocations;
    return adapter->fallback_service(adapter->fallback_user, call, result);
}
