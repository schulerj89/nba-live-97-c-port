#include "game_clock_read_adapter.h"

#include <cstring>

namespace {
bool is_clock_event(const Nba97GameSpeechStartupEvent& event) {
    return event.kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810 &&
        event.entry == UINT32_C(0x800a5810) &&
        event.delay_slot_pc == event.pc + 4u &&
        (event.pc == UINT32_C(0x800801ec) ||
            event.pc == UINT32_C(0x80080208)) &&
        event.argument_count == 0;
}

struct AdapterRun {
    Nba97GameSpeechStartupIo unresolved_io;
    void* unresolved_user;
    const Nba97GameClockReadAdapterContext* clock;
    Nba97GameClockReadAdapterProgress* out;
};

int dispatch(void* opaque, const Nba97GameTextMemory* memory,
    const Nba97GameSpeechStartupEvent* event,
    Nba97GameSpeechStartupRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(opaque);
    if (!event || event->kind != NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810) {
        if (!run.unresolved_io)
            return 0;
        const int accepted = run.unresolved_io(run.unresolved_user, memory,
            event, registers);
        if (accepted == 1)
            ++run.out->unresolved_callbacks_completed;
        return accepted;
    }

    Nba97GameClockReadAdapterContext invocation = *run.clock;
    if (run.out->clock_access_events < invocation.access_journal_capacity) {
        invocation.access_journal += run.out->clock_access_events;
        invocation.access_journal_capacity -= run.out->clock_access_events;
    } else {
        invocation.access_journal = nullptr;
        invocation.access_journal_capacity = 0;
    }
    Nba97GameClockReadProgress progress{};
    const int result = nba97_game_clock_read_from_speech_startup(memory,
        event, registers, &invocation, &progress);
    ++run.out->invocations;
    run.out->clock_access_events += progress.access_events;
    run.out->clock_result = result;
    if (event->pc == UINT32_C(0x800801ec)) {
        ++run.out->initial_invocations;
        run.out->initial_event = *event;
        run.out->initial_clock = progress;
    } else {
        ++run.out->poll_invocations;
        run.out->poll_event = *event;
        run.out->poll_clock = progress;
    }
    return result == NBA97_TEXT_COMPLETE;
}
}

int nba97_game_clock_read_from_speech_startup(
    const Nba97GameTextMemory* memory,
    const Nba97GameSpeechStartupEvent* event,
    Nba97GameSpeechStartupRegisters* registers,
    const Nba97GameClockReadAdapterContext* adapter,
    Nba97GameClockReadProgress* progress) {
    if (!memory || !event || !registers || !adapter || !progress ||
        !is_clock_event(*event) ||
        (!adapter->access_journal && adapter->access_journal_capacity))
        return NBA97_TEXT_ARGUMENT;
    Nba97GameClockReadContext context{};
    context.memory = *memory;
    context.operation_budget = adapter->operation_budget;
    context.machine.registers = *registers;
    context.machine.hi = {0, 0};
    context.machine.lo = {0, 0};
    context.access_journal = adapter->access_journal;
    context.access_journal_capacity = adapter->access_journal_capacity;
    const int result = nba97_game_clock_read(&context, progress);
    /* Validation failures have no executed prefix. Runtime metadata failures
     * stop at the LW and therefore do return the observable LUI register file. */
    if (result != NBA97_TEXT_ARGUMENT || progress->stopped_pc != 0)
        *registers = progress->machine.registers;
    return result;
}

int nba97_game_speech_startup_with_clock_read(
    const Nba97GameSpeechStartupContext* speech,
    const Nba97GameClockReadAdapterContext* clock,
    Nba97GameSpeechStartupProgress* speech_progress,
    Nba97GameClockReadAdapterProgress* adapter_progress) {
    if (!speech || !clock || !speech_progress || !adapter_progress ||
        (!clock->access_journal && clock->access_journal_capacity))
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->clock_result = NBA97_TEXT_COMPLETE;
    Nba97GameSpeechStartupContext composed = *speech;
    AdapterRun run{speech->io, speech->user, clock, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_speech_startup(&composed, speech_progress);
}
