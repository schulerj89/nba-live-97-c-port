#include "gameload_bios_heap_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr Nba97GameloadBiosHeapParentContract Contract{
    UINT32_C(0x801e1498), UINT32_C(0x801e149c),
    UINT32_C(0x801e1590), UINT32_C(0x801e14a0), 2079u, 1u,
    NBA97_GAMELOAD_ENTRY_SITE_801E1498, 2u,
    NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD};

bool machineValid(const Nba97GameloadBiosHeapMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u ||
      machine.hi.known_mask > 15u || machine.lo.known_mask > 15u)
    return false;
  for (unsigned index = 0u;
       index != NBA97_GAMELOAD_BIOS_HEAP_REGISTER_COUNT; ++index)
    if (machine.registers.gpr[index].known_mask > 15u)
      return false;
  return true;
}

bool memoryValid(const Nba97GameTextMemory &memory) {
  if (memory.count != 0u && memory.region == nullptr)
    return false;
  for (std::size_t index = 0u; index != memory.count; ++index) {
    const Nba97GameTextRegion &region = memory.region[index];
    if (region.data == nullptr || region.size == 0u ||
        region.size > UINT64_C(0x100000000) ||
        std::uint64_t(region.base) + region.size > UINT64_C(0x100000000))
      return false;
    for (std::size_t earlier = 0u; earlier != index; ++earlier) {
      const Nba97GameTextRegion &other = memory.region[earlier];
      if (std::uint64_t(region.base) <
              std::uint64_t(other.base) + other.size &&
          std::uint64_t(other.base) <
              std::uint64_t(region.base) + region.size)
        return false;
    }
  }
  return true;
}

bool eventValid(const Nba97GameloadEntryEvent &event,
                const Nba97GameloadBiosHeapMachine &machine) {
  return event.pc == Contract.pc &&
         event.delay_slot_pc == Contract.delay_slot_pc &&
         event.entry == Contract.target && event.operation == Contract.operation &&
         event.invocation == Contract.invocation && event.site == Contract.site &&
         event.argument_count == Contract.argument_count &&
         event.target_program == Contract.target_program &&
         machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_RA].word ==
             Contract.return_address &&
         machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_RA].known_mask == 15u;
}

struct Composition {
  Nba97GameloadEntryIo fallback;
  void *fallback_user;
  Nba97GameloadBiosHeapBinding *binding;
  Nba97GameloadBiosHeapAdapterProgress *progress;
};

int composedIo(void *opaque, const Nba97GameTextMemory *memory,
               const Nba97GameloadEntryEvent *event,
               Nba97GameloadEntryMachine *machine,
               Nba97GameloadEntryCalleeOutcome *outcome) {
  Composition &composition = *static_cast<Composition *>(opaque);
  if (event != nullptr &&
      event->site == NBA97_GAMELOAD_ENTRY_SITE_801E1498) {
    const std::size_t before = composition.binding->invocations;
    const int accepted = nba97_gameload_bios_heap_from_gameload_entry(
        composition.binding, memory, event, machine, outcome);
    if (composition.binding->invocations != before) {
      ++composition.progress->invocations;
      if (composition.binding->result == NBA97_TEXT_COMPLETE)
        ++composition.progress->completions;
      composition.progress->parent_event = composition.binding->parent_event;
      composition.progress->parent_machine =
          composition.binding->parent_machine;
      composition.progress->progress = composition.binding->progress;
      composition.progress->result = composition.binding->result;
    }
    return accepted;
  }
  return composition.fallback != nullptr
             ? composition.fallback(composition.fallback_user, memory, event,
                                    machine, outcome)
             : 0;
}

} // namespace

extern "C" void nba97_gameload_bios_heap_binding_init(
    Nba97GameloadBiosHeapBinding *binding, size_t operation_budget,
    Nba97GameloadBiosHeapIo bios_io, void *bios_user,
    uint32_t *instruction_journal, size_t instruction_journal_capacity) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operation_budget;
  binding->bios_io = bios_io;
  binding->bios_user = bios_user;
  binding->instruction_journal = instruction_journal;
  binding->instruction_journal_capacity = instruction_journal_capacity;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_gameload_bios_heap_parent_contract(
    Nba97GameloadBiosHeapParentContract *contract) {
  if (contract == nullptr)
    return 0;
  *contract = Contract;
  return 1;
}

extern "C" int nba97_gameload_bios_heap_from_gameload_entry(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameloadEntryEvent *event, Nba97GameloadEntryMachine *machine,
    Nba97GameloadEntryCalleeOutcome *outcome) {
  Nba97GameloadBiosHeapBinding *binding =
      static_cast<Nba97GameloadBiosHeapBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr || outcome == nullptr || !memoryValid(*memory) ||
      !machineValid(*machine) || !eventValid(*event, *machine) ||
      (binding->instruction_journal == nullptr &&
       binding->instruction_journal_capacity != 0u)) {
    if (binding != nullptr)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->parent_event = *event;
  binding->parent_machine = *machine;
  Nba97GameloadBiosHeapContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->bios_io;
  context.user = binding->bios_user;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity =
      binding->instruction_journal_capacity;
  binding->result = nba97_gameload_bios_heap(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  *outcome = NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED;
  return 1;
}

extern "C" int nba97_gameload_entry_with_recovered_bios_heap(
    const Nba97GameloadEntryContext *context,
    Nba97GameloadBiosHeapBinding *binding,
    Nba97GameloadEntryProgress *progress,
    Nba97GameloadBiosHeapAdapterProgress *adapter_progress) {
  if (adapter_progress == nullptr)
    return NBA97_TEXT_ARGUMENT;
  std::memset(adapter_progress, 0, sizeof(*adapter_progress));
  if (context == nullptr || binding == nullptr || progress == nullptr)
    return NBA97_TEXT_ARGUMENT;
  Composition composition{context->io, context->user, binding,
                          adapter_progress};
  Nba97GameloadEntryContext composed = *context;
  composed.io = composedIo;
  composed.user = &composition;
  const std::size_t before = binding->invocations;
  const int result = nba97_gameload_entry(&composed, progress);
  if (result == NBA97_TEXT_IO_REFUSED && binding->invocations != before &&
      binding->result != NBA97_TEXT_COMPLETE)
    return binding->result;
  return result;
}
