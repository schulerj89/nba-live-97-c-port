#include "game_match_initialize_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameMatchInitializeIo unresolved_io;
    void* unresolved_user;
    std::size_t zero_budget;
    Nba97GameMatchInitializeAdapterProgress* out;
};

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameMatchInitializeEvent* event,
    Nba97GameMatchInitializeRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (event->kind == NBA97_MATCH_INITIALIZE_MEMORY_ZERO) {
        const Nba97GameMatchInitializeWord incoming_t0 =
            registers->gpr[NBA97_MATCH_INITIALIZE_T0];
        const Nba97GameMatchInitializeWord incoming_t1 =
            registers->gpr[NBA97_MATCH_INITIALIZE_T0 + 1];
        ++run.out->memory_zero_invocations;
        Nba97GameMemoryZeroContext context{*memory, run.zero_budget,
            registers->gpr[NBA97_MATCH_INITIALIZE_A0].word,
            registers->gpr[NBA97_MATCH_INITIALIZE_A1].word,
            registers->gpr[NBA97_MATCH_INITIALIZE_V0].word,
            static_cast<std::uint8_t>(
                registers->gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0f)};
        run.out->memory_zero_result = nba97_game_memory_zero(&context,
            &run.out->memory_zero);
        /* The recovered child exposes its actual live a0/a1 and assigns a2=0.
           Its v0 is untouched, including partial per-byte knownness retained
           by this wider orchestration interface. */
        registers->gpr[NBA97_MATCH_INITIALIZE_A0] = {
            run.out->memory_zero.working_destination, 0x0f};
        registers->gpr[NBA97_MATCH_INITIALIZE_A1] = {
            run.out->memory_zero.working_count, 0x0f};
        registers->gpr[NBA97_MATCH_INITIALIZE_A2] = {0, 0x0f};
        /* The recovered zero owner does not expose these internal scratch
           outputs. For this fixed large clear, the listing proves AT/a2/t2
           before the first attempted SWR. t0/t1 change only after that store. */
        registers->gpr[NBA97_MATCH_INITIALIZE_AT] = {0, 0x0f};
        registers->gpr[NBA97_MATCH_INITIALIZE_T0 + 2] = {0, 0x0f};
        if (run.out->memory_zero.stores) {
            registers->gpr[NBA97_MATCH_INITIALIZE_T0] = {4, 0x0f};
            registers->gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = {0, 0x0f};
        } else {
            registers->gpr[NBA97_MATCH_INITIALIZE_T0] = incoming_t0;
            registers->gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = incoming_t1;
        }
        return run.out->memory_zero_result == NBA97_TEXT_COMPLETE;
    }
    if (!run.unresolved_io)
        return 0;
    const int accepted = run.unresolved_io(run.unresolved_user, memory, event,
        registers);
    if (accepted == 1)
        ++run.out->unresolved_callbacks_completed;
    return accepted;
}
}

int nba97_game_match_initialize_with_zero(
    const Nba97GameMatchInitializeContext* context,
    size_t zero_operation_budget, Nba97GameMatchInitializeProgress* progress,
    Nba97GameMatchInitializeAdapterProgress* adapter_progress) {
    if (!context || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->memory_zero_result = NBA97_TEXT_COMPLETE;
    Nba97GameMatchInitializeContext composed = *context;
    AdapterRun run{context->io, context->user, zero_operation_budget,
        adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_match_initialize(&composed, progress);
}

int nba97_game_match_initialize_registers_from_session(
    const Nba97GameMatchSessionEvent* event,
    Nba97GameMatchInitializeRegisters* registers) {
    if (!event || !registers ||
        event->kind != NBA97_GAME_MATCH_SESSION_INITIALIZE ||
        event->pc != UINT32_C(0x8002da7c) ||
        event->entry != UINT32_C(0x8002db90))
        return NBA97_TEXT_ARGUMENT;
    std::memset(registers, 0, sizeof *registers);
    registers->gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {event->stack_pointer, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_GP] = {event->global_pointer, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_RA] = {event->return_address, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {
        event->saved_register[0], 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {
        event->saved_register[1], 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = {
        event->saved_register[2], 0x0f};
    return NBA97_TEXT_COMPLETE;
}
