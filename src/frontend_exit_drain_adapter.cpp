#include "frontend_exit_drain_adapter.h"

#include <cstddef>
#include <cstdint>

namespace {
constexpr std::uint8_t FE = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
constexpr Nba97FrontendExitDrainSiteContract Contracts[] = {
    {0, 0, 0, 0, 0},
    {0x800394e8u, 0x800394ecu, 0x800393f0u, 0, FE},
    {0x800394f0u, 0x800394f4u, 0x800392a0u, 0, FE},
    {0x80039500u, 0x80039504u, 0x80038e84u, 0, FE},
    {0x80039530u, 0x80039534u, 0x80029b64u, 2, FE},
    {0x80039538u, 0x8003953cu, 0x8008c274u, 0, FE},
    {0x80039554u, 0x80039558u, 0x8006cde4u, 1, FE},
    {0x8003955cu, 0x80039560u, 0x8006ae60u, 0, FE},
};
static_assert(sizeof Contracts / sizeof Contracts[0] ==
              NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT);

bool machineValid(const Nba97FrontendExitCleanupMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 0x0f ||
      machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
    return false;
  for (unsigned i = 0; i < NBA97_FRONTEND_EXIT_CLEANUP_REGISTER_COUNT; ++i)
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

int nba97_frontend_exit_drain_site_contract(
    uint8_t site, Nba97FrontendExitDrainSiteContract *contract) {
  if (!contract || site == NBA97_FRONTEND_EXIT_DRAIN_SITE_NONE ||
      site >= NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT)
    return 0;
  *contract = Contracts[site];
  return 1;
}

int nba97_frontend_exit_drain_from_frontend_exit_cleanup(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendExitCleanupEvent *event,
    Nba97FrontendExitCleanupMachine *machine) {
  auto *binding = static_cast<Nba97FrontendExitDrainBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->site != NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F094 ||
      event->pc != 0x8002f094u || event->delay_slot_pc != 0x8002f098u ||
      event->entry != 0x800394d4u || event->invocation != 1 ||
      event->argument_count != 0 ||
      event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
      machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA].word !=
          0x8002f09cu ||
      machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA].known_mask !=
          0x0f ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97FrontendExitDrainContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_frontend_exit_drain(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
