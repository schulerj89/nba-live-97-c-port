#include "game_audio_stream_pump_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameSpeechStartupIo unresolved_io;
    void* unresolved_user;
    const Nba97GameAudioStreamPumpContext* pump;
    Nba97GameAudioStreamPumpAdapterProgress* out;
};

bool is_pump_event(const Nba97GameSpeechStartupEvent* event) {
    return event &&
        event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_80083EEC &&
        event->entry == UINT32_C(0x80083eec) &&
        event->delay_slot_pc == event->pc + 4u &&
        (event->pc == UINT32_C(0x800801e4) ||
            event->pc == UINT32_C(0x8008021c)) &&
        event->argument_count == 0;
}

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameSpeechStartupEvent* event,
    Nba97GameSpeechStartupRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (is_pump_event(event))
        return nba97_game_audio_stream_pump_from_speech_startup(memory, event,
            registers, run.pump, run.out) == NBA97_TEXT_COMPLETE;
    if (!run.unresolved_io)
        return 0;
    const int accepted = run.unresolved_io(run.unresolved_user, memory, event,
        registers);
    if (accepted == 1)
        ++run.out->unresolved_callbacks_completed;
    return accepted;
}
}

int nba97_game_audio_stream_pump_from_speech_startup(
    const Nba97GameTextMemory* memory,
    const Nba97GameSpeechStartupEvent* event,
    Nba97GameSpeechStartupRegisters* registers,
    const Nba97GameAudioStreamPumpContext* pump,
    Nba97GameAudioStreamPumpAdapterProgress* out) {
    if (!memory || !is_pump_event(event) || !registers || !pump || !out)
        return NBA97_TEXT_ARGUMENT;
    const size_t event_index = event->pc == UINT32_C(0x800801e4) ? 0u : 1u;
    ++out->pump_invocations;
    out->pump_event[event_index] = *event;
    Nba97GameAudioStreamPumpContext context = *pump;
    context.memory = *memory;
    context.registers = *registers;
    out->pump_result = nba97_game_audio_stream_pump(&context, &out->pump);
    *registers = out->pump.registers;
    if (out->pump_result == NBA97_TEXT_COMPLETE)
        ++out->pump_completions;
    return out->pump_result;
}

int nba97_game_speech_startup_with_audio_stream_pump(
    const Nba97GameSpeechStartupContext* speech,
    const Nba97GameAudioStreamPumpContext* pump,
    Nba97GameSpeechStartupProgress* progress,
    Nba97GameAudioStreamPumpAdapterProgress* adapter_progress) {
    if (!speech || !pump || !progress || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->pump_result = NBA97_TEXT_COMPLETE;
    Nba97GameSpeechStartupContext composed = *speech;
    AdapterRun run{speech->io, speech->user, pump, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_speech_startup(&composed, progress);
}

int nba97_game_audio_stream_pump_from_controller_reset(
    const Nba97GameTextMemory* memory, const Nba97GameControllerFrameResetEvent* event,
    Nba97GameControllerFrameResetRegisters* registers,
    const Nba97GameAudioStreamPumpContext* pump, Nba97GameAudioStreamPumpProgress* out) {
    if (!memory || !event || !registers || !pump || !out ||
        event->kind != NBA97_GAME_CONTROLLER_FRAME_RESET_83EEC ||
        event->pc != UINT32_C(0x8006764c) || event->delay_slot_pc != UINT32_C(0x80067650) ||
        event->entry != UINT32_C(0x80083eec) || event->argument_count != 0)
        return NBA97_TEXT_ARGUMENT;
    Nba97GameAudioStreamPumpContext context = *pump;
    context.memory = *memory;
    context.registers = *registers;
    const int result = nba97_game_audio_stream_pump(&context, out);
    *registers = out->registers;
    return result;
}
