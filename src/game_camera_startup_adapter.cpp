#include "game_camera_startup_adapter.h"

#include <cstring>

int nba97_game_camera_startup_from_match_tick(void* opaque,
    const Nba97MatchTickCall* call, Nba97GamePeriodValue* value) {
    auto* binding = static_cast<Nba97GameCameraStartupTickBinding*>(opaque);
    if (!binding || !call)
        return NBA97_BODY_ARGUMENT;

    const bool camera_pc = call->pc == UINT32_C(0x80068c2c);
    const bool camera_entry = call->entry == UINT32_C(0x80079664);
    if (!camera_pc && !camera_entry) {
        if (!binding->fallback_service)
            return NBA97_MATCH_TICK_SERVICE_REQUIRED;
        return binding->fallback_service(binding->fallback_user, call, value);
    }

    const auto& entry_a0 =
        binding->entry_registers.gpr[NBA97_MATCH_INITIALIZE_A0];
    if (!camera_pc || !camera_entry || call->count != 1 || value ||
        call->args[0] != 0 || entry_a0.known_mask != 0x0f ||
        entry_a0.word != call->args[0] ||
        (!binding->memory.region && binding->memory.count) ||
        (!binding->access_journal && binding->access_journal_capacity)) {
        binding->result = NBA97_TEXT_ARGUMENT;
        return NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE;
    }

    Nba97GameCameraStartupContext context{};
    context.memory = binding->memory;
    context.operation_budget = binding->operation_budget;
    context.registers = binding->entry_registers;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    ++binding->invocations;
    std::memset(&binding->progress, 0, sizeof binding->progress);
    binding->result = nba97_game_camera_startup(&context, &binding->progress);
    return binding->result == NBA97_TEXT_COMPLETE ? NBA97_BODY_OK :
        NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE;
}
