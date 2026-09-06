#include "game_late_period_limits_adapter.h"

#include <cstring>

namespace {
int mapResult(int result) {
    switch (result) {
    case NBA97_TEXT_COMPLETE: return NBA97_BODY_OK;
    case NBA97_TEXT_ARGUMENT: return NBA97_BODY_ARGUMENT;
    case NBA97_TEXT_RESOURCE: return NBA97_BODY_BOUNDS;
    case NBA97_TEXT_UNKNOWN: return NBA97_BODY_UNKNOWN;
    case NBA97_TEXT_ALIGNMENT_TRAP: return NBA97_BODY_ALIGNMENT_TRAP;
    case NBA97_TEXT_LIMIT: return NBA97_BODY_JOURNAL_LIMIT;
    default: return NBA97_BODY_ARGUMENT;
    }
}
}

int nba97_game_late_period_limits_from_match_tick(void* opaque,
    const Nba97MatchTickCall* call, Nba97GamePeriodValue* value) {
    auto* binding =
        static_cast<Nba97GameLatePeriodLimitsTickBinding*>(opaque);
    if (!binding || !call)
        return NBA97_BODY_ARGUMENT;

    const bool exact_pc = call->pc == UINT32_C(0x80068cec);
    const bool exact_entry = call->entry == UINT32_C(0x80067550);
    if (!exact_pc && !exact_entry) {
        if (!binding->fallback_service)
            return NBA97_MATCH_TICK_SERVICE_REQUIRED;
        return binding->fallback_service(binding->fallback_user, call, value);
    }

    binding->owner_result = NBA97_TEXT_ARGUMENT;
    if (!exact_pc || !exact_entry || value || call->count != 0u ||
        call->args[0] != 0u || call->args[1] != 0u || !binding->limits ||
        binding->entry_context_source_proven > 1u)
        return NBA97_BODY_ARGUMENT;
    if (!binding->entry_context_source_proven)
        return NBA97_GAME_LATE_PERIOD_LIMITS_TICK_CONTEXT_REQUIRED;

    Nba97GameLatePeriodLimitsContext context = *binding->limits;
    ++binding->invocations;
    std::memset(&binding->progress, 0, sizeof binding->progress);
    binding->owner_result =
        nba97_game_late_period_limits(&context, &binding->progress);
    return mapResult(binding->owner_result);
}
