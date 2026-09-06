#include "frontend_clock_read_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::uint8_t FE = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
constexpr Nba97FrontendClockReadParentContract Initial{
    0x8002efe4u, 0x8002efe8u, 0x8008da5cu, 0x8002efecu,
    NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFE4, 0, FE};
constexpr Nba97FrontendClockReadParentContract Loop{
    0x8002f018u, 0x8002f01cu, 0x8008da5cu, 0x8002f020u,
    NBA97_FRONTEND_EXIT_WAIT_SITE_8002F018, 0, FE};

bool machineValid(const Nba97FrontendClockReadMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 0x0f ||
      machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
    return false;
  for (unsigned i = 0; i < NBA97_FRONTEND_CLOCK_READ_REGISTER_COUNT; ++i)
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

const Nba97FrontendClockReadParentContract *contractFor(
    std::uint8_t site) {
  if (site == Initial.site) return &Initial;
  if (site == Loop.site) return &Loop;
  return nullptr;
}

bool eventValid(const Nba97FrontendExitWaitEvent &event,
                const Nba97FrontendClockReadMachine &machine) {
  const auto *contract = contractFor(event.site);
  return contract && event.pc == contract->pc &&
         event.delay_slot_pc == contract->delay_slot_pc &&
         event.entry == contract->target &&
         event.argument_count == contract->argument_count &&
         event.target_program == contract->target_program &&
         ((event.site == Initial.site && event.invocation == 1) ||
          (event.site == Loop.site && event.invocation >= 1)) &&
         machine.registers.gpr[NBA97_FRONTEND_CLOCK_READ_RA].word ==
             contract->return_address &&
         machine.registers.gpr[NBA97_FRONTEND_CLOCK_READ_RA].known_mask ==
             0x0f;
}

struct Composition {
  Nba97FrontendExitWaitIo fallback;
  void *fallback_user;
  Nba97FrontendClockReadBinding *binding;
  Nba97FrontendClockReadAdapterProgress *out;
};

void copyResult(Composition &composition,
                const Nba97FrontendExitWaitEvent &event) {
  auto &binding = *composition.binding;
  auto &out = *composition.out;
  ++out.invocations;
  if (binding.result == NBA97_TEXT_COMPLETE) ++out.completions;
  if (event.site == Initial.site) {
    ++out.initial_invocations;
    out.initial_event = binding.parent_event;
    out.initial_parent_machine = binding.parent_machine;
    out.initial_progress = binding.progress;
    out.initial_result = binding.result;
    out.initial_access = {};
    if (binding.progress.access_events && binding.access_journal &&
        binding.access_journal_capacity)
      out.initial_access = binding.access_journal[0];
  } else {
    ++out.loop_invocations;
    out.loop_event = binding.parent_event;
    out.loop_parent_machine = binding.parent_machine;
    out.loop_progress = binding.progress;
    out.loop_result = binding.result;
    out.loop_access = {};
    if (binding.progress.access_events && binding.access_journal &&
        binding.access_journal_capacity)
      out.loop_access = binding.access_journal[0];
  }
}

int composedIo(void *opaque, const Nba97GameTextMemory *memory,
               const Nba97FrontendExitWaitEvent *event,
               Nba97FrontendExitWaitMachine *machine) {
  auto &composition = *static_cast<Composition *>(opaque);
  if (event && (event->site == Initial.site || event->site == Loop.site)) {
    const std::size_t before = composition.binding->invocations;
    const int accepted = nba97_frontend_clock_read_from_frontend_exit_wait(
        composition.binding, memory, event, machine);
    if (composition.binding->invocations != before)
      copyResult(composition, *event);
    return accepted;
  }
  return composition.fallback
             ? composition.fallback(composition.fallback_user, memory, event,
                                    machine)
             : 0;
}
} // namespace

int nba97_frontend_clock_read_parent_contract(
    uint8_t site, Nba97FrontendClockReadParentContract *contract) {
  const auto *found = contractFor(site);
  if (!found || !contract) return 0;
  *contract = *found;
  return 1;
}

int nba97_frontend_clock_read_from_frontend_exit_wait(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendExitWaitEvent *event,
    Nba97FrontendExitWaitMachine *machine) {
  auto *binding = static_cast<Nba97FrontendClockReadBinding *>(opaque);
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
  Nba97FrontendClockReadContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_frontend_clock_read(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE) return 0;
  ++binding->completions;
  return 1;
}

int nba97_frontend_exit_wait_with_recovered_clock(
    Nba97FrontendExitWaitContext *context,
    Nba97FrontendClockReadBinding *binding,
    Nba97FrontendExitWaitProgress *progress,
    Nba97FrontendClockReadAdapterProgress *adapter_progress) {
  if (!adapter_progress) return NBA97_TEXT_ARGUMENT;
  std::memset(adapter_progress, 0, sizeof *adapter_progress);
  if (!context || !binding) return NBA97_TEXT_ARGUMENT;
  Composition composition{context->io, context->user, binding,
                          adapter_progress};
  Nba97FrontendExitWaitContext composed = *context;
  composed.io = composedIo;
  composed.user = &composition;
  return nba97_frontend_exit_wait(&composed, progress);
}
