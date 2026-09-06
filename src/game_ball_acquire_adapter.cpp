#include "game_ball_acquire_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
int site_index(std::uint32_t pc) {
    switch (pc) {
    case UINT32_C(0x8005d498): return NBA97_GAME_BALL_ACQUIRE_CHANGE_SHORT_DELAY;
    case UINT32_C(0x8005d4a8): return NBA97_GAME_BALL_ACQUIRE_CHANGE_LONG_DELAY;
    case UINT32_C(0x8005d898): return NBA97_GAME_BALL_ACQUIRE_SAME_SHORT_DELAY;
    case UINT32_C(0x8005d8a8): return NBA97_GAME_BALL_ACQUIRE_SAME_LONG_DELAY;
    default: return -1;
    }
}

bool valid_delay(const Nba97GameBallAcquireEvent& event,
    const Nba97GameBallAcquireMachine& machine) {
    const auto a0 = machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0];
    const auto ra = machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    const int site = site_index(event.pc);
    if (site < 0 || event.kind != NBA97_GAME_BALL_ACQUIRE_CHILD_800295C8 ||
        event.entry != UINT32_C(0x800295c8) ||
        event.delay_slot_pc != event.pc + 4u || event.argument_count != 1 ||
        event.operation == 0 || event.invocation == 0 ||
        a0.known_mask != 0x0fu || ra.known_mask != 0x0fu ||
        ra.word != event.pc + 8u)
        return false;
    if (site == NBA97_GAME_BALL_ACQUIRE_CHANGE_LONG_DELAY ||
        site == NBA97_GAME_BALL_ACQUIRE_SAME_LONG_DELAY)
        return a0.word == 20000u;
    return a0.word == 10000u;
}

struct AdapterRun {
    Nba97GameBallAcquireIo unresolved;
    void* unresolved_user;
    Nba97GameBallAcquireAdapterProgress* out;
};

struct NaturalRun {
    Nba97GameBallActorContactIo unresolved;
    void* unresolved_user;
    Nba97GameBallAcquireNaturalProgress* out;
};

int natural_dispatch(void* opaque, const Nba97GameTextMemory* memory,
    const Nba97GameBallActorContactEvent* event,
    Nba97GameBallActorContactMachine* machine) {
    auto& run = *static_cast<NaturalRun*>(opaque);
    if (!event || event->entry != UINT32_C(0x8005d140)) {
        if (!run.unresolved) return 0;
        const int accepted = run.unresolved(run.unresolved_user, memory,
            event, machine);
        if (accepted == 1)
            ++run.out->unresolved_contact_callbacks_completed;
        return accepted;
    }
    const auto a0 = machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0];
    const auto s1 = machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1];
    const auto ra = machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    if (run.out->acquisition_count != 0 ||
        event->pc != UINT32_C(0x8006089c) ||
        event->delay_slot_pc != UINT32_C(0x800608a0) ||
        event->kind != NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8005D140 ||
        event->argument_count != 1 || a0.word != s1.word ||
        a0.known_mask != s1.known_mask || ra.word != UINT32_C(0x800608a4) ||
        ra.known_mask != 0x0f) {
        run.out->acquisition_result = NBA97_TEXT_ARGUMENT;
        return 0;
    }
    run.out->acquisition_event = *event;
    ++run.out->acquisition_count;
    Nba97GameBallAcquireContext context{};
    context.memory = *memory;
    context.operation_budget = run.out->acquisition_operation_budget;
    context.machine = *machine;
    context.io = run.out->acquisition_io;
    context.user = run.out->acquisition_user;
    const auto before = *machine;
    run.out->acquisition_result = nba97_game_ball_acquire_with_rule_delay(
        &context, &run.out->acquisition, &run.out->acquisition_adapter);
    if (run.out->acquisition_result == NBA97_TEXT_ARGUMENT &&
        run.out->acquisition.operations == 0 &&
        run.out->acquisition.stopped_pc == 0)
        *machine = before;
    else
        *machine = run.out->acquisition.machine;
    return run.out->acquisition_result == NBA97_TEXT_COMPLETE;
}

int dispatch(void* opaque, const Nba97GameTextMemory* memory,
    const Nba97GameBallAcquireEvent* event,
    Nba97GameBallAcquireMachine* machine) {
    auto& run = *static_cast<AdapterRun*>(opaque);
    if (!event || event->kind != NBA97_GAME_BALL_ACQUIRE_CHILD_800295C8) {
        if (!run.unresolved)
            return 0;
        const int accepted = run.unresolved(run.unresolved_user, memory,
            event, machine);
        if (accepted == 1)
            ++run.out->unresolved_callbacks_completed;
        return accepted;
    }
    Nba97GameRuleDelayProgress progress{};
    const int result = nba97_game_ball_acquire_rule_delay(event, machine,
        &progress);
    ++run.out->delay_invocations;
    run.out->delay_result = result;
    const int site = site_index(event->pc);
    if (site >= 0) {
        ++run.out->delay_site_invocations[site];
        run.out->event[site] = *event;
        run.out->delay[site] = progress;
    }
    if (result == NBA97_TEXT_COMPLETE) {
        const auto duration = machine->registers.gpr[
            NBA97_MATCH_INITIALIZE_A0].word;
        if (duration == 10000u) ++run.out->duration_10000_invocations;
        if (duration == 20000u) ++run.out->duration_20000_invocations;
    }
    return result == NBA97_TEXT_COMPLETE;
}
}

int nba97_game_ball_acquire_rule_delay(
    const Nba97GameBallAcquireEvent* event,
    Nba97GameBallAcquireMachine* machine,
    Nba97GameRuleDelayProgress* progress) {
    if (!event || !machine || !progress || !valid_delay(*event, *machine)) {
        if (progress) std::memset(progress, 0, sizeof *progress);
        return NBA97_TEXT_ARGUMENT;
    }
    Nba97GameRuleDelayContext context{};
    context.machine = *machine;
    const int result = nba97_game_rule_delay(&context, progress);
    if (result != NBA97_TEXT_ARGUMENT)
        *machine = progress->machine;
    return result;
}

int nba97_game_ball_acquire_with_rule_delay(
    const Nba97GameBallAcquireContext* context,
    Nba97GameBallAcquireProgress* progress,
    Nba97GameBallAcquireAdapterProgress* adapter) {
    if (!context || !progress || !adapter)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter, 0, sizeof *adapter);
    adapter->delay_result = NBA97_TEXT_COMPLETE;
    Nba97GameBallAcquireContext composed = *context;
    AdapterRun run{context->io, context->user, adapter};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_ball_acquire(&composed, progress);
}

int nba97_game_ball_actor_contact_with_ball_acquire(
    Nba97GameBallActorContactContext* context,
    Nba97GameBallActorContactProgress* progress,
    Nba97GameBallActorContactBinding* binding,
    Nba97GameBallAcquireNaturalProgress* natural) {
    if (!context || !progress || !binding || !natural)
        return NBA97_TEXT_ARGUMENT;
    natural->acquisition_count = 0;
    natural->unresolved_contact_callbacks_completed = 0;
    natural->acquisition_result = NBA97_TEXT_COMPLETE;
    std::memset(&natural->acquisition_event, 0,
        sizeof natural->acquisition_event);
    std::memset(&natural->acquisition, 0, sizeof natural->acquisition);
    std::memset(&natural->acquisition_adapter, 0,
        sizeof natural->acquisition_adapter);
    NaturalRun run{context->io, context->user, natural};
    context->io = natural_dispatch;
    context->user = &run;
    const int result = nba97_game_ball_actor_contact_run(context, progress,
        binding);
    context->io = run.unresolved;
    context->user = run.unresolved_user;
    return result;
}
