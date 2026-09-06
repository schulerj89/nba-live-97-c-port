#include "game_tipoff_announcement_adapter.h"

#include <cstdint>

int nba97_game_tipoff_announcement_from_first_period_startup(void* opaque,
    const Nba97GameTextMemory* memory,
    const Nba97GameFirstPeriodStartupEvent* event,
    Nba97GameFirstPeriodStartupRegisters* registers) {
    auto* binding = static_cast<Nba97GameTipoffAnnouncementBinding*>(opaque);
    if (!binding || !memory || !event || !registers ||
        event->kind != NBA97_GAME_FIRST_PERIOD_STARTUP_7EF4C ||
        event->pc != UINT32_C(0x80067450) ||
        event->delay_slot_pc != UINT32_C(0x80067454) ||
        event->entry != UINT32_C(0x8007ef4c) || event->argument_count != 0 ||
        (!binding->access_journal && binding->access_journal_capacity)) {
        if (binding)
            binding->result = NBA97_TEXT_ARGUMENT;
        return 0;
    }

    Nba97GameTipoffAnnouncementContext context{};
    context.memory = *memory;
    context.operation_budget = binding->operation_budget;
    context.registers = *registers;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    ++binding->invocations;
    binding->result =
        nba97_game_tipoff_announcement(&context, &binding->progress);
    if (binding->progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO]
            .known_mask == 0x0f)
        *registers = binding->progress.registers;
    return binding->result == NBA97_TEXT_COMPLETE;
}
