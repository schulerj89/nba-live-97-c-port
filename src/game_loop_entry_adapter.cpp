#include "game_loop_entry_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct AdapterRun {
    const Nba97GameTextMemory* memory;
    const Nba97GameLoopEntryMatchTickServices* services;
    Nba97GameLoopEntryAdapterProgress* out;
};

int locate(AdapterRun& run, std::uint32_t address, unsigned width,
    std::uint8_t** data, std::uint8_t** known) {
    if (!width || width > 4)
        return NBA97_BODY_ARGUMENT;
    for (std::size_t i = 0; i < run.memory->count; ++i) {
        const auto& region = run.memory->region[i];
        const std::uint64_t offset =
            static_cast<std::uint64_t>(address) - region.base;
        if (address < region.base || offset > region.size ||
            width > region.size - static_cast<std::size_t>(offset))
            continue;
        *data = region.data + static_cast<std::size_t>(offset);
        *known = region.known ?
            region.known + static_cast<std::size_t>(offset) : nullptr;
        if (*known)
            for (unsigned j = 0; j < width; ++j)
                if ((*known)[j] > 1)
                    return NBA97_BODY_ARGUMENT;
        return NBA97_BODY_OK;
    }
    return NBA97_BODY_BOUNDS;
}

int retainedAccess(void* user, std::uint32_t, std::uint32_t address,
    unsigned width, unsigned kind, Nba97PlayerFrameValue* value) {
    auto& run = *static_cast<AdapterRun*>(user);
    std::uint8_t* data = nullptr;
    std::uint8_t* known = nullptr;
    if (!value || kind > NBA97_FRAME_WRITE_POINTER)
        return NBA97_BODY_ARGUMENT;
    const int status = locate(run, address, width, &data, &known);
    if (status != NBA97_BODY_OK)
        return status;
    if (kind == NBA97_FRAME_READ) {
        std::memset(value, 0, sizeof *value);
        for (unsigned i = 0; i < width; ++i) {
            if (!known || known[i]) {
                value->word |= std::uint32_t(data[i]) << (i * 8u);
                value->known_mask = static_cast<std::uint8_t>(
                    value->known_mask | (1u << i));
            }
        }
        return NBA97_BODY_OK;
    }
    if (value->is_reference)
        return NBA97_BODY_REFERENCE_REQUIRED;
    const std::uint8_t full_mask =
        static_cast<std::uint8_t>((1u << width) - 1u);
    if (!known && value->known_mask != full_mask)
        return NBA97_BODY_ARGUMENT;
    for (unsigned i = 0; i < width; ++i) {
        data[i] = static_cast<std::uint8_t>(value->word >> (i * 8u));
        if (known)
            known[i] = static_cast<std::uint8_t>(
                (value->known_mask >> i) & 1u);
    }
    return NBA97_BODY_OK;
}

int service(void* user, const Nba97MatchTickCall* call,
    Nba97GamePeriodValue* value) {
    auto& run = *static_cast<AdapterRun*>(user);
    return run.services->service(run.services->user, call, value);
}

int playerUpdate(void* user, std::uint32_t pc) {
    auto& run = *static_cast<AdapterRun*>(user);
    return run.services->player_update(run.services->user, pc);
}

int ballSimulation(void* user, std::uint32_t pc, std::uint32_t pointer) {
    auto& run = *static_cast<AdapterRun*>(user);
    return run.services->ball_simulation(run.services->user, pc, pointer);
}

int netTransform(void* user, std::uint32_t pc) {
    auto& run = *static_cast<AdapterRun*>(user);
    return run.services->net_transform(run.services->user, pc);
}

int matchFrame(void* user, std::uint32_t pc) {
    auto& run = *static_cast<AdapterRun*>(user);
    return run.services->match_frame(run.services->user, pc);
}

void makeUnknown(Nba97GameMatchInitializeRegisters& registers) {
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        registers.gpr[i] = {0, 0};
}

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameLoopEntryEvent* event,
    Nba97GameMatchInitializeRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (!memory || !event || !registers ||
        event->kind != NBA97_GAME_LOOP_ENTRY_MATCH_TICK ||
        event->pc != UINT32_C(0x8002dc40) ||
        event->delay_slot_pc != UINT32_C(0x8002dc44) ||
        event->entry != UINT32_C(0x80068bf8)) {
        run.out->match_tick_result = NBA97_BODY_ARGUMENT;
        return 0;
    }
    run.memory = memory;
    ++run.out->match_tick_invocations;
    Nba97MatchTickContext tick{};
    tick.access = retainedAccess;
    tick.service = run.services->service ? service : nullptr;
    tick.player_update = run.services->player_update ? playerUpdate : nullptr;
    tick.ball_simulation = run.services->ball_simulation ?
        ballSimulation : nullptr;
    tick.net_transform = run.services->net_transform ? netTransform : nullptr;
    tick.match_frame = run.services->match_frame ? matchFrame : nullptr;
    tick.user = &run;
    tick.operation_budget = run.services->operation_budget;
    const auto& s6 = registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 6];
    tick.incoming_s6 = (s6.known_mask & 0x03u) == 0x03u ?
        Nba97GamePeriodValue{s6.word & UINT32_C(0x0000ffff), 1} :
        Nba97GamePeriodValue{0, 0};
    run.out->match_tick_result = nba97_game_match_tick(&tick,
        &run.out->match_tick);
    /* The existing tick owner exposes its ordered memory/call prefix but no
       live output GPRs or guest stack. Even a complete tick cannot prove the
       wrapper's following LW address, so do not infer o32 preservation. */
    makeUnknown(*registers);
    run.out->output_registers_available = 0;
    return 0;
}
}

int nba97_game_loop_entry_with_match_tick(
    const Nba97GameLoopEntryContext* context,
    const Nba97GameLoopEntryMatchTickServices* services,
    Nba97GameLoopEntryProgress* progress,
    Nba97GameLoopEntryAdapterProgress* adapter_progress) {
    if (!context || !services || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->match_tick_result = NBA97_BODY_OK;
    Nba97GameLoopEntryContext composed = *context;
    AdapterRun run{nullptr, services, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_loop_entry(&composed, progress);
}

int nba97_game_loop_entry_registers_from_session(
    const Nba97GameMatchSessionEvent* event,
    Nba97GameMatchInitializeRegisters* registers) {
    if (!event || !registers ||
        event->kind != NBA97_GAME_MATCH_SESSION_RUN_LOOP ||
        event->pc != UINT32_C(0x8002da8c) ||
        event->entry != UINT32_C(0x8002dc38))
        return NBA97_TEXT_ARGUMENT;
    std::memset(registers, 0, sizeof *registers);
    registers->gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
        event->stack_pointer, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_GP] = {
        event->global_pointer, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_RA] = {
        event->return_address, 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {
        event->saved_register[0], 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {
        event->saved_register[1], 0x0f};
    registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = {
        event->saved_register[2], 0x0f};
    return NBA97_TEXT_COMPLETE;
}
