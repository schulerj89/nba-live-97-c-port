#include "frontend_load_payload_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr Nba97FrontendLoadPayloadParentContract Contract{
    0x8007b124u, 0x8007b128u, 0x8007b15cu, 0x8007b12cu,
    NBA97_FRONTEND_OVERLAY_LOAD_SITE_8007B124, 3,
    NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};

bool machineValid(const Nba97FrontendLoadPayloadMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 0x0f ||
      machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
    return false;
  for (unsigned i = 0; i < NBA97_FRONTEND_LOAD_PAYLOAD_REGISTER_COUNT; ++i)
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

bool eventValid(const Nba97FrontendOverlayLoadEvent &event,
                const Nba97FrontendLoadPayloadMachine &machine) {
  return event.site == Contract.site && event.pc == Contract.pc &&
         event.delay_slot_pc == Contract.delay_slot_pc &&
         event.entry == Contract.target && event.invocation == 1 &&
         event.argument_count == Contract.argument_count &&
         event.target_program == Contract.target_program &&
         machine.registers.gpr[NBA97_FRONTEND_LOAD_PAYLOAD_A2].word == 1 &&
         machine.registers.gpr[NBA97_FRONTEND_LOAD_PAYLOAD_A2].known_mask ==
             0x0f &&
         machine.registers.gpr[NBA97_FRONTEND_LOAD_PAYLOAD_RA].word ==
             Contract.return_address &&
         machine.registers.gpr[NBA97_FRONTEND_LOAD_PAYLOAD_RA].known_mask ==
             0x0f;
}

struct Composition {
  Nba97FrontendOverlayLoadIo fallback;
  void *fallback_user;
  Nba97FrontendLoadPayloadBinding *binding;
  Nba97FrontendLoadPayloadAdapterProgress *out;
};

int composedIo(void *opaque, const Nba97GameTextMemory *memory,
               const Nba97FrontendOverlayLoadEvent *event,
               Nba97FrontendOverlayLoadMachine *machine) {
  auto &composition = *static_cast<Composition *>(opaque);
  if (event && event->site == Contract.site) {
    const std::size_t before = composition.binding->invocations;
    const int accepted = nba97_frontend_load_payload_from_frontend_overlay_load(
        composition.binding, memory, event, machine);
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
                                    machine)
             : 0;
}
} // namespace

int nba97_frontend_load_payload_parent_contract(
    Nba97FrontendLoadPayloadParentContract *contract) {
  if (!contract) return 0;
  *contract = Contract;
  return 1;
}

int nba97_frontend_load_payload_from_frontend_overlay_load(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendOverlayLoadEvent *event,
    Nba97FrontendOverlayLoadMachine *machine) {
  auto *binding = static_cast<Nba97FrontendLoadPayloadBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) || !eventValid(*event, *machine) ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity)) {
    if (binding) binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->parent_event = *event;
  binding->parent_machine = *machine;
  Nba97FrontendLoadPayloadContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_frontend_load_payload(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE) return 0;
  ++binding->completions;
  return 1;
}

int nba97_frontend_overlay_load_with_recovered_payload(
    Nba97FrontendOverlayLoadContext *context,
    Nba97FrontendLoadPayloadBinding *binding,
    Nba97FrontendOverlayLoadProgress *progress,
    Nba97FrontendLoadPayloadAdapterProgress *adapter_progress) {
  if (!adapter_progress) return NBA97_TEXT_ARGUMENT;
  std::memset(adapter_progress, 0, sizeof *adapter_progress);
  if (!context || !binding) return NBA97_TEXT_ARGUMENT;
  Composition composition{context->io, context->user, binding,
                          adapter_progress};
  Nba97FrontendOverlayLoadContext composed = *context;
  composed.io = composedIo;
  composed.user = &composition;
  return nba97_frontend_overlay_load(&composed, progress);
}
