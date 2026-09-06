#include "game_stream_readiness_adapter.h"

#include <cstring>

namespace {
bool isReadinessEvent(const Nba97GameMatchAudioServiceEvent* event) {
    return event &&
        event->kind == NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80088D0C &&
        event->pc == UINT32_C(0x8002a2ec) &&
        event->delay_slot_pc == UINT32_C(0x8002a2f0) &&
        event->entry == UINT32_C(0x80088d0c) &&
        event->argument_count == 0 && event->operation != 0 &&
        event->invocation != 0;
}

struct AdapterRun {
    Nba97GameMatchAudioServiceIo unresolved_io;
    void* unresolved_user;
    const Nba97GameStreamReadinessContext* readiness;
    Nba97GameStreamReadinessAdapterProgress* out;
};

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameMatchAudioServiceEvent* event,
    Nba97GameMatchAudioServiceMachine* machine) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (isReadinessEvent(event)) {
        ++run.out->readiness_invocations;
        run.out->readiness_event = *event;
        run.out->readiness_result =
            nba97_game_stream_readiness_from_match_audio_service(memory,
                event, machine, run.readiness, &run.out->readiness);
        if (run.out->readiness_result == NBA97_TEXT_COMPLETE)
            ++run.out->readiness_completions;
        return run.out->readiness_result == NBA97_TEXT_COMPLETE;
    }
    if (!run.unresolved_io)
        return 0;
    const int accepted = run.unresolved_io(run.unresolved_user, memory,
        event, machine);
    if (accepted == 1)
        ++run.out->unresolved_callbacks_completed;
    return accepted;
}
}

int nba97_game_stream_readiness_from_match_audio_service(
    const Nba97GameTextMemory* memory,
    const Nba97GameMatchAudioServiceEvent* event,
    Nba97GameMatchAudioServiceMachine* machine,
    const Nba97GameStreamReadinessContext* readiness,
    Nba97GameStreamReadinessProgress* progress) {
    if (!memory || !isReadinessEvent(event) || !machine || !readiness ||
        !progress ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask !=
            0x0fu ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
            UINT32_C(0x8002a2f4))
        return NBA97_TEXT_ARGUMENT;
    const auto entry_machine = *machine;
    Nba97GameStreamReadinessContext context = *readiness;
    context.memory = *memory;
    context.machine = *machine;
    const int result = nba97_game_stream_readiness(&context, progress);
    /* A rejected initial machine has no source prefix. Any started run has
     * published the hard-wired zero register and returns its exact prefix. */
    if (progress->machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO]
            .known_mask == 0x0fu)
        *machine = progress->machine;
    else
        *machine = entry_machine;
    return result;
}

int nba97_game_match_audio_service_with_stream_readiness(
    const Nba97GameMatchAudioServiceContext* service,
    const Nba97GameClockReadContext* clock,
    const Nba97GameAudioStreamStatusContext* status,
    const Nba97GameStreamReadinessContext* readiness,
    Nba97GameMatchAudioServiceProgress* service_progress,
    Nba97GameStreamReadinessAdapterProgress* adapter_progress) {
    if (!service || !clock || !status || !readiness || !service_progress ||
        !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->readiness_result = NBA97_TEXT_COMPLETE;
    adapter_progress->audio_service_result = NBA97_TEXT_COMPLETE;
    Nba97GameMatchAudioServiceContext composed = *service;
    AdapterRun run{service->io, service->user, readiness, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    adapter_progress->audio_service_result =
        nba97_game_match_audio_service_with_stream_status(&composed, clock,
            status, service_progress, &adapter_progress->audio_service);
    return adapter_progress->audio_service_result;
}
