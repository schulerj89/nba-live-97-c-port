#include "game_camera_select_adapter.h"

#include <cstring>

int nba97_game_camera_select_from_camera_startup(void* opaque,
    const Nba97GameTextMemory* memory,
    const Nba97GameCameraStartupEvent* event,
    Nba97GameCameraStartupRegisters* registers) {
    auto* binding = static_cast<Nba97GameCameraSelectStartupBinding*>(opaque);
    if (!binding)
        return 0;
    if (!memory || !event || !registers ||
        (event->pc != UINT32_C(0x800796b8) &&
            event->pc != UINT32_C(0x800796e4)) ||
        event->delay_slot_pc != event->pc + 4u ||
        event->entry != UINT32_C(0x800799cc) ||
        event->kind != NBA97_GAME_CAMERA_STARTUP_CHILD_800799CC ||
        event->argument_count != 2 ||
        (!memory->region && memory->count) ||
        (!binding->access_journal && binding->access_journal_capacity)) {
        binding->result = NBA97_TEXT_ARGUMENT;
        return 0;
    }

    Nba97GameCameraSelectContext context{};
    context.memory = *memory;
    context.operation_budget = binding->operation_budget;
    context.registers = *registers;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    ++binding->invocations;
    binding->caller_pc = event->pc;
    binding->entry_registers = *registers;
    std::memset(&binding->progress, 0, sizeof binding->progress);
    binding->result = nba97_game_camera_select(&context, &binding->progress);
    *registers = binding->progress.registers;
    return binding->result == NBA97_TEXT_COMPLETE ? 1 : 0;
}
