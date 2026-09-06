#include "frontend_io_complete_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr Nba97FrontendIoCompleteParentContract Contract{
    0x800394f0u, 0x800394f4u, 0x800392a0u, 0x800394f8u,
    NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0, 0,
    NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};

constexpr Nba97FrontendIoCompleteParentContract IoContract{
    0x8003949cu, 0x800394a0u, 0x800392a0u, 0x800394a4u,
    NBA97_FRONTEND_IO_DRAIN_SITE_8003949C, 0,
    NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};

bool machineValid(const Nba97FrontendIoCompleteMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 0x0f ||
      machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
    return false;
  for (unsigned i = 0; i < NBA97_FRONTEND_IO_COMPLETE_REGISTER_COUNT; ++i)
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

bool eventValid(const Nba97FrontendExitDrainEvent &event,
                const Nba97FrontendIoCompleteMachine &machine,
                const Nba97FrontendIoCompleteParentContract &contract) {
  return event.site == contract.site && event.pc == contract.pc &&
         event.delay_slot_pc == contract.delay_slot_pc &&
         event.entry == contract.target && event.invocation >= 1 &&
         event.argument_count == contract.argument_count &&
         event.target_program == contract.target_program &&
         machine.registers.gpr[NBA97_FRONTEND_IO_COMPLETE_RA].word ==
             contract.return_address &&
         machine.registers.gpr[NBA97_FRONTEND_IO_COMPLETE_RA].known_mask ==
             0x0f;
}

struct Composition {
  Nba97FrontendExitDrainIo fallback;
  void *fallback_user;
  Nba97FrontendIoCompleteBinding *binding;
  Nba97FrontendIoCompleteAdapterProgress *out;
};

void copyCurrent(Composition &composition) {
  auto &binding = *composition.binding;
  auto &out = *composition.out;
  const bool first = out.invocations == 0;
  ++out.invocations;
  if (binding.result == NBA97_TEXT_COMPLETE) ++out.completions;
  out.latest_event = binding.parent_event;
  out.latest_parent_machine = binding.parent_machine;
  out.latest_progress = binding.progress;
  out.latest_access = {};
  out.latest_result = binding.result;
  if (binding.progress.access_events && binding.access_journal &&
      binding.access_journal_capacity)
    out.latest_access = binding.access_journal[0];
  if (first) {
    out.first_event = out.latest_event;
    out.first_parent_machine = out.latest_parent_machine;
    out.first_progress = out.latest_progress;
    out.first_access = out.latest_access;
    out.first_result = out.latest_result;
  }
}

int composedIo(void *opaque, const Nba97GameTextMemory *memory,
               const Nba97FrontendExitDrainEvent *event,
               Nba97FrontendExitDrainMachine *machine) {
  auto &composition = *static_cast<Composition *>(opaque);
  if (event && event->site == Contract.site) {
    const std::size_t before = composition.binding->invocations;
    const int accepted = nba97_frontend_io_complete_from_frontend_exit_drain(
        composition.binding, memory, event, machine);
    if (composition.binding->invocations != before) copyCurrent(composition);
    return accepted;
  }
  return composition.fallback
             ? composition.fallback(composition.fallback_user, memory, event,
                                    machine)
             : 0;
}
} // namespace

int nba97_frontend_io_complete_parent_contract(
    Nba97FrontendIoCompleteParentContract *contract) {
  if (!contract) return 0;
  *contract = Contract;
  return 1;
}

static int fromParent(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendExitDrainEvent *event,
    Nba97FrontendExitDrainMachine *machine,
    const Nba97FrontendIoCompleteParentContract &contract) {
  auto *binding = static_cast<Nba97FrontendIoCompleteBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) || !eventValid(*event, *machine, contract) ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity) ||
      (!binding->parent_journal && binding->parent_journal_capacity)) {
    if (binding) binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  const std::size_t record_index = binding->parent_events++;
  Nba97FrontendIoCompleteParentRecord *record =
      record_index < binding->parent_journal_capacity
          ? &binding->parent_journal[record_index]
          : nullptr;
  if (record) {
    std::memset(record, 0, sizeof *record);
    record->event = *event;
    record->parent_machine = *machine;
  }
  ++binding->invocations;
  binding->parent_event = *event;
  binding->parent_machine = *machine;
  Nba97FrontendIoCompleteContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_frontend_io_complete(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (record) {
    record->progress = binding->progress;
    record->result = binding->result;
    if (binding->progress.access_events && binding->access_journal &&
        binding->access_journal_capacity)
      record->first_access = binding->access_journal[0];
    record->access_events =
        binding->progress.access_events < binding->access_journal_capacity
            ? binding->progress.access_events
            : binding->access_journal_capacity;
    if (record->access_events > NBA97_FRONTEND_IO_COMPLETE_RECORD_ACCESSES)
      record->access_events = NBA97_FRONTEND_IO_COMPLETE_RECORD_ACCESSES;
    for (std::size_t i = 0; i < record->access_events; ++i)
      record->access_journal[i] = binding->access_journal[i];
    record->instruction_events =
        binding->progress.instruction_events < binding->instruction_journal_capacity
            ? binding->progress.instruction_events
            : binding->instruction_journal_capacity;
    if (record->instruction_events >
        NBA97_FRONTEND_IO_COMPLETE_RECORD_INSTRUCTIONS)
      record->instruction_events =
          NBA97_FRONTEND_IO_COMPLETE_RECORD_INSTRUCTIONS;
    for (std::size_t i = 0; i < record->instruction_events; ++i)
      record->instruction_journal[i] = binding->instruction_journal[i];
  }
  if (binding->result != NBA97_TEXT_COMPLETE) return 0;
  ++binding->completions;
  if (record) record->completed = 1;
  return 1;
}

int nba97_frontend_io_complete_from_frontend_exit_drain(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendExitDrainEvent *event,
    Nba97FrontendExitDrainMachine *machine) {
  return fromParent(opaque, memory, event, machine, Contract);
}

int nba97_frontend_io_complete_from_frontend_io_drain(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendIoDrainEvent *event,
    Nba97FrontendIoDrainMachine *machine) {
  if (!event) return fromParent(opaque, memory, nullptr, machine, IoContract);
  // These event types share a field layout but remain distinct C types.
  // Copy actual observed fields explicitly; preserve the natural caller PC.
  const Nba97FrontendExitDrainEvent observed{
      event->pc, event->delay_slot_pc, event->entry, event->operation,
      event->invocation, event->site, event->argument_count, event->target_program};
  return fromParent(opaque, memory, &observed, machine, IoContract);
}

int nba97_frontend_exit_drain_with_recovered_io_complete(
    Nba97FrontendExitDrainContext *context,
    Nba97FrontendIoCompleteBinding *binding,
    Nba97FrontendExitDrainProgress *progress,
    Nba97FrontendIoCompleteAdapterProgress *adapter_progress) {
  if (!adapter_progress) return NBA97_TEXT_ARGUMENT;
  std::memset(adapter_progress, 0, sizeof *adapter_progress);
  if (!context || !binding) return NBA97_TEXT_ARGUMENT;
  Composition composition{context->io, context->user, binding,
                          adapter_progress};
  Nba97FrontendExitDrainContext composed = *context;
  composed.io = composedIo;
  composed.user = &composition;
  return nba97_frontend_exit_drain(&composed, progress);
}
