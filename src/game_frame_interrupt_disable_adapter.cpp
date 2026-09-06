#include "game_frame_interrupt_disable_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97PlayerFrameAccess access;
    Nba97MatchFrameIo io;
    void* user;
    Nba97GameFrameInterruptDisableBinding* binding;
};

int callIndex(const Nba97MatchFrameCall* call) {
    if (!call)
        return -1;
    switch (call->pc) {
    case UINT32_C(0x80049070):
        return NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_49070;
    case UINT32_C(0x800491c8):
        return NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_491C8;
    case UINT32_C(0x8004920c):
        return NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_4920C;
    case UINT32_C(0x8004927c):
        return NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_4927C;
    default:
        return -1;
    }
}

bool targetsDisable(const Nba97MatchFrameCall* call) {
    return call && (call->entry == UINT32_C(0x80048ff4) ||
        callIndex(call) >= 0);
}

int mapResult(int result) {
    switch (result) {
    case NBA97_TEXT_COMPLETE:
        return NBA97_BODY_OK;
    case NBA97_TEXT_ARGUMENT:
        return NBA97_BODY_ARGUMENT;
    case NBA97_TEXT_RESOURCE:
        return NBA97_BODY_BOUNDS;
    case NBA97_TEXT_UNKNOWN:
        return NBA97_BODY_UNKNOWN;
    case NBA97_TEXT_ALIGNMENT_TRAP:
        return NBA97_BODY_ALIGNMENT_TRAP;
    case NBA97_TEXT_LIMIT:
        return NBA97_BODY_JOURNAL_LIMIT;
    default:
        return NBA97_BODY_ARGUMENT;
    }
}

void resetTelemetry(Nba97GameFrameInterruptDisableBinding* binding) {
    binding->invocations = 0;
    binding->completions = 0;
    binding->fallback_callbacks_completed = 0;
    std::memset(binding->call_count, 0, sizeof binding->call_count);
    std::memset(binding->event, 0, sizeof binding->event);
    std::memset(binding->progress, 0, sizeof binding->progress);
    std::memset(binding->result, 0, sizeof binding->result);
}

int accessDispatch(void* opaque, uint32_t pc, uint32_t address,
    unsigned width, unsigned kind, Nba97PlayerFrameValue* value) {
    auto& run = *static_cast<AdapterRun*>(opaque);
    return run.access(run.user, pc, address, width, kind, value);
}

int ioDispatch(void* opaque, const Nba97MatchFrameCall* call,
    Nba97GamePeriodValue* value) {
    auto& run = *static_cast<AdapterRun*>(opaque);
    if (targetsDisable(call))
        return nba97_game_frame_interrupt_disable_from_match_frame(
            run.binding, call, value);
    if (!run.io)
        return NBA97_MATCH_FRAME_IO_REQUIRED;
    const int result = run.io(run.user, call, value);
    if (result == NBA97_BODY_OK)
        ++run.binding->fallback_callbacks_completed;
    return result;
}
}

int nba97_game_frame_interrupt_disable_from_match_frame(void* opaque,
    const Nba97MatchFrameCall* call, Nba97GamePeriodValue* value) {
    auto* binding =
        static_cast<Nba97GameFrameInterruptDisableBinding*>(opaque);
    const int index = callIndex(call);
    if (!binding || !call || !value || index < 0 ||
        call->entry != UINT32_C(0x80048ff4) || call->args[0] != 0 ||
        call->args[1] != 0 || binding->cp0_status.known_mask > 0x0fu ||
        (!binding->journal && binding->journal_capacity))
        return NBA97_BODY_ARGUMENT;

    Nba97GameFrameInterruptDisableMachine entry{};
    entry.cp0_status = binding->cp0_status;
    entry.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
    entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {call->pc + 8u, 0x0f};

    ++binding->invocations;
    ++binding->call_count[index];
    binding->event[index] = *call;
    Nba97GameFrameInterruptDisableContext context{};
    context.operation_budget = binding->operation_budget;
    context.machine = entry;
    context.journal = binding->journal;
    context.journal_capacity = binding->journal_capacity;
    binding->result[index] = nba97_game_frame_interrupt_disable(
        &context, &binding->progress[index]);

    const auto& progress = binding->progress[index];
    if (progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_ZERO].known_mask == 0x0fu)
        binding->cp0_status = progress.machine.cp0_status;
    if (binding->result[index] != NBA97_TEXT_COMPLETE)
        return mapResult(binding->result[index]);

    ++binding->completions;
    if (progress.old_status.known_mask == 0x0fu) {
        value->word = progress.old_status.word;
        value->known = 1;
    } else {
        value->word = 0;
        value->known = 0;
    }
    return NBA97_BODY_OK;
}

int nba97_game_match_frame_with_interrupt_disable(
    const Nba97MatchFrameContext* frame,
    Nba97GameFrameInterruptDisableBinding* binding,
    Nba97MatchFrameProgress* progress) {
    if (!frame || !binding || !progress || !frame->access)
        return NBA97_BODY_ARGUMENT;
    resetTelemetry(binding);
    AdapterRun run{frame->access, frame->io, frame->user, binding};
    Nba97MatchFrameContext composed = *frame;
    composed.access = accessDispatch;
    composed.io = ioDispatch;
    composed.user = &run;
    return nba97_game_match_frame(&composed, progress);
}
