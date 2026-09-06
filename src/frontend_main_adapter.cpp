#include "frontend_main_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::uint8_t FE = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
constexpr Nba97FrontendMainSiteContract Contracts[] = {
    {0, 0, 0, 0, 0, 0},
    {0x80028810, 0x80028814, 0x8007b844, 0, FE, 0},
    {0x80028818, 0x8002881c, 0x8008b368, 0, FE, 0},
    {0x80028834, 0x80028838, 0x800769e0, 3, FE, 0},
    {0x80028858, 0x8002885c, 0x80061674, 1, FE, 0},
    {0x80028880, 0x80028884, 0x8008bfb0, 2, FE, 0},
    {0x80028898, 0x8002889c, 0x80078b7c, 0, FE, 0},
    {0x800288a8, 0x800288ac, 0x8008a4f8, 1, FE, 0},
    {0x800288b8, 0x800288bc, 0x80079bf0, 2, FE, 0},
    {0x800288c0, 0x800288c4, 0x8007f5a8, 1, FE, 0},
    {0x800288c8, 0x800288cc, 0x8007f5d0, 0, FE, 0},
    {0x800288d0, 0x800288d4, 0x80076148, 1, FE, 0},
    {0x800288d8, 0x800288dc, 0x8008004c, 1, FE, 0},
    {0x800288ec, 0x800288f0, 0x8007844c, 1, FE, 0},
    {0x800288f4, 0x800288f8, 0x8008b104, 0, FE, 0},
    {0x800288fc, 0x80028900, 0x800802b8, 1, FE, 0},
    {0x80028904, 0x80028908, 0x80028b8c, 0, FE, 0},
    {0x8002890c, 0x80028910, 0x80028ed0, 1, FE, 0},
    {0x80028934, 0x80028938, 0x800807d8, 3, FE, 0},
    {0x8002893c, 0x80028940, 0x800804e8, 1, FE, 0},
    {0x8002894c, 0x80028950, 0x800807d8, 3, FE, 0},
    {0x80028954, 0x80028958, 0x800804e8, 1, FE, 0},
    {0x8002895c, 0x80028960, 0x8008044c, 1, FE, 0},
    {0x80028974, 0x80028978, 0x8008bfb0, 2, FE, 0},
    {0x800289f4, 0x800289f8, 0x80035d80, 0, FE, 0},
    {0x800289fc, 0x80028a00, 0x800517bc, 0, FE, 0},
    {0x80028a04, 0x80028a08, 0x800673a0, 0, FE, 0},
    {0x80028a0c, 0x80028a10, 0x8008da98, 0, FE, 0},
    {0x80028a14, 0x80028a18, 0x8008acb0, 0, FE, 0},
    {0x80028a48, 0x80028a4c, 0x80036008, 0, FE, 0},
    {0x80028a50, 0x80028a54, 0x80035984, 0, FE, 0},
    {0x80028a58, 0x80028a5c, 0x8008e5a0, 0, FE, 0},
    {0x80028a60, 0x80028a64, 0x80064c90, 0, FE, 0},
    {0x80028a7c, 0x80028a80, 0x8008da5c, 0, FE, 0},
    {0x80028a90, 0x80028a94, 0x80029b20, 0, FE, 0},
    {0x80028aa0, 0x80028aa4, 0x800360d4, 0, FE, 0},
    {0x80028aa8, 0x80028aac, 0x8002f084, 0, FE, 0},
    {0x80028ab0, 0x80028ab4, 0x80028e08, 0, FE, 0},
    {0x80028acc, 0x80028ad0, 0x8007b11c, 2, FE, 0},
    {0x80028ad8, 0x80028adc, 0x80077cd4, 1, FE, 0},
    {0x80028af0, 0x80028af4, 0x80084c44, 2, FE, 0},
    {0x80028af8, 0x80028afc, 0x80084c84, 1, FE, 0},
    {0x80028b00, 0x80028b04, 0x80084c9c, 1, FE, 0},
    {0x80028b08, 0x80028b0c, 0x80028b8c, 0, FE, 0},
    {0x80028b1c, 0x80028b20, 0x8008b1f0, 0, FE, 0},
    {0x80028b24, 0x80028b28, 0x800785f0, 0, FE, 0},
    {0x80028b2c, 0x80028b30, 0x80076110, 0, FE, 0},
    {0x80028b34, 0x80028b38, 0x80051b44, 0, FE, 0},
    {0x80028b44, 0x80028b48, 0x8008a944, 2, FE, 0},
    {0x80028b54, 0x80028b58, 0x800909a8, 3, FE, 0},
    {0x80028b68, 0x80028b6c, 0, 0,
     NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD, 1},
};
static_assert(sizeof Contracts / sizeof Contracts[0] ==
              NBA97_FRONTEND_MAIN_SITE_COUNT);

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

struct Composition {
  Nba97FrontendMainIo fallback;
  void *fallback_user;
  Nba97FrontendDispatchEntryBinding *wrapper;
  Nba97FrontendMainAdapterProgress *progress;
};

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97FrontendMainEvent *event,
             Nba97FrontendMainMachine *machine,
             Nba97FrontendMainCalleeOutcome *outcome) {
  auto &composition = *static_cast<Composition *>(opaque);
  if (!event || !machine || !outcome)
    return 0;
  if (event->site != NBA97_FRONTEND_MAIN_SITE_80028AA0) {
    if (!composition.fallback)
      return 0;
    return composition.fallback(composition.fallback_user, memory, event,
                                machine, outcome);
  }
  auto &out = *composition.progress;
  ++out.wrapper_invocations;
  out.wrapper_event = *event;
  out.wrapper_machine = *machine;
  const Nba97FrontendDispatchEntryCallerEvent converted{
      event->pc,       event->delay_slot_pc, event->entry,
      event->operation, event->invocation,    event->argument_count};
  const std::size_t before = composition.wrapper->invocations;
  const int accepted = nba97_frontend_dispatch_entry_from_frontend_main(
      composition.wrapper, memory, &converted, machine);
  out.wrapper_result = composition.wrapper->result;
  if (composition.wrapper->invocations != before) {
    out.wrapper_progress = composition.wrapper->progress;
    out.wrapper_adapter = composition.wrapper->adapter;
  } else {
    std::memset(&out.wrapper_progress, 0, sizeof out.wrapper_progress);
    std::memset(&out.wrapper_adapter, 0, sizeof out.wrapper_adapter);
  }
  if (accepted == 1) {
    ++out.wrapper_completions;
    *outcome = NBA97_FRONTEND_MAIN_CALLEE_RETURNED;
  }
  return accepted;
}
} // namespace

int nba97_frontend_main_site_contract(
    uint8_t site, Nba97FrontendMainSiteContract *contract) {
  if (!contract || site == NBA97_FRONTEND_MAIN_SITE_NONE ||
      site >= NBA97_FRONTEND_MAIN_SITE_COUNT)
    return 0;
  *contract = Contracts[site];
  return 1;
}

int nba97_frontend_main_with_recovered_dispatch_entry(
    Nba97FrontendMainContext *context,
    Nba97FrontendDispatchEntryBinding *wrapper,
    Nba97FrontendMainProgress *progress,
    Nba97FrontendMainAdapterProgress *adapter) {
  if (!context || !wrapper || !progress || !adapter)
    return NBA97_TEXT_ARGUMENT;
  std::memset(adapter, 0, sizeof *adapter);
  Nba97FrontendMainContext composed = *context;
  Composition composition{context->io, context->user, wrapper, adapter};
  composed.io = dispatch;
  composed.user = &composition;
  const int result = nba97_frontend_main(&composed, progress);
  if (!adapter->wrapper_invocations)
    adapter->wrapper_result = NBA97_TEXT_ARGUMENT;
  return result;
}

int nba97_frontend_main_from_overlay_entry(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97FrontendMainCallerEvent *event,
    Nba97FrontendMainMachine *machine) {
  auto *binding = static_cast<Nba97FrontendMainBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) || event->pc != 0x8007b838u ||
      event->delay_slot_pc != 0x8007b83cu || event->entry != 0x80028800u ||
      event->invocation != 1 || event->argument_count != 0 ||
      event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
      machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].word != 0x8007b840u ||
      machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].known_mask != 0x0f ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97FrontendMainContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_frontend_main_with_recovered_dispatch_entry(
      &context, &binding->wrapper, &binding->progress, &binding->adapter);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
