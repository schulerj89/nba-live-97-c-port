#include "game_rule_delay_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
int site_index(std::uint32_t pc) {
    switch (pc) {
    case UINT32_C(0x80067df4):
        return NBA97_GAME_RULE_DELAY_FIRST_VIOLATION;
    case UINT32_C(0x80067ef0):
        return NBA97_GAME_RULE_DELAY_PHASE_82_VIOLATION;
    case UINT32_C(0x80067fdc):
        return NBA97_GAME_RULE_DELAY_FINAL_VIOLATION;
    default:
        return -1;
    }
}

bool is_rule_event(const Nba97GameClockViolationsEvent& event,
    const Nba97GameClockViolationsMachine& machine) {
    const auto a0 = machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0];
    const auto ra = machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
    return event.kind == NBA97_GAME_CLOCK_VIOLATIONS_CHILD_800295C8 &&
        event.entry == UINT32_C(0x800295c8) &&
        site_index(event.pc) >= 0 && event.delay_slot_pc == event.pc + 4u &&
        event.argument_count == 1 && event.operation != 0 &&
        event.invocation != 0 && a0.known_mask == 0x0fu &&
        (a0.word == 5000u || a0.word == 20000u) &&
        ra.known_mask == 0x0fu && ra.word == event.pc + 8u;
}

struct AdapterRun {
    Nba97GameClockViolationsIo unresolved_io;
    void* unresolved_user;
    Nba97GameRuleDelayAdapterProgress* out;
};

int dispatch(void* opaque, const Nba97GameTextMemory* memory,
    const Nba97GameClockViolationsEvent* event,
    Nba97GameClockViolationsMachine* machine) {
    auto& run = *static_cast<AdapterRun*>(opaque);
    if (!event || event->kind !=
            NBA97_GAME_CLOCK_VIOLATIONS_CHILD_800295C8) {
        if (!run.unresolved_io)
            return 0;
        const int accepted = run.unresolved_io(run.unresolved_user, memory,
            event, machine);
        if (accepted == 1)
            ++run.out->unresolved_callbacks_completed;
        return accepted;
    }

    Nba97GameRuleDelayProgress progress{};
    const int result = nba97_game_rule_delay_from_clock_violations(event,
        machine, &progress);
    ++run.out->invocations;
    run.out->rule_result = result;
    const int site = site_index(event->pc);
    if (site >= 0) {
        ++run.out->site_invocations[site];
        run.out->event[site] = *event;
        run.out->rule[site] = progress;
    }
    if (result == NBA97_TEXT_COMPLETE) {
        const auto duration = machine->registers.gpr[
            NBA97_MATCH_INITIALIZE_A0].word;
        if (duration == 5000u)
            ++run.out->duration_5000_invocations;
        else if (duration == 20000u)
            ++run.out->duration_20000_invocations;
    }
    return result == NBA97_TEXT_COMPLETE;
}
}

int nba97_game_rule_delay_from_clock_violations(
    const Nba97GameClockViolationsEvent* event,
    Nba97GameClockViolationsMachine* machine,
    Nba97GameRuleDelayProgress* progress) {
    if (!event || !machine || !progress || !is_rule_event(*event, *machine)) {
        if (progress)
            std::memset(progress, 0, sizeof *progress);
        return NBA97_TEXT_ARGUMENT;
    }
    Nba97GameRuleDelayContext context{};
    context.machine = *machine;
    const int result = nba97_game_rule_delay(&context, progress);
    if (result != NBA97_TEXT_ARGUMENT)
        *machine = progress->machine;
    return result;
}

int nba97_game_clock_violations_with_rule_delay(
    const Nba97GameClockViolationsContext* violations,
    Nba97GameClockViolationsProgress* violations_progress,
    Nba97GameRuleDelayAdapterProgress* adapter_progress) {
    if (!violations || !violations_progress || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->rule_result = NBA97_TEXT_COMPLETE;
    Nba97GameClockViolationsContext composed = *violations;
    AdapterRun run{violations->io, violations->user, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_clock_violations(&composed, violations_progress);
}
