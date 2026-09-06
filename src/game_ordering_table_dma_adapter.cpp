#include "game_ordering_table_dma_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameClearOrderingTableIo fallback;
    void* fallback_user;
    Nba97GameOrderingTableDmaBinding* binding;
};

bool targetsDma(const Nba97GameClearOrderingTableEvent* event) {
    return event != nullptr &&
        event->kind == NBA97_GAME_CLEAR_ORDERING_TABLE_BACKEND &&
        event->pc == UINT32_C(0x800999bc) &&
        event->delay_slot_pc == UINT32_C(0x800999c0) &&
        event->target == UINT32_C(0x8009a97c) &&
        event->argument_count == 2;
}

bool validMachine(const Nba97GameOrderingTableDmaMachine& machine) {
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

int ioDispatch(void* opaque, const Nba97GameTextMemory* memory,
    const Nba97GameClearOrderingTableEvent* event,
    Nba97GameClearOrderingTableMachine* machine) {
    auto& run = *static_cast<AdapterRun*>(opaque);
    if (targetsDma(event))
        return nba97_game_ordering_table_dma_from_clear_ordering_table(
            run.binding, memory, event, machine);
    if (run.fallback == nullptr)
        return 0;
    const int accepted = run.fallback(
        run.fallback_user, memory, event, machine);
    if (accepted == 1)
        ++run.binding->fallback_callbacks_completed;
    return accepted;
}
}

int nba97_game_ordering_table_dma_from_clear_ordering_table(void* opaque,
    const Nba97GameTextMemory* memory,
    const Nba97GameClearOrderingTableEvent* event,
    Nba97GameClearOrderingTableMachine* machine) {
    auto* binding = static_cast<Nba97GameOrderingTableDmaBinding*>(opaque);
    if (binding == nullptr || memory == nullptr || machine == nullptr ||
        !targetsDma(event) || (!memory->region && memory->count) ||
        (!binding->access_journal && binding->access_journal_capacity) ||
        (machine != nullptr && !validMachine(*machine)) ||
        (machine != nullptr &&
            (machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
                UINT32_C(0x800999c4) ||
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask !=
                0x0fu))) {
        if (binding != nullptr)
            binding->result = NBA97_TEXT_ARGUMENT;
        return 0;
    }

    ++binding->invocations;
    binding->event = *event;
    Nba97GameOrderingTableDmaContext context{};
    context.memory = *memory;
    context.operation_budget = binding->operation_budget;
    context.machine = *machine;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    binding->result = nba97_game_ordering_table_dma(
        &context, &binding->progress);
    const bool malformed_child = binding->result == NBA97_TEXT_ARGUMENT &&
        !validMachine(binding->progress.machine) &&
        (binding->progress.call_attempts[
             NBA97_GAME_ORDERING_TABLE_DMA_START] >
             binding->progress.call_count[
             NBA97_GAME_ORDERING_TABLE_DMA_START] ||
         binding->progress.call_attempts[
             NBA97_GAME_ORDERING_TABLE_DMA_WAIT] >
             binding->progress.call_count[
             NBA97_GAME_ORDERING_TABLE_DMA_WAIT]);
    if (binding->progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_ZERO].known_mask == 0x0fu ||
        malformed_child)
        *machine = binding->progress.machine;
    if (malformed_child)
        return 1;
    if (binding->result != NBA97_TEXT_COMPLETE)
        return 0;
    ++binding->completions;
    return 1;
}

int nba97_game_clear_ordering_table_with_dma(
    const Nba97GameClearOrderingTableContext* parent,
    Nba97GameOrderingTableDmaBinding* binding,
    Nba97GameClearOrderingTableProgress* progress) {
    if (parent == nullptr || binding == nullptr || progress == nullptr)
        return NBA97_TEXT_ARGUMENT;
    binding->invocations = 0;
    binding->completions = 0;
    binding->fallback_callbacks_completed = 0;
    binding->result = NBA97_TEXT_ARGUMENT;
    std::memset(&binding->event, 0, sizeof binding->event);
    std::memset(&binding->progress, 0, sizeof binding->progress);
    AdapterRun run{parent->io, parent->user, binding};
    Nba97GameClearOrderingTableContext composed = *parent;
    composed.io = ioDispatch;
    composed.user = &run;
    return nba97_game_clear_ordering_table(&composed, progress);
}
