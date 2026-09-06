#include "frontend_exit_cleanup_adapter.h"

#include <cstddef>
#include <cstdint>

namespace {
constexpr std::uint8_t FE = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
constexpr Nba97FrontendExitCleanupSiteContract Contracts[] = {
    {0, 0, 0, 0, 0},
    {0x8002f08cu, 0x8002f090u, 0x8002efbcu, 0, FE},
    {0x8002f094u, 0x8002f098u, 0x800394d4u, 0, FE},
    {0x8002f0a4u, 0x8002f0a8u, 0x80028c90u, 1, FE},
    {0x8002f0c0u, 0x8002f0c4u, 0x8007760cu, 1, FE},
    {0x8002f0d0u, 0x8002f0d4u, 0x80076540u, 0, FE},
};
static_assert(sizeof Contracts / sizeof Contracts[0] ==
              NBA97_FRONTEND_EXIT_CLEANUP_SITE_COUNT);

bool machineValid(const Nba97FrontendMainMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 0x0f ||
      machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
    return false;
  for (unsigned i = 0; i < NBA97_FRONTEND_MAIN_REGISTER_COUNT; ++i)
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
} // namespace

int nba97_frontend_exit_cleanup_site_contract(
    uint8_t site, Nba97FrontendExitCleanupSiteContract *contract) {
  if (!contract || site == NBA97_FRONTEND_EXIT_CLEANUP_SITE_NONE ||
      site >= NBA97_FRONTEND_EXIT_CLEANUP_SITE_COUNT)
    return 0;
  *contract = Contracts[site];
  return 1;
}

int nba97_frontend_exit_cleanup_from_frontend_main(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendMainEvent *event, Nba97FrontendMainMachine *machine,
    Nba97FrontendMainCalleeOutcome *outcome) {
  auto *binding = static_cast<Nba97FrontendExitCleanupBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !outcome ||
      !memoryValid(*memory) || !machineValid(*machine) ||
      event->site != NBA97_FRONTEND_MAIN_SITE_80028AA8 ||
      event->pc != 0x80028aa8u || event->delay_slot_pc != 0x80028aacu ||
      event->entry != 0x8002f084u || event->invocation != 1 ||
      event->argument_count != 0 ||
      event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
      machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].word != 0x80028ab0u ||
      machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].known_mask != 0x0f ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97FrontendExitCleanupContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_frontend_exit_cleanup(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  *outcome = NBA97_FRONTEND_MAIN_CALLEE_RETURNED;
  return 1;
}
