#include "game_scene_resources_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameSceneResourcesBinding* binding;
};

bool full(const Nba97GameSceneResourcesWord& value) {
    return value.known_mask == 0x0f;
}

void forget_caller_saved(Nba97GameSceneResourcesRegisters& registers) {
    static constexpr unsigned indices[] = {
        NBA97_MATCH_INITIALIZE_AT, NBA97_MATCH_INITIALIZE_V1,
        NBA97_MATCH_INITIALIZE_A0, NBA97_MATCH_INITIALIZE_A1,
        NBA97_MATCH_INITIALIZE_A2, NBA97_MATCH_INITIALIZE_A3,
        NBA97_MATCH_INITIALIZE_T0, NBA97_MATCH_INITIALIZE_T0 + 1,
        NBA97_MATCH_INITIALIZE_T0 + 2, NBA97_MATCH_INITIALIZE_T0 + 3,
        NBA97_MATCH_INITIALIZE_T0 + 4, NBA97_MATCH_INITIALIZE_T0 + 5,
        NBA97_MATCH_INITIALIZE_T0 + 6, NBA97_MATCH_INITIALIZE_T0 + 7,
        NBA97_MATCH_INITIALIZE_T8, NBA97_MATCH_INITIALIZE_T9
    };
    for (const unsigned index : indices)
        registers.gpr[index].known_mask = 0;
}

int fallback(AdapterRun& run, const Nba97GameTextMemory* memory,
    const Nba97GameSceneResourcesEvent* event,
    Nba97GameSceneResourcesRegisters* registers) {
    if (!run.binding->io)
        return 0;
    const int accepted = run.binding->io(run.binding->user, memory, event,
        registers);
    if (accepted == 1)
        ++run.binding->unresolved_callbacks_completed;
    return accepted;
}

int resource_load(AdapterRun& run, const Nba97GameTextMemory* memory,
    const Nba97GameSceneResourcesEvent* event,
    Nba97GameSceneResourcesRegisters* registers) {
    auto& binding = *run.binding;
    const auto slot = binding.resource_loader_invocations;
    if (slot >= NBA97_GAME_SCENE_RESOURCES_LOADER_CALLS_MAX ||
        !binding.resource_loader_io ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_A0]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_A1]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_SP]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_RA]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_S0]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_GP]))
        return fallback(run, memory, event, registers);

    const auto incoming_s0 =
        registers->gpr[NBA97_MATCH_INITIALIZE_S0];
    const auto incoming_s1 =
        registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1];
    const auto incoming_ra = registers->gpr[NBA97_MATCH_INITIALIZE_RA];
    Nba97GameResourceLoaderContext context{
        *memory,
        binding.resource_loader_operation_budget,
        registers->gpr[NBA97_MATCH_INITIALIZE_A0].word,
        registers->gpr[NBA97_MATCH_INITIALIZE_A1].word,
        registers->gpr[NBA97_MATCH_INITIALIZE_SP].word,
        registers->gpr[NBA97_MATCH_INITIALIZE_RA].word,
        {registers->gpr[NBA97_MATCH_INITIALIZE_S0].word,
            registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word},
        registers->gpr[NBA97_MATCH_INITIALIZE_GP].word,
        binding.resource_loader_io,
        binding.resource_loader_user
    };
    ++binding.resource_loader_invocations;
    auto& progress = binding.resource_loader[slot];
    const int result = nba97_game_resource_loader(&context, &progress);
    binding.resource_loader_result[slot] = result;

    registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
        progress.stack_pointer, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0] = progress.stores >= 1
        ? Nba97GameSceneResourcesWord{context.filename, 0x0f} : incoming_s0;
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = progress.stores >= 2
        ? Nba97GameSceneResourcesWord{context.flags, 0x0f} : incoming_s1;
    registers->gpr[NBA97_MATCH_INITIALIZE_RA] =
        progress.load_attempts || progress.stopped_entry == 0x800941c8u
            ? Nba97GameSceneResourcesWord{0x80029c20u, 0x0f}
            : incoming_ra;
    if (progress.load_attempts || progress.stopped_entry == 0x800941c8u)
        forget_caller_saved(*registers);

    if (result != NBA97_TEXT_COMPLETE) {
        if (progress.return_v0_known)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                progress.return_v0, 0x0f};
        else if (progress.load_attempts ||
            progress.stopped_entry == 0x800941c8u)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0].known_mask = 0;
        if (progress.reads >= 1)
            registers->gpr[NBA97_MATCH_INITIALIZE_RA] = {
                progress.restored_return_address, 0x0f};
        else if (progress.stopped_pc == 0x80029c28u)
            registers->gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0;
        if (progress.stopped_pc == 0x80029c2cu || progress.reads >= 2)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1].known_mask = 0;
        if (progress.stopped_pc == 0x80029c30u)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0].known_mask = 0;
        return 0;
    }

    registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
        progress.return_v0,
        static_cast<std::uint8_t>(progress.return_v0_known ? 0x0f : 0)};
    registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
        progress.stack_pointer, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_RA] = {
        progress.restored_return_address, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {
        progress.restored_saved_register[0], 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {
        progress.restored_saved_register[1], 0x0f};
    return 1;
}

int dispatch(void* opaque, const Nba97GameTextMemory* memory,
    const Nba97GameSceneResourcesEvent* event,
    Nba97GameSceneResourcesRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(opaque);
    if (event->entry == 0x80029bfcu)
        return resource_load(run, memory, event, registers);
    return fallback(run, memory, event, registers);
}
}

int nba97_game_scene_resources_from_scene_startup(void* opaque,
    const Nba97GameTextMemory* memory,
    const Nba97GameSceneStartupEvent* event,
    Nba97GameSceneStartupRegisters* registers) {
    auto* binding = static_cast<Nba97GameSceneResourcesBinding*>(opaque);
    if (!binding || !memory || !event || !registers ||
        event->kind != NBA97_GAME_SCENE_STARTUP_CHILD_80052C20 ||
        event->pc != UINT32_C(0x80048e94) ||
        event->delay_slot_pc != UINT32_C(0x80048e98) ||
        event->entry != UINT32_C(0x80052c20) || event->argument_count != 0 ||
        (!binding->access_journal && binding->access_journal_capacity)) {
        if (binding) binding->result = NBA97_TEXT_ARGUMENT;
        return 0;
    }
    Nba97GameSceneResourcesContext context{};
    context.memory = *memory;
    context.operation_budget = binding->operation_budget;
    context.registers = *registers;
    AdapterRun run{binding};
    context.io = dispatch;
    context.user = &run;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    std::memset(binding->resource_loader, 0,
        sizeof binding->resource_loader);
    std::memset(binding->resource_loader_result, 0,
        sizeof binding->resource_loader_result);
    binding->resource_loader_invocations = 0;
    binding->unresolved_callbacks_completed = 0;
    ++binding->invocations;
    binding->result = nba97_game_scene_resources(&context,
        &binding->progress);
    if (binding->progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO]
            .known_mask == 0x0f)
        *registers = binding->progress.registers;
    return binding->result == NBA97_TEXT_COMPLETE;
}
