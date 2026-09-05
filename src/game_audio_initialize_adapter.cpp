#include "game_audio_initialize_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameAudioInitializeIo unresolved_io;
    void* unresolved_user;
    const Nba97GameAudioInitializeDependencies* dependencies;
    Nba97GameAudioInitializeAdapterProgress* out;
};

bool full(const Nba97GameAudioInitializeWord& value) {
    return value.known_mask == 0x0f;
}

void forgetCallerSaved(Nba97GameAudioInitializeRegisters& registers) {
    static constexpr unsigned indices[] = {
        NBA97_MATCH_INITIALIZE_AT,
        NBA97_MATCH_INITIALIZE_V1,
        NBA97_MATCH_INITIALIZE_A0,
        NBA97_MATCH_INITIALIZE_A1,
        NBA97_MATCH_INITIALIZE_A2,
        NBA97_MATCH_INITIALIZE_A3,
        NBA97_MATCH_INITIALIZE_T0,
        NBA97_MATCH_INITIALIZE_T0 + 1,
        NBA97_MATCH_INITIALIZE_T0 + 2,
        NBA97_MATCH_INITIALIZE_T0 + 3,
        NBA97_MATCH_INITIALIZE_T0 + 4,
        NBA97_MATCH_INITIALIZE_T0 + 5,
        NBA97_MATCH_INITIALIZE_T0 + 6,
        NBA97_MATCH_INITIALIZE_T0 + 7,
        NBA97_MATCH_INITIALIZE_T8,
        NBA97_MATCH_INITIALIZE_T9
    };
    for (const unsigned index : indices)
        registers.gpr[index].known_mask = 0;
}

int fallback(AdapterRun& run, const Nba97GameTextMemory* memory,
    const Nba97GameAudioInitializeEvent* event,
    Nba97GameAudioInitializeRegisters* registers) {
    if (!run.unresolved_io)
        return 0;
    const int accepted = run.unresolved_io(run.unresolved_user, memory, event,
        registers);
    if (accepted == 1)
        ++run.out->unresolved_callbacks_completed;
    return accepted;
}

int resourceLoad(AdapterRun& run, const Nba97GameTextMemory* memory,
    const Nba97GameAudioInitializeEvent* event,
    Nba97GameAudioInitializeRegisters* registers) {
    const unsigned slot = static_cast<unsigned>(
        run.out->resource_loader_invocations);
    if (slot >= 2 || !run.dependencies->resource_loader_io ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_A0]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_A1]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_SP]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_RA]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_S0]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_GP]))
        return fallback(run, memory, event, registers);

    const auto incoming_s0 = registers->gpr[NBA97_MATCH_INITIALIZE_S0];
    const auto incoming_s1 = registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1];
    const auto incoming_ra = registers->gpr[NBA97_MATCH_INITIALIZE_RA];
    Nba97GameResourceLoaderContext context{
        *memory,
        run.dependencies->resource_loader_operation_budget,
        registers->gpr[NBA97_MATCH_INITIALIZE_A0].word,
        registers->gpr[NBA97_MATCH_INITIALIZE_A1].word,
        registers->gpr[NBA97_MATCH_INITIALIZE_SP].word,
        registers->gpr[NBA97_MATCH_INITIALIZE_RA].word,
        {registers->gpr[NBA97_MATCH_INITIALIZE_S0].word,
            registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word},
        registers->gpr[NBA97_MATCH_INITIALIZE_GP].word,
        run.dependencies->resource_loader_io,
        run.dependencies->resource_loader_user
    };
    ++run.out->resource_loader_invocations;
    auto& progress = run.out->resource_loader[slot];
    const int result = nba97_game_resource_loader(&context, &progress);
    run.out->resource_loader_result[slot] = result;

    registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
        progress.stack_pointer, 0x0f};
    /* 0x80029C00 SW s0 completes before 0x80029C04 MOVE s0,a0;
       0x80029C08 SW s1 likewise precedes 0x80029C0C MOVE s1,a1. The owner's
       store counter therefore proves exactly when each move was reached. */
    registers->gpr[NBA97_MATCH_INITIALIZE_S0] =
        progress.stores >= 1 ? Nba97GameAudioInitializeWord{
            context.filename, 0x0f} : incoming_s0;
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] =
        progress.stores >= 2 ? Nba97GameAudioInitializeWord{
            context.flags, 0x0f} : incoming_s1;
    registers->gpr[NBA97_MATCH_INITIALIZE_RA] =
        progress.load_attempts || progress.stopped_entry == 0x800941c8u
            ? Nba97GameAudioInitializeWord{0x80029c20u, 0x0f}
            : incoming_ra;
    if (progress.load_attempts || progress.stopped_entry == 0x800941c8u)
        forgetCallerSaved(*registers);
    if (result != NBA97_TEXT_COMPLETE) {
        if (progress.return_v0_known)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                progress.return_v0, 0x0f};
        else if (progress.load_attempts ||
            progress.stopped_entry == 0x800941c8u)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0].known_mask = 0;
        /* The child publishes a restored ra after the first completed
           epilogue read. Its s1 temporary is not exposed until all three reads
           complete, so a later epilogue failure must not fabricate it. */
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

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameAudioInitializeEvent* event,
    Nba97GameAudioInitializeRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (event->entry == 0x80029bfcu)
        return resourceLoad(run, memory, event, registers);
    return fallback(run, memory, event, registers);
}
}

int nba97_game_audio_initialize_with_recovered_dependencies(
    const Nba97GameAudioInitializeContext* context,
    const Nba97GameAudioInitializeDependencies* dependencies,
    Nba97GameAudioInitializeProgress* progress,
    Nba97GameAudioInitializeAdapterProgress* adapter_progress) {
    if (!context || !dependencies || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->resource_loader_result[0] = NBA97_TEXT_COMPLETE;
    adapter_progress->resource_loader_result[1] = NBA97_TEXT_COMPLETE;
    Nba97GameAudioInitializeContext composed = *context;
    AdapterRun run{context->io, context->user, dependencies,
        adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_audio_initialize(&composed, progress);
}

int nba97_game_audio_initialize_registers_from_match_initialize(
    const Nba97GameMatchInitializeEvent* event,
    const Nba97GameMatchInitializeRegisters* registers,
    Nba97GameAudioInitializeRegisters* out) {
    if (!event || !registers || !out ||
        event->kind != NBA97_MATCH_INITIALIZE_CHILD_80029114 ||
        event->pc != 0x8002dbd0u || event->delay_slot_pc != 0x8002dbd4u ||
        event->entry != 0x80029114u || event->argument_count != 0)
        return NBA97_TEXT_ARGUMENT;
    *out = *registers;
    return NBA97_TEXT_COMPLETE;
}
