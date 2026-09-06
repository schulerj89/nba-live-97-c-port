#include "game_clear_ordering_table_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97PlayerFrameAccess access;
    Nba97MatchFrameIo io;
    void* user;
    Nba97GameClearOrderingTableMatchFrameBinding* binding;
};

int callIndex(const Nba97MatchFrameCall* call) {
    if (call == nullptr)
        return -1;
    switch (call->pc) {
    case UINT32_C(0x80049084):
        return NBA97_GAME_CLEAR_ORDERING_TABLE_CALL_49084;
    case UINT32_C(0x80049094):
        return NBA97_GAME_CLEAR_ORDERING_TABLE_CALL_49094;
    default:
        return -1;
    }
}

bool targetsOwner(const Nba97MatchFrameCall* call) {
    return call != nullptr &&
        (call->entry == UINT32_C(0x80099960) || callIndex(call) >= 0);
}

bool validMachine(const Nba97GameClearOrderingTableMachine& machine) {
    if (machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0u ||
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask !=
            0x0fu ||
        machine.hi.known_mask > 0x0fu || machine.lo.known_mask > 0x0fu)
        return false;
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (machine.registers.gpr[i].known_mask > 0x0fu)
            return false;
    return true;
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
    case NBA97_TEXT_IO_REFUSED:
        return NBA97_GAME_CLEAR_ORDERING_TABLE_CHILD_INCOMPLETE;
    default:
        return NBA97_BODY_ARGUMENT;
    }
}

void resetTelemetry(Nba97GameClearOrderingTableMatchFrameBinding* binding) {
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
    if (targetsOwner(call))
        return nba97_game_clear_ordering_table_from_match_frame(
            run.binding, call, value);
    if (run.io == nullptr)
        return NBA97_MATCH_FRAME_IO_REQUIRED;
    const int result = run.io(run.user, call, value);
    if (result == NBA97_BODY_OK)
        ++run.binding->fallback_callbacks_completed;
    return result;
}
}

int nba97_game_clear_ordering_table_from_match_frame(void* opaque,
    const Nba97MatchFrameCall* call, Nba97GamePeriodValue* value) {
    auto* binding =
        static_cast<Nba97GameClearOrderingTableMatchFrameBinding*>(opaque);
    if (binding == nullptr || call == nullptr)
        return NBA97_BODY_ARGUMENT;

    const int index = callIndex(call);
    if (index < 0 && call->entry != UINT32_C(0x80099960)) {
        if (binding->fallback == nullptr)
            return NBA97_MATCH_FRAME_IO_REQUIRED;
        const int result = binding->fallback(
            binding->fallback_user, call, value);
        if (result == NBA97_BODY_OK)
            ++binding->fallback_callbacks_completed;
        return result;
    }

    if (index < 0 || call->entry != UINT32_C(0x80099960) ||
        value == nullptr ||
        (index == NBA97_GAME_CLEAR_ORDERING_TABLE_CALL_49084 &&
            call->args[1] != 32u) ||
        (index == NBA97_GAME_CLEAR_ORDERING_TABLE_CALL_49094 &&
            call->args[1] != 4096u) ||
        (!binding->memory.region && binding->memory.count) ||
        (!binding->access_journal && binding->access_journal_capacity))
        return NBA97_GAME_CLEAR_ORDERING_TABLE_CHILD_INCOMPLETE;

    ++binding->invocations;
    ++binding->call_count[index];
    binding->event[index] = *call;
    binding->result[index] = NBA97_TEXT_ARGUMENT;
    std::memset(&binding->progress[index], 0,
        sizeof binding->progress[index]);

    Nba97GameClearOrderingTableMachine entry{};
    if (binding->entry_machine_provider == nullptr ||
        binding->entry_machine_provider(binding->entry_machine_user,
            call, binding->invocations, &entry) != 1)
        return NBA97_GAME_CLEAR_ORDERING_TABLE_ENTRY_MACHINE_REQUIRED;

    entry.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0u, 0x0fu};
    entry.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {call->args[0], 0x0fu};
    entry.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
        {call->args[1], 0x0fu};
    entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {call->pc + 8u, 0x0fu};
    if (!validMachine(entry))
        return NBA97_GAME_CLEAR_ORDERING_TABLE_CHILD_INCOMPLETE;

    Nba97GameClearOrderingTableContext context{};
    context.memory = binding->memory;
    context.operation_budget = binding->operation_budget;
    context.machine = entry;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    binding->result[index] = nba97_game_clear_ordering_table(
        &context, &binding->progress[index]);
    const int result = mapResult(binding->result[index]);
    if (result != NBA97_BODY_OK)
        return result;

    ++binding->completions;
    value->word = 0;
    value->known = 0;
    return NBA97_BODY_OK;
}

int nba97_game_match_frame_with_clear_ordering_table(
    const Nba97MatchFrameContext* frame,
    Nba97GameClearOrderingTableMatchFrameBinding* binding,
    Nba97MatchFrameProgress* progress) {
    if (frame == nullptr || binding == nullptr || progress == nullptr ||
        frame->access == nullptr)
        return NBA97_BODY_ARGUMENT;
    resetTelemetry(binding);
    AdapterRun run{frame->access, frame->io, frame->user, binding};
    Nba97MatchFrameContext composed = *frame;
    composed.access = accessDispatch;
    composed.io = ioDispatch;
    composed.user = &run;
    return nba97_game_match_frame(&composed, progress);
}
