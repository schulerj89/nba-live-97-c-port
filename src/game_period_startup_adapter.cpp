#include "game_period_startup_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
int mapResult(int result) {
    switch (result) {
    case NBA97_TEXT_COMPLETE:
        return NBA97_BODY_OK;
    case NBA97_TEXT_ARGUMENT:
        return NBA97_BODY_ARGUMENT;
    case NBA97_TEXT_RESOURCE:
        return NBA97_BODY_BOUNDS;
    case NBA97_TEXT_UNKNOWN:
        return NBA97_BODY_UNKNOWN;
    case NBA97_TEXT_ALIGNMENT_TRAP:
        return NBA97_BODY_ALIGNMENT_TRAP;
    case NBA97_TEXT_LIMIT:
        return NBA97_BODY_JOURNAL_LIMIT;
    case NBA97_TEXT_IO_REFUSED:
        return NBA97_GAME_PERIOD_STARTUP_TICK_CHILD_REQUIRED;
    default:
        return NBA97_BODY_ARGUMENT;
    }
}
}

int nba97_game_period_startup_from_match_tick(
    const Nba97MatchTickCall* call,
    const Nba97GamePeriodStartupMatchTickContext* context,
    Nba97GamePeriodStartupProgress* progress,
    Nba97GamePeriodStartupAdapterProgress* adapter_progress) {
    if (!adapter_progress)
        return NBA97_BODY_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->owner_result = NBA97_TEXT_ARGUMENT;
    if (!call || !context || !progress || !context->period ||
        context->entry_context_source_proven > 1u ||
        call->pc != UINT32_C(0x80068c4c) ||
        call->entry != UINT32_C(0x80067468) || call->count != 0u ||
        call->args[0] != 0u || call->args[1] != 0u)
        return NBA97_BODY_ARGUMENT;
    if (!context->entry_context_source_proven)
        return NBA97_GAME_PERIOD_STARTUP_TICK_CONTEXT_REQUIRED;

    Nba97GamePeriodStartupContext period = *context->period;
    adapter_progress->invocations = 1;
    adapter_progress->source_context_used = 1;
    adapter_progress->owner_result =
        nba97_game_period_startup(&period, progress);
    return mapResult(adapter_progress->owner_result);
}
