#include "game_stream_queue_count_adapter.h"

#include <cstring>

namespace {
bool isQueueEvent(const Nba97GameStreamReadinessEvent* event) {
    return event &&
        event->kind == NBA97_GAME_STREAM_READINESS_CHILD_80084448 &&
        event->pc == UINT32_C(0x80088d30) &&
        event->delay_slot_pc == UINT32_C(0x80088d34) &&
        event->entry == UINT32_C(0x80084448) &&
        event->argument_count == 0 && event->operation != 0 &&
        event->invocation != 0;
}

struct AdapterRun {
    Nba97GameStreamReadinessIo unresolved_io;
    void* unresolved_user;
    const Nba97GameStreamQueueCountContext* queue;
    Nba97GameStreamQueueCountAdapterProgress* out;
};

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameStreamReadinessEvent* event,
    Nba97GameStreamReadinessMachine* machine) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (isQueueEvent(event)) {
        ++run.out->queue_invocations;
        run.out->queue_event = *event;
        run.out->queue_result =
            nba97_game_stream_queue_count_from_stream_readiness(memory,
                event, machine, run.queue, &run.out->queue_count);
        if (run.out->queue_result == NBA97_TEXT_COMPLETE)
            ++run.out->queue_completions;
        return run.out->queue_result == NBA97_TEXT_COMPLETE;
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

int nba97_game_stream_queue_count_from_stream_readiness(
    const Nba97GameTextMemory* memory,
    const Nba97GameStreamReadinessEvent* event,
    Nba97GameStreamReadinessMachine* machine,
    const Nba97GameStreamQueueCountContext* queue,
    Nba97GameStreamQueueCountProgress* progress) {
    if (!memory || !isQueueEvent(event) || !machine || !queue || !progress ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask !=
            0x0fu ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
            UINT32_C(0x80088d38))
        return NBA97_TEXT_ARGUMENT;
    const auto entry_machine = *machine;
    Nba97GameStreamQueueCountContext context = *queue;
    context.memory = *memory;
    context.machine = *machine;
    const int result = nba97_game_stream_queue_count(&context, progress);
    if (progress->machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO]
            .known_mask == 0x0fu)
        *machine = progress->machine;
    else
        *machine = entry_machine;
    return result;
}

int nba97_game_stream_readiness_with_queue_count(
    const Nba97GameStreamReadinessContext* readiness,
    const Nba97GameStreamQueueCountContext* queue,
    Nba97GameStreamReadinessProgress* readiness_progress,
    Nba97GameStreamQueueCountAdapterProgress* adapter_progress) {
    if (!readiness || !queue || !readiness_progress || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->queue_result = NBA97_TEXT_COMPLETE;
    adapter_progress->readiness_result = NBA97_TEXT_COMPLETE;
    Nba97GameStreamReadinessContext composed = *readiness;
    AdapterRun run{readiness->io, readiness->user, queue, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    adapter_progress->readiness_result = nba97_game_stream_readiness(
        &composed, readiness_progress);
    return adapter_progress->readiness_result;
}
