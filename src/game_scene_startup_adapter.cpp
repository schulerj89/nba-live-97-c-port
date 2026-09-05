#include "game_scene_startup_adapter.h"

#include <cstring>

int nba97_game_scene_startup_from_scene_load(void* opaque,
    const Nba97GameTextMemory* memory, const Nba97GameSceneLoadEvent* event,
    Nba97GameSceneLoadRegisters* registers) {
    auto* binding = static_cast<Nba97GameSceneStartupBinding*>(opaque);
    if (!binding || !memory || !event || !registers ||
        event->kind != NBA97_GAME_SCENE_LOAD_CHILD_80048D5C ||
        event->pc != UINT32_C(0x8002db78) ||
        event->delay_slot_pc != UINT32_C(0x8002db7c) ||
        event->entry != UINT32_C(0x80048d5c) || event->argument_count != 0) {
        if (binding) binding->result = NBA97_TEXT_ARGUMENT;
        return 0;
    }
    if (!binding->access_journal && binding->access_journal_capacity) {
        binding->result = NBA97_TEXT_ARGUMENT;
        return 0;
    }
    Nba97GameSceneStartupContext context{};
    context.memory = *memory;
    context.operation_budget = binding->operation_budget;
    context.registers = *registers;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    ++binding->invocations;
    binding->result = nba97_game_scene_startup(&context, &binding->progress);
    if (binding->progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask == 0x0f)
        *registers = binding->progress.registers;
    return binding->result == NBA97_TEXT_COMPLETE;
}
