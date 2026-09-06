#include "frontend_overlay_load_adapter.h"

#include <cstddef>
#include <cstdint>

namespace {
constexpr std::uint8_t FE = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
constexpr Nba97FrontendOverlayLoadSiteContract Contracts[] = {
    {0, 0, 0, 0, 0},
    {0x8007b124u, 0x8007b128u, 0x8007b15cu, 3, FE},
};
static_assert(sizeof Contracts / sizeof Contracts[0] ==
              NBA97_FRONTEND_OVERLAY_LOAD_SITE_COUNT);

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

int nba97_frontend_overlay_load_site_contract(
    uint8_t site, Nba97FrontendOverlayLoadSiteContract *contract) {
  if (!contract || site == NBA97_FRONTEND_OVERLAY_LOAD_SITE_NONE ||
      site >= NBA97_FRONTEND_OVERLAY_LOAD_SITE_COUNT)
    return 0;
  *contract = Contracts[site];
  return 1;
}

int nba97_frontend_overlay_load_from_frontend_main(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendMainEvent *event, Nba97FrontendMainMachine *machine,
    Nba97FrontendMainCalleeOutcome *outcome) {
  auto *binding = static_cast<Nba97FrontendOverlayLoadBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !outcome ||
      !memoryValid(*memory) || !machineValid(*machine) ||
      event->site != NBA97_FRONTEND_MAIN_SITE_80028ACC ||
      event->pc != 0x80028accu || event->delay_slot_pc != 0x80028ad0u ||
      event->entry != 0x8007b11cu || event->invocation != 1 ||
      event->argument_count != 2 ||
      event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
      machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0].word !=
          0x80024854u ||
      machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0].known_mask !=
          0x0f ||
      machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1].word != 0 ||
      machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1].known_mask !=
          0x0f ||
      machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_RA].word !=
          0x80028ad4u ||
      machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_RA].known_mask !=
          0x0f ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97FrontendOverlayLoadContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_frontend_overlay_load(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  *outcome = NBA97_FRONTEND_MAIN_CALLEE_RETURNED;
  return 1;
}
