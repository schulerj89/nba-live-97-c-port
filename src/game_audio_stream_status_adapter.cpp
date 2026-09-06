#include "game_audio_stream_status_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameAudioStreamPumpIo unresolved_io;
    void* unresolved_user;
    const Nba97GameAudioStreamStatusContext* status;
    Nba97GameAudioStreamStatusAdapterProgress* out;
};

bool isStatusEvent(const Nba97GameAudioStreamPumpEvent* event) {
    return event &&
        event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_8008472C &&
        event->pc == UINT32_C(0x80083f00) &&
        event->delay_slot_pc == UINT32_C(0x80083f04) &&
        event->entry == UINT32_C(0x8008472c) &&
        event->argument_count == 0;
}

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameAudioStreamPumpEvent* event,
    Nba97GameAudioStreamPumpRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (isStatusEvent(event))
        return nba97_game_audio_stream_status_from_stream_pump(memory, event,
            registers, run.status, run.out) == NBA97_TEXT_COMPLETE;
    if (!run.unresolved_io)
        return 0;
    const int accepted = run.unresolved_io(run.unresolved_user, memory, event,
        registers);
    if (accepted == 1)
        ++run.out->unresolved_callbacks_completed;
    return accepted;
}
}

int nba97_game_audio_stream_status_from_stream_pump(
    const Nba97GameTextMemory* memory,
    const Nba97GameAudioStreamPumpEvent* event,
    Nba97GameAudioStreamPumpRegisters* registers,
    const Nba97GameAudioStreamStatusContext* status,
    Nba97GameAudioStreamStatusAdapterProgress* out) {
    if (!memory || !isStatusEvent(event) || !registers || !status || !out)
        return NBA97_TEXT_ARGUMENT;
    ++out->status_invocations;
    out->status_event = *event;
    const Nba97GameAudioStreamPumpRegisters entry_registers = *registers;
    Nba97GameAudioStreamStatusContext context = *status;
    context.memory = *memory;
    context.registers = *registers;
    out->status_result = nba97_game_audio_stream_status(&context, &out->status);
    /* A validation failure has no source prefix and leaves progress registers
     * zero-initialized. Preserve the parent's JAL-entry state in that case;
     * every started execution publishes a valid hard-wired zero register. */
    if (out->status.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask ==
        0x0fu)
        *registers = out->status.registers;
    else
        *registers = entry_registers;
    if (out->status_result == NBA97_TEXT_COMPLETE)
        ++out->status_completions;
    return out->status_result;
}

int nba97_game_audio_stream_pump_with_stream_status(
    const Nba97GameAudioStreamPumpContext* pump,
    const Nba97GameAudioStreamStatusContext* status,
    Nba97GameAudioStreamPumpProgress* pump_progress,
    Nba97GameAudioStreamStatusAdapterProgress* adapter_progress) {
    if (!pump || !status || !pump_progress || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->status_result = NBA97_TEXT_COMPLETE;
    Nba97GameAudioStreamPumpContext composed = *pump;
    AdapterRun run{pump->io, pump->user, status, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_audio_stream_pump(&composed, pump_progress);
}
