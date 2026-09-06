#include "frontend_dispatch_adapter.h"

#include <cstddef>
#include <cstdint>

namespace {
constexpr Nba97FrontendDispatchSiteContract Contracts[] = {
    {0, 0, 0, 0},
    {0x8003f8c8, 0x8003f8cc, 0x8003f7b0, 2},
    {0x8003f8dc, 0x8003f8e0, 0x800770d4, 3},
    {0x8003f8f4, 0x8003f8f8, 0x80030cdc, 0},
    {0x8003f8fc, 0x8003f900, 0x80030308, 0},
    {0x8003f92c, 0x8003f930, 0x8003d2a4, 0},
    {0x8003f97c, 0x8003f980, 0x800459c8, 0},
    {0x8003fa08, 0x8003fa0c, 0x80031a88, 1},
    {0x8003fa3c, 0x8003fa40, 0x8003f43c, 0},
    {0x8003fbd8, 0x8003fbdc, 0x8003d930, 5},
    {0x8003fc58, 0x8003fc5c, 0x8003d930, 5},
    {0x8003fc68, 0x8003fc6c, 0x8002f0e8, 1},
    {0x8003fc78, 0x8003fc7c, 0x8004fcd8, 0},
    {0x8003fca8, 0x8003fcac, 0x8004f5f4, 0},
    {0x8003fcdc, 0x8003fce0, 0x80041144, 1},
    {0x8003fcf4, 0x8003fcf8, 0x80037010, 0},
    {0x8003fd10, 0x8003fd14, 0x8003b194, 0},
    {0x8003fd3c, 0x8003fd40, 0x80061674, 1},
    {0x8003fd44, 0x8003fd48, 0x80046d24, 0},
    {0x8003fd4c, 0x8003fd50, 0x8003e7a8, 0},
    {0x8003fd74, 0x8003fd78, 0x8003f778, 1},
    {0x8003fd7c, 0x8003fd80, 0x80044944, 0},
    {0x8003fe14, 0x8003fe18, 0x800435a4, 0},
    {0x8003fe58, 0x8003fe5c, 0x800417d4, 0},
    {0x8003fe98, 0x8003fe9c, 0x80041df4, 0},
    {0x8003fed8, 0x8003fedc, 0x8003f778, 1},
    {0x8003fee0, 0x8003fee4, 0x80046354, 0},
    {0x8003ff00, 0x8003ff04, 0x800435a4, 0},
    {0x8003ff10, 0x8003ff14, 0x80057ce4, 0},
    {0x8003ff8c, 0x8003ff90, 0x8003f7b0, 2},
    {0x8004005c, 0x80040060, 0x80042288, 0},
    {0x8004006c, 0x80040070, 0x80053f4c, 0},
    {0x8004009c, 0x800400a0, 0x8005428c, 0},
    {0x800400ac, 0x800400b0, 0x8005460c, 0},
    {0x800400f4, 0x800400f8, 0x80056aec, 1},
    {0x80040128, 0x8004012c, 0x80056cd0, 2},
    {0x80040158, 0x8004015c, 0x80056f9c, 1},
    {0x80040184, 0x80040188, 0x80057508, 0},
    {0x80040194, 0x80040198, 0x800592c4, 0},
    {0x800401c4, 0x800401c8, 0x8005721c, 1},
    {0x800401fc, 0x80040200, 0x80041a38, 0},
    {0x8004028c, 0x80040290, 0x8005cf78, 0},
    {0x800402d8, 0x800402dc, 0x80059220, 0},
    {0x800402e8, 0x800402ec, 0x8005bf34, 0},
    {0x80040350, 0x80040354, 0x800431d4, 0},
    {0x80040360, 0x80040364, 0x80058a18, 0},
    {0x80040370, 0x80040374, 0x8005b500, 0},
    {0x80040380, 0x80040384, 0x8005bc8c, 0},
    {0x80040390, 0x80040394, 0x8003f778, 1},
    {0x80040398, 0x8004039c, 0x800482f0, 0},
    {0x800403d4, 0x800403d8, 0x8004875c, 1},
    {0x800403dc, 0x800403e0, 0x800487e0, 0},
    {0x80040410, 0x80040414, 0x80047618, 0},
    {0x80040474, 0x80040478, 0x80049c40, 0},
    {0x80040548, 0x8004054c, 0x800435a4, 0},
    {0x80040558, 0x8004055c, 0x80046f80, 0},
    {0x800405d8, 0x800405dc, 0x80047194, 0},
    {0x80040658, 0x8004065c, 0x8004dae8, 0},
    {0x800406bc, 0x800406c0, 0x8004e46c, 0},
    {0x800406e8, 0x800406ec, 0x8004d514, 1},
    {0x800406fc, 0x80040700, 0x8004e768, 0},
    {0x8004070c, 0x80040710, 0x8004d514, 1},
    {0x8004071c, 0x80040720, 0x8005a880, 0},
    {0x8004072c, 0x80040730, 0x8005a538, 0},
    {0x8004073c, 0x80040740, 0x8005cb2c, 0},
    {0x8004076c, 0x80040770, 0x8005c4e0, 0},
    {0x8004077c, 0x80040780, 0x8005d46c, 0},
    {0x800407d4, 0x800407d8, 0x8005d7d4, 0},
    {0x800407e8, 0x800407ec, 0x80028b8c, 0},
    {0x800407f0, 0x800407f4, 0x800804e8, 1},
    {0x800407f8, 0x800407fc, 0x80028b8c, 0},
    {0x80040830, 0x80040834, 0x800357b0, 0},
    {0x80040850, 0x80040854, 0x8005851c, 1},
    {0x80040868, 0x8004086c, 0x8005851c, 1},
    {0x80040900, 0x80040904, 0x800909a8, 3},
    {0x80040964, 0x80040968, 0x800909a8, 3},
    {0x800409a8, 0x800409ac, 0x8004e9d8, 2},
    {0x800409d0, 0x800409d4, 0x8004e9d8, 2},
    {0x800409d8, 0x800409dc, 0x80029dd0, 0},
    {0x800409e0, 0x800409e4, 0x8002fc30, 0},
};

static_assert(sizeof Contracts / sizeof Contracts[0] ==
              NBA97_FRONTEND_DISPATCH_SITE_COUNT);

bool machineValid(const Nba97FrontendDispatchMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 15 || machine.hi.known_mask > 15 ||
      machine.lo.known_mask > 15)
    return false;
  for (unsigned i = 0; i < 32; ++i)
    if (machine.registers.gpr[i].known_mask > 15)
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

int nba97_frontend_dispatch_site_contract(
    uint8_t site, Nba97FrontendDispatchSiteContract *contract) {
  if (!contract || site == NBA97_FRONTEND_DISPATCH_SITE_NONE ||
      site >= NBA97_FRONTEND_DISPATCH_SITE_COUNT)
    return 0;
  *contract = Contracts[site];
  return 1;
}

int nba97_frontend_dispatch_from_800360d4(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendDispatchCallerEvent *event,
    Nba97FrontendDispatchMachine *machine) {
  auto *binding = static_cast<Nba97FrontendDispatchBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) || event->pc != 0x800360f4u ||
      event->delay_slot_pc != 0x800360f8u || event->entry != 0x8003f7c8u ||
      event->invocation != 1 || event->argument_count != 0 ||
      machine->registers.gpr[31].known_mask != 15 ||
      machine->registers.gpr[31].word != 0x800360fcu ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97FrontendDispatchContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_frontend_dispatch(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
