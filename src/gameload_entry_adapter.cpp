#include "gameload_entry_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr Nba97GameloadEntryParentContract Contract{
    0x80028b68u, 0x80028b6cu, 0x801e1410u, 0x80028b70u,
    NBA97_FRONTEND_MAIN_SITE_80028B68, 0,
    NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD};

bool machineValid(const Nba97GameloadEntryMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 0x0f ||
      machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
    return false;
  for (unsigned i = 0; i < NBA97_GAMELOAD_ENTRY_REGISTER_COUNT; ++i)
    if (machine.registers.gpr[i].known_mask > 0x0f)
      return false;
  return true;
}

bool memoryValid(const Nba97GameTextMemory &memory) {
  if (!memory.region && memory.count)
    return false;
  for (std::size_t i = 0; i < memory.count; ++i) {
    const auto &a = memory.region[i];
    if (!a.data || !a.size || a.size > UINT64_C(0x100000000) ||
        std::uint64_t(a.base) + a.size > UINT64_C(0x100000000))
      return false;
    for (std::size_t j = 0; j < i; ++j) {
      const auto &b = memory.region[j];
      if (std::uint64_t(a.base) < std::uint64_t(b.base) + b.size &&
          std::uint64_t(b.base) < std::uint64_t(a.base) + a.size)
        return false;
    }
  }
  return true;
}

bool eventValid(const Nba97FrontendMainEvent &event,
                const Nba97GameloadEntryMachine &machine) {
  return event.site == Contract.site && event.pc == Contract.pc &&
         event.delay_slot_pc == Contract.delay_slot_pc &&
         event.entry == Contract.target && event.invocation == 1 &&
         event.argument_count == Contract.argument_count &&
         event.target_program == Contract.target_program &&
         machine.registers.gpr[NBA97_GAMELOAD_ENTRY_RA].word ==
             Contract.return_address &&
         machine.registers.gpr[NBA97_GAMELOAD_ENTRY_RA].known_mask == 0x0f;
}

struct Composition {
  Nba97FrontendMainIo fallback;
  void *fallback_user;
  Nba97GameloadEntryBinding *binding;
  Nba97GameloadEntryAdapterProgress *out;
};

int composedIo(void *opaque, const Nba97GameTextMemory *memory,
               const Nba97FrontendMainEvent *event,
               Nba97FrontendMainMachine *machine,
               Nba97FrontendMainCalleeOutcome *outcome) {
  auto &composition = *static_cast<Composition *>(opaque);
  if (event && event->site == Contract.site) {
    const std::size_t before = composition.binding->invocations;
    const int accepted = nba97_gameload_entry_from_frontend_main(
        composition.binding, memory, event, machine, outcome);
    if (composition.binding->invocations != before) {
      ++composition.out->invocations;
      if (composition.binding->result == NBA97_TEXT_COMPLETE)
        ++composition.out->completions;
      composition.out->parent_event = composition.binding->parent_event;
      composition.out->parent_machine = composition.binding->parent_machine;
      composition.out->progress = composition.binding->progress;
      composition.out->result = composition.binding->result;
    }
    return accepted;
  }
  return composition.fallback
             ? composition.fallback(composition.fallback_user, memory, event,
                                    machine, outcome)
             : 0;
}
} // namespace

int nba97_gameload_entry_parent_contract(
    Nba97GameloadEntryParentContract *contract) {
  if (!contract)
    return 0;
  *contract = Contract;
  return 1;
}

int nba97_gameload_entry_from_frontend_main(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendMainEvent *event, Nba97FrontendMainMachine *machine,
    Nba97FrontendMainCalleeOutcome *outcome) {
  auto *binding = static_cast<Nba97GameloadEntryBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !outcome ||
      !memoryValid(*memory) || !machineValid(*machine) ||
      !eventValid(*event, *machine) ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->parent_event = *event;
  binding->parent_machine = *machine;
  Nba97GameloadEntryContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_gameload_entry(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE ||
      !binding->progress.transferred)
    return 0;
  ++binding->completions;
  *outcome = NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED;
  return 1;
}

int nba97_frontend_main_with_recovered_memory_copy_and_gameload(
    Nba97FrontendMainContext *context,
    Nba97FrontendMemoryCopyBinding *copy_binding,
    Nba97GameloadEntryBinding *gameload_binding,
    Nba97FrontendMainProgress *progress,
    Nba97GameloadEntryAdapterProgress *adapter_progress) {
  if (!adapter_progress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(adapter_progress, 0, sizeof *adapter_progress);
  if (!context || !copy_binding || !gameload_binding)
    return NBA97_TEXT_ARGUMENT;
  Composition composition{context->io, context->user, gameload_binding,
                          adapter_progress};
  Nba97FrontendMainContext composed = *context;
  composed.io = composedIo;
  composed.user = &composition;
  return nba97_frontend_main_with_recovered_memory_copy(
      &composed, copy_binding, progress);
}
