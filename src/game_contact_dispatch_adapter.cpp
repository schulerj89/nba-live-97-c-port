#include "game_contact_dispatch_adapter.h"

#include <cstddef>
#include <cstdint>

namespace {
bool machine_well_formed(const Nba97GameContactDispatchMachine& machine) {
    if (machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0f ||
        machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
        return false;
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (machine.registers.gpr[i].known_mask > 0x0f)
            return false;
    return true;
}

bool memory_well_formed(const Nba97GameTextMemory& memory) {
    if (!memory.region && memory.count)
        return false;
    for (std::size_t i = 0; i < memory.count; ++i) {
        const auto& a = memory.region[i];
        if (!a.data || !a.size || a.size > UINT64_C(0x100000000) ||
            std::uint64_t(a.base) + a.size > UINT64_C(0x100000000))
            return false;
        for (std::size_t j = 0; j < i; ++j) {
            const auto& b = memory.region[j];
            if (std::uint64_t(a.base) < std::uint64_t(b.base) + b.size &&
                std::uint64_t(b.base) < std::uint64_t(a.base) + a.size)
                return false;
        }
    }
    return true;
}
}

int nba97_game_contact_dispatch_compose_children(void* opaque,
    const Nba97GameTextMemory* memory,
    const Nba97GameContactDispatchEvent* event,
    Nba97GameContactDispatchMachine* machine) {
    auto* children = static_cast<Nba97GameContactDispatchChildren*>(opaque);
    if (!children || !memory || !event || !machine ||
        !machine_well_formed(*machine) ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 0x0f ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
            event->pc + 8u)
        return 0;
    if (event->entry == UINT32_C(0x8005faa8)) {
        if (event->pc != UINT32_C(0x8006104c) ||
            event->delay_slot_pc != UINT32_C(0x80061050) ||
            event->kind != NBA97_GAME_CONTACT_DISPATCH_CHILD_8005FAA8 ||
            event->argument_count != 2 || !children->child_8005FAA8)
            return 0;
        ++children->child_8005FAA8_invocations;
        return children->child_8005FAA8(children->user, memory, event,
            machine);
    }
    if (event->entry != UINT32_C(0x80060e8c) ||
        (event->pc != UINT32_C(0x80061070) &&
         event->pc != UINT32_C(0x800610c4)) ||
        event->delay_slot_pc != event->pc + 4u ||
        event->kind != NBA97_GAME_CONTACT_DISPATCH_CHILD_80060E8C ||
        event->argument_count != 2 ||
        !memory_well_formed(*memory) ||
        (!children->ball_gate_access_journal &&
         children->ball_gate_access_journal_capacity))
        return 0;

    Nba97GameBallContactGateContext context{};
    context.memory = *memory;
    context.operation_budget = children->ball_gate_operation_budget;
    context.machine = *machine;
    context.io = children->child_800602CC;
    context.user = children->user;
    context.access_journal = children->ball_gate_access_journal;
    context.access_journal_capacity = children->ball_gate_access_journal_capacity;
    ++children->ball_gate_invocations;
    children->ball_gate_result = children->contact_binding ?
        nba97_game_ball_contact_gate_run(&context, &children->ball_gate_progress,
            children->contact_binding) :
        nba97_game_ball_contact_gate(&context, &children->ball_gate_progress);
    *machine = children->ball_gate_progress.machine;
    return children->ball_gate_result == NBA97_TEXT_COMPLETE;
}

int nba97_game_contact_dispatch_from_match_tick(void* opaque,
    const Nba97MatchTickCall* call, Nba97GamePeriodValue* result) {
    auto* binding = static_cast<Nba97GameContactDispatchBinding*>(opaque);
    if (!binding || !call || result ||
        call->pc != UINT32_C(0x80068e08) ||
        call->entry != UINT32_C(0x80060fbc) || call->count != 0 ||
        call->args[0] != 0 || call->args[1] != 0 ||
        binding->entry_machine_ready != 1 ||
        binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .known_mask != 0x0f ||
        binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
            call->pc + 8u ||
        (!binding->memory.region && binding->memory.count) ||
        (!binding->access_journal && binding->access_journal_capacity)) {
        if (binding) binding->result = NBA97_TEXT_ARGUMENT;
        return 0;
    }

    Nba97GameContactDispatchContext context{};
    context.memory = binding->memory;
    context.operation_budget = binding->operation_budget;
    context.machine = binding->entry_machine;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    ++binding->invocations;
    binding->result = nba97_game_contact_dispatch(&context, &binding->progress);
    return binding->result == NBA97_TEXT_COMPLETE;
}
