#include "game_match_audio_service_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameMatchAudioServiceIo unresolved_io;
    void* unresolved_user;
    const Nba97GameClockReadContext* clock;
    const Nba97GameAudioStreamStatusContext* status;
    Nba97GameMatchAudioServiceAdapterProgress* out;
};

bool isClockReadEvent(const Nba97GameMatchAudioServiceEvent* event) {
    return event &&
        event->kind == NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800A5810 &&
        event->pc == UINT32_C(0x8002a270) &&
        event->delay_slot_pc == UINT32_C(0x8002a274) &&
        event->entry == UINT32_C(0x800a5810) &&
        event->argument_count == 0;
}

bool isStreamStatusEvent(const Nba97GameMatchAudioServiceEvent* event) {
    return event &&
        event->kind == NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008472C &&
        event->pc == UINT32_C(0x8002a2dc) &&
        event->delay_slot_pc == UINT32_C(0x8002a2e0) &&
        event->entry == UINT32_C(0x8008472c) &&
        event->argument_count == 0;
}

bool isCallerEvent(const Nba97GameMatchAudioServiceCallerEvent* event) {
    return event && event->pc == UINT32_C(0x8002de5c) &&
        event->delay_slot_pc == UINT32_C(0x8002de60) &&
        event->entry == UINT32_C(0x8002a264) && event->argument_count == 0;
}

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameMatchAudioServiceEvent* event,
    Nba97GameMatchAudioServiceMachine* machine) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (isClockReadEvent(event)) {
        ++run.out->clock_read_invocations;
        run.out->clock_read_event = *event;
        const auto entry_machine = *machine;
        Nba97GameClockReadContext context = *run.clock;
        context.memory = *memory;
        context.machine = *machine;
        run.out->clock_read_result = nba97_game_clock_read(&context,
            &run.out->clock_read);
        if (run.out->clock_read.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_ZERO].known_mask == 0x0fu)
            *machine = run.out->clock_read.machine;
        else
            *machine = entry_machine;
        if (run.out->clock_read_result == NBA97_TEXT_COMPLETE)
            ++run.out->clock_read_completions;
        return run.out->clock_read_result == NBA97_TEXT_COMPLETE;
    }
    if (isStreamStatusEvent(event)) {
        ++run.out->stream_status_invocations;
        run.out->stream_status_event = *event;
        const auto entry_registers = machine->registers;
        Nba97GameAudioStreamStatusContext context = *run.status;
        context.memory = *memory;
        context.registers = machine->registers;
        run.out->stream_status_result = nba97_game_audio_stream_status(
            &context, &run.out->stream_status);
        /* X has no HI/LO instructions or callbacks. Copy only its exposed
         * GPR state and leave the full-machine HI/LO pair byte-for-byte live. */
        if (run.out->stream_status.registers.gpr[
                NBA97_MATCH_INITIALIZE_ZERO].known_mask == 0x0fu)
            machine->registers = run.out->stream_status.registers;
        else
            machine->registers = entry_registers;
        if (run.out->stream_status_result == NBA97_TEXT_COMPLETE)
            ++run.out->stream_status_completions;
        return run.out->stream_status_result == NBA97_TEXT_COMPLETE;
    }
    if (!run.unresolved_io)
        return 0;
    const int accepted = run.unresolved_io(run.unresolved_user, memory, event,
        machine);
    if (accepted == 1)
        ++run.out->unresolved_callbacks_completed;
    return accepted;
}
}

int nba97_game_match_audio_service_with_stream_status(
    const Nba97GameMatchAudioServiceContext* service,
    const Nba97GameClockReadContext* clock,
    const Nba97GameAudioStreamStatusContext* status,
    Nba97GameMatchAudioServiceProgress* progress,
    Nba97GameMatchAudioServiceAdapterProgress* adapter_progress) {
    if (!service || !clock || !status || !progress || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->clock_read_result = NBA97_TEXT_COMPLETE;
    adapter_progress->stream_status_result = NBA97_TEXT_COMPLETE;
    adapter_progress->service_result = NBA97_TEXT_COMPLETE;
    Nba97GameMatchAudioServiceContext composed = *service;
    AdapterRun run{service->io, service->user, clock, status,
        adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    adapter_progress->service_result = nba97_game_match_audio_service(
        &composed, progress);
    return adapter_progress->service_result;
}

int nba97_game_match_audio_service_from_8002de5c(
    const Nba97GameTextMemory* memory,
    const Nba97GameMatchAudioServiceCallerEvent* event,
    Nba97GameMatchAudioServiceMachine* machine,
    const Nba97GameMatchAudioServiceContext* service,
    const Nba97GameClockReadContext* clock,
    const Nba97GameAudioStreamStatusContext* status,
    Nba97GameMatchAudioServiceProgress* progress,
    Nba97GameMatchAudioServiceAdapterProgress* adapter_progress) {
    if (!memory || !isCallerEvent(event) || !machine || !service || !clock ||
        !status || !progress || !adapter_progress ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask !=
            0x0fu ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
            UINT32_C(0x8002de64))
        return NBA97_TEXT_ARGUMENT;
    Nba97GameMatchAudioServiceContext context = *service;
    context.memory = *memory;
    context.machine = *machine;
    const int result = nba97_game_match_audio_service_with_stream_status(
        &context, clock, status, progress, adapter_progress);
    ++adapter_progress->caller_invocations;
    adapter_progress->caller_event = *event;
    if (progress->machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO]
            .known_mask == 0x0fu)
        *machine = progress->machine;
    if (result == NBA97_TEXT_COMPLETE)
        ++adapter_progress->caller_completions;
    return result;
}

int nba97_game_match_audio_service_from_match_service_publish(
    const Nba97GameTextMemory* memory,
    const Nba97GameMatchServicePublishEvent* event,
    Nba97GameMatchServicePublishMachine* machine,
    const Nba97GameMatchAudioServiceContext* service,
    const Nba97GameClockReadContext* clock,
    const Nba97GameAudioStreamStatusContext* status,
    Nba97GameMatchAudioServiceProgress* progress,
    Nba97GameMatchAudioServiceAdapterProgress* adapter_progress) {
    if (!event ||
        event->kind != NBA97_GAME_MATCH_SERVICE_PUBLISH_CHILD_8002A264 ||
        event->operation == 0 || event->invocation == 0)
        return NBA97_TEXT_ARGUMENT;
    const Nba97GameMatchAudioServiceCallerEvent caller{
        event->pc, event->delay_slot_pc, event->entry,
        event->argument_count};
    return nba97_game_match_audio_service_from_8002de5c(memory, &caller,
        machine, service, clock, status, progress, adapter_progress);
}
