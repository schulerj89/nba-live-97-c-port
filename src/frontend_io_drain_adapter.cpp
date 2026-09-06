#include "frontend_io_drain_adapter.h"

#include <cstddef>
#include <cstdint>

namespace {
constexpr std::uint8_t FE = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
constexpr Nba97FrontendIoDrainSiteContract Contracts[] = {
    {0, 0, 0, 0, 0},
    {0x80039458u, 0x8003945cu, 0x80077638u, 1, FE},
    {0x8003949cu, 0x800394a0u, 0x800392a0u, 0, FE},
    {0x800394acu, 0x800394b0u, 0x80038e84u, 0, FE},
};
static_assert(sizeof Contracts / sizeof Contracts[0] ==
              NBA97_FRONTEND_IO_DRAIN_SITE_COUNT);

bool machineValid(const Nba97FrontendExitDrainMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 0x0f ||
      machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
    return false;
  for (unsigned i = 0; i < NBA97_FRONTEND_EXIT_DRAIN_REGISTER_COUNT; ++i)
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

int nba97_frontend_io_drain_site_contract(
    uint8_t site, Nba97FrontendIoDrainSiteContract *contract) {
  if (!contract || site == NBA97_FRONTEND_IO_DRAIN_SITE_NONE ||
      site >= NBA97_FRONTEND_IO_DRAIN_SITE_COUNT)
    return 0;
  *contract = Contracts[site];
  return 1;
}

int nba97_frontend_io_drain_from_frontend_exit_drain(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendExitDrainEvent *event,
    Nba97FrontendExitDrainMachine *machine) {
  auto *binding = static_cast<Nba97FrontendIoDrainBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->site != NBA97_FRONTEND_EXIT_DRAIN_SITE_800394E8 ||
      event->pc != 0x800394e8u || event->delay_slot_pc != 0x800394ecu ||
      event->entry != 0x800393f0u || event->invocation != 1 ||
      event->argument_count != 0 ||
      event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
      machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].word !=
          0x800394f0u ||
      machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].known_mask !=
          0x0f ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97FrontendIoDrainContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_frontend_io_drain(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
