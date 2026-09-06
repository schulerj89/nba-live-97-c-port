#include "game_audio_stream_service_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameAudioStreamPumpIo unresolved_io;
    void* unresolved_user;
    const Nba97GameAudioStreamStatusContext* status;
    const Nba97GameAudioStreamServiceContext* service;
    Nba97GameAudioStreamStatusAdapterProgress* status_out;
    Nba97GameAudioStreamServiceAdapterProgress* service_out;
};

bool isServiceEvent(const Nba97GameAudioStreamPumpEvent* event) {
    return event &&
        event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80086190 &&
        (event->pc == UINT32_C(0x80083f78) ||
            event->pc == UINT32_C(0x80084034)) &&
        event->delay_slot_pc == event->pc + 4u &&
        event->entry == UINT32_C(0x80086190) &&
        event->argument_count == 0;
}

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
            registers, run.status, run.status_out) == NBA97_TEXT_COMPLETE;
    if (isServiceEvent(event))
        return nba97_game_audio_stream_service_from_stream_pump(memory, event,
            registers, run.service, run.service_out) == NBA97_TEXT_COMPLETE;
    if (!run.unresolved_io)
        return 0;
    const int accepted = run.unresolved_io(run.unresolved_user, memory, event,
        registers);
    if (accepted == 1)
        ++run.service_out->unresolved_callbacks_completed;
    return accepted;
}
}

int nba97_game_audio_stream_service_from_stream_pump(
    const Nba97GameTextMemory* memory,
    const Nba97GameAudioStreamPumpEvent* event,
    Nba97GameAudioStreamPumpRegisters* registers,
    const Nba97GameAudioStreamServiceContext* service,
    Nba97GameAudioStreamServiceAdapterProgress* out) {
    if (!memory || !isServiceEvent(event) || !registers || !service || !out)
        return NBA97_TEXT_ARGUMENT;
    ++out->service_invocations;
    out->service_event = *event;
    const Nba97GameAudioStreamPumpRegisters entry_registers = *registers;
    Nba97GameAudioStreamServiceContext context = *service;
    context.memory = *memory;
    context.registers = *registers;
    out->service_result = nba97_game_audio_stream_service(&context,
        &out->service);
    if (out->service.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask ==
        0x0fu)
        *registers = out->service.registers;
    else
        *registers = entry_registers;
    if (out->service_result == NBA97_TEXT_COMPLETE)
        ++out->service_completions;
    return out->service_result;
}

int nba97_game_audio_stream_pump_with_stream_status_and_service(
    const Nba97GameAudioStreamPumpContext* pump,
    const Nba97GameAudioStreamStatusContext* status,
    const Nba97GameAudioStreamServiceContext* service,
    Nba97GameAudioStreamPumpProgress* pump_progress,
    Nba97GameAudioStreamStatusAdapterProgress* status_progress,
    Nba97GameAudioStreamServiceAdapterProgress* service_progress) {
    if (!pump || !status || !service || !pump_progress || !status_progress ||
        !service_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(status_progress, 0, sizeof *status_progress);
    std::memset(service_progress, 0, sizeof *service_progress);
    status_progress->status_result = NBA97_TEXT_COMPLETE;
    service_progress->service_result = NBA97_TEXT_COMPLETE;
    Nba97GameAudioStreamPumpContext composed = *pump;
    AdapterRun run{pump->io, pump->user, status, service, status_progress,
        service_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_audio_stream_pump(&composed, pump_progress);
}
