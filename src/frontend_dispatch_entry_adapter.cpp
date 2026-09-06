#include "frontend_dispatch_entry_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
bool machineValid(const Nba97FrontendDispatchEntryMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 0x0f ||
      machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
    return false;
  for (unsigned i = 0; i < NBA97_FRONTEND_DISPATCH_ENTRY_REGISTER_COUNT; ++i)
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

struct Composition {
  Nba97FrontendDispatchBinding *binding;
  Nba97FrontendDispatchEntryAdapterProgress *progress;
};

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97FrontendDispatchEntryEvent *event,
             Nba97FrontendDispatchEntryMachine *machine) {
  auto &composition = *static_cast<Composition *>(opaque);
  auto &out = *composition.progress;
  if (!event) {
    out.dispatcher_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++out.dispatcher_invocations;
  out.dispatcher_event = *event;
  const Nba97FrontendDispatchCallerEvent converted{
      event->pc,       event->delay_slot_pc, event->entry,
      event->operation, event->invocation,    event->argument_count};
  const std::size_t beforeInvocations = composition.binding->invocations;
  const int accepted = nba97_frontend_dispatch_from_800360d4(
      composition.binding, memory, &converted, machine);
  out.dispatcher_result = composition.binding->result;
  if (composition.binding->invocations != beforeInvocations)
    out.dispatcher_progress = composition.binding->progress;
  else
    std::memset(&out.dispatcher_progress, 0,
                sizeof out.dispatcher_progress);
  if (accepted == 1)
    ++out.dispatcher_completions;
  return accepted;
}
} // namespace

int nba97_frontend_dispatch_entry_with_recovered_dispatch(
    Nba97FrontendDispatchEntryContext *context,
    Nba97FrontendDispatchBinding *binding,
    Nba97FrontendDispatchEntryProgress *progress,
    Nba97FrontendDispatchEntryAdapterProgress *adapter) {
  if (!context || !binding || !progress || !adapter)
    return NBA97_TEXT_ARGUMENT;
  std::memset(adapter, 0, sizeof *adapter);
  Nba97FrontendDispatchEntryContext composed = *context;
  Composition composition{binding, adapter};
  composed.io = dispatch;
  composed.user = &composition;
  const int result = nba97_frontend_dispatch_entry(&composed, progress);
  if (!adapter->dispatcher_invocations)
    adapter->dispatcher_result = NBA97_TEXT_ARGUMENT;
  return result;
}

int nba97_frontend_dispatch_entry_from_frontend_main(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendDispatchEntryCallerEvent *event,
    Nba97FrontendDispatchEntryMachine *machine) {
  auto *binding = static_cast<Nba97FrontendDispatchEntryBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) || event->pc != 0x80028aa0u ||
      event->delay_slot_pc != 0x80028aa4u || event->entry != 0x800360d4u ||
      event->invocation != 1 || event->argument_count != 0 ||
      machine->registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_S0].word != 0 ||
      machine->registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_S0].known_mask !=
          0x0f ||
      machine->registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA].word !=
          0x80028aa8u ||
      machine->registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA].known_mask !=
          0x0f ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97FrontendDispatchEntryContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_frontend_dispatch_entry_with_recovered_dispatch(
      &context, &binding->dispatcher, &binding->progress, &binding->adapter);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
