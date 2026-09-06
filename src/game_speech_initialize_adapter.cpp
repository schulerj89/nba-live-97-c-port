#include "game_speech_initialize_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameSpeechInitializeIo unresolved_io;
    void* unresolved_user;
    const Nba97GameSpeechInitializeDependencies* dependencies;
    Nba97GameSpeechInitializeAdapterProgress* out;
};

bool full(const Nba97GameSpeechInitializeWord& value) {
    return value.known_mask == 0x0f;
}

void forgetCallerSaved(Nba97GameSpeechInitializeRegisters& registers) {
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
    const Nba97GameSpeechInitializeEvent* event,
    Nba97GameSpeechInitializeRegisters* registers) {
    if (!run.unresolved_io)
        return 0;
    const int accepted = run.unresolved_io(run.unresolved_user, memory,
        event, registers);
    if (accepted == 1)
        ++run.out->unresolved_callbacks_completed;
    return accepted;
}

int resourceLoad(AdapterRun& run, const Nba97GameTextMemory* memory,
    const Nba97GameSpeechInitializeEvent* event,
    Nba97GameSpeechInitializeRegisters* registers) {
    const unsigned slot = static_cast<unsigned>(
        run.out->resource_loader_invocations);
    if (slot >= 3 || !run.dependencies->resource_loader_io ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_A0]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_A1]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_SP]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_RA]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_S0]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1]) ||
        !full(registers->gpr[NBA97_MATCH_INITIALIZE_GP]))
        return fallback(run, memory, event, registers);

    const auto incomingS0 = registers->gpr[NBA97_MATCH_INITIALIZE_S0];
    const auto incomingS1 = registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1];
    const auto incomingRa = registers->gpr[NBA97_MATCH_INITIALIZE_RA];
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
    registers->gpr[NBA97_MATCH_INITIALIZE_S0] =
        progress.stores >= 1 ? Nba97GameSpeechInitializeWord{
            context.filename, 0x0f} : incomingS0;
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] =
        progress.stores >= 2 ? Nba97GameSpeechInitializeWord{
            context.flags, 0x0f} : incomingS1;
    registers->gpr[NBA97_MATCH_INITIALIZE_RA] =
        progress.load_attempts || progress.stopped_entry == 0x800941c8u
            ? Nba97GameSpeechInitializeWord{0x80029c20u, 0x0f}
            : incomingRa;
    if (progress.load_attempts || progress.stopped_entry == 0x800941c8u)
        forgetCallerSaved(*registers);
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

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameSpeechInitializeEvent* event,
    Nba97GameSpeechInitializeRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (event->entry == 0x80029bfcu)
        return resourceLoad(run, memory, event, registers);
    return fallback(run, memory, event, registers);
}
}

int nba97_game_speech_initialize_with_recovered_dependencies(
    const Nba97GameSpeechInitializeContext* context,
    const Nba97GameSpeechInitializeDependencies* dependencies,
    Nba97GameSpeechInitializeProgress* progress,
    Nba97GameSpeechInitializeAdapterProgress* adapterProgress) {
    if (!context || !dependencies || !adapterProgress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapterProgress, 0, sizeof *adapterProgress);
    for (int& result : adapterProgress->resource_loader_result)
        result = NBA97_TEXT_COMPLETE;
    Nba97GameSpeechInitializeContext composed = *context;
    AdapterRun run{context->io, context->user, dependencies, adapterProgress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_speech_initialize(&composed, progress);
}

int nba97_game_speech_initialize_registers_from_match_initialize(
    const Nba97GameMatchInitializeEvent* event,
    const Nba97GameMatchInitializeRegisters* registers,
    Nba97GameSpeechInitializeRegisters* out) {
    if (!event || !registers || !out ||
        event->kind != NBA97_MATCH_INITIALIZE_CHILD_8007FD40 ||
        event->pc != 0x8002dbd8u || event->delay_slot_pc != 0x8002dbdcu ||
        event->entry != 0x8007fd40u || event->argument_count != 0)
        return NBA97_TEXT_ARGUMENT;
    *out = *registers;
    return NBA97_TEXT_COMPLETE;
}
