#include "game_first_period_startup_adapter.h"

#include <cstdint>

int nba97_game_first_period_startup_from_period_startup(void* opaque,
    const Nba97GameTextMemory* memory,
    const Nba97GamePeriodStartupEvent* event,
    Nba97GamePeriodStartupRegisters* registers) {
    auto* binding = static_cast<Nba97GameFirstPeriodStartupBinding*>(opaque);
    if (!binding || !memory || !event || !registers ||
        event->kind != NBA97_GAME_PERIOD_STARTUP_ZERO_PERIOD_SERVICE ||
        event->pc != UINT32_C(0x80067494) ||
        event->delay_slot_pc != UINT32_C(0x80067498) ||
        event->entry != UINT32_C(0x800673f0) || event->argument_count != 0 ||
        (!binding->access_journal && binding->access_journal_capacity)) {
        if (binding)
            binding->result = NBA97_TEXT_ARGUMENT;
        return 0;
    }

    Nba97GameFirstPeriodStartupContext context{};
    context.memory = *memory;
    context.operation_budget = binding->operation_budget;
    context.registers = *registers;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    ++binding->invocations;
    binding->result =
        nba97_game_first_period_startup(&context, &binding->progress);
    if (binding->progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO]
            .known_mask == 0x0f)
        *registers = binding->progress.registers;
    return binding->result == NBA97_TEXT_COMPLETE;
}
