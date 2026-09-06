#include "game_controller_frame_reset_adapter.h"

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
        return NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CHILD_INCOMPLETE;
    default:
        return NBA97_BODY_ARGUMENT;
    }
}
}

int nba97_game_controller_frame_reset_from_match_tick(void* opaque,
    const Nba97MatchTickCall* call, Nba97GamePeriodValue* value) {
    auto* binding =
        static_cast<Nba97GameControllerFrameResetTickBinding*>(opaque);
    if (!binding || !call)
        return NBA97_BODY_ARGUMENT;

    const bool reset_pc = call->pc == UINT32_C(0x80068cf4);
    const bool reset_entry = call->entry == UINT32_C(0x800675e4);
    if (!reset_pc && !reset_entry) {
        if (!binding->fallback_service)
            return NBA97_MATCH_TICK_SERVICE_REQUIRED;
        return binding->fallback_service(binding->fallback_user, call, value);
    }

    binding->result = NBA97_TEXT_ARGUMENT;
    if (!reset_pc || !reset_entry || call->count != 0u || value ||
        call->args[0] != 0u || call->args[1] != 0u ||
        binding->entry_context_source_proven > 1u ||
        (!binding->memory.region && binding->memory.count) ||
        (!binding->access_journal && binding->access_journal_capacity))
        return NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CHILD_INCOMPLETE;
    if (!binding->entry_context_source_proven)
        return NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CONTEXT_REQUIRED;

    Nba97GameControllerFrameResetContext context{};
    context.memory = binding->memory;
    context.operation_budget = binding->operation_budget;
    context.registers = binding->entry_registers;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    ++binding->invocations;
    std::memset(&binding->progress, 0, sizeof binding->progress);
    binding->result = nba97_game_controller_frame_reset(
        &context, &binding->progress);
    return mapResult(binding->result);
}
