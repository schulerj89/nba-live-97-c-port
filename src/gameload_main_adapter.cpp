#include "gameload_main_adapter.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::uint8_t GL = NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD;
constexpr Nba97GameloadMainSiteContract Sites[] = {
    {0, 0, 0, 0, 0, 0},
    {0x801e1374u, 0x801e1378u, 0x801e14b8u, 0, GL, 0},
    {0x801e137cu, 0x801e1380u, 0x801e000cu, 0, GL, 0},
    {0x801e1384u, 0x801e1388u, 0x801e059cu, 0, GL, 0},
    {0x801e1394u, 0x801e1398u, 0x801e0938u, 2, GL, 0},
    {0x801e13b0u, 0x801e13b4u, 0x801e1344u, 3, GL, 0},
    {0x801e13c4u, 0x801e13c8u, 0x801e1300u, 2, GL, 0},
    {0x801e13ccu, 0x801e13d0u, 0x801e1670u, 0, GL, 0},
    {0x801e13e0u, 0x801e13e4u, 0x801e1344u, 3, GL, 0},
    {0x801e13f4u, 0x801e13f8u, 0, 0,
     NBA97_GAMELOAD_MAIN_PROGRAM_GAMEONLY, 1}};
static_assert(sizeof Sites / sizeof Sites[0] ==
              NBA97_GAMELOAD_MAIN_SITE_COUNT);

constexpr Nba97GameloadMainParentContract Parent{
    0x801e14acu,
    0x801e14b0u,
    0x801e136cu,
    0x801e14b4u,
    2081,
    1,
    NBA97_GAMELOAD_ENTRY_SITE_801E14AC,
    0,
    NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD};

bool machineValid(const Nba97GameloadEntryMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 0x0f ||
      machine.hi.known_mask > 0x0f || machine.lo.known_mask > 0x0f)
    return false;
  for (unsigned i = 0; i < NBA97_GAMELOAD_ENTRY_REGISTER_COUNT; ++i)
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

bool eventValid(const Nba97GameloadEntryEvent &event,
                const Nba97GameloadEntryMachine &machine) {
  return event.pc == Parent.pc &&
         event.delay_slot_pc == Parent.delay_slot_pc &&
         event.entry == Parent.target && event.operation == Parent.operation &&
         event.invocation == Parent.invocation && event.site == Parent.site &&
         event.argument_count == Parent.argument_count &&
         event.target_program == Parent.target_program &&
         machine.registers.gpr[NBA97_GAMELOAD_ENTRY_RA].word ==
             Parent.return_address &&
         machine.registers.gpr[NBA97_GAMELOAD_ENTRY_RA].known_mask == 0x0f;
}

void toMain(const Nba97GameloadEntryMachine &source,
            Nba97GameloadMainMachine &destination) {
  for (unsigned i = 0; i < NBA97_GAMELOAD_MAIN_REGISTER_COUNT; ++i) {
    destination.registers.gpr[i].word = source.registers.gpr[i].word;
    destination.registers.gpr[i].known_mask =
        source.registers.gpr[i].known_mask;
  }
  destination.hi.word = source.hi.word;
  destination.hi.known_mask = source.hi.known_mask;
  destination.lo.word = source.lo.word;
  destination.lo.known_mask = source.lo.known_mask;
}

void toEntry(const Nba97GameloadMainMachine &source,
             Nba97GameloadEntryMachine &destination) {
  for (unsigned i = 0; i < NBA97_GAMELOAD_ENTRY_REGISTER_COUNT; ++i) {
    destination.registers.gpr[i].word = source.registers.gpr[i].word;
    destination.registers.gpr[i].known_mask =
        source.registers.gpr[i].known_mask;
  }
  destination.hi.word = source.hi.word;
  destination.hi.known_mask = source.hi.known_mask;
  destination.lo.word = source.lo.word;
  destination.lo.known_mask = source.lo.known_mask;
}

struct Composition {
  Nba97GameloadEntryIo fallback;
  void *fallback_user;
  Nba97GameloadMainBinding *binding;
  Nba97GameloadMainAdapterProgress *out;
};

int composedIo(void *opaque, const Nba97GameTextMemory *memory,
               const Nba97GameloadEntryEvent *event,
               Nba97GameloadEntryMachine *machine,
               Nba97GameloadEntryCalleeOutcome *outcome) {
  auto &composition = *static_cast<Composition *>(opaque);
  if (event && event->site == NBA97_GAMELOAD_ENTRY_SITE_801E14AC) {
    const std::size_t before = composition.binding->invocations;
    const int accepted = nba97_gameload_main_from_entry(
        composition.binding, memory, event, machine, outcome);
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
                                    machine, outcome)
             : 0;
}
} // namespace

int nba97_gameload_main_site_contract(
    uint8_t site, Nba97GameloadMainSiteContract *contract) {
  if (!contract || site == NBA97_GAMELOAD_MAIN_SITE_NONE ||
      site >= NBA97_GAMELOAD_MAIN_SITE_COUNT)
    return 0;
  *contract = Sites[site];
  return 1;
}

int nba97_gameload_main_parent_contract(
    Nba97GameloadMainParentContract *contract) {
  if (!contract)
    return 0;
  *contract = Parent;
  return 1;
}

int nba97_gameload_main_from_entry(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameloadEntryEvent *event, Nba97GameloadEntryMachine *machine,
    Nba97GameloadEntryCalleeOutcome *outcome) {
  auto *binding = static_cast<Nba97GameloadMainBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !outcome ||
      !memoryValid(*memory) || !machineValid(*machine) ||
      !eventValid(*event, *machine) ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->instruction_journal && binding->instruction_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->parent_event = *event;
  toMain(*machine, binding->parent_machine);
  Nba97GameloadMainContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = binding->parent_machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.instruction_journal = binding->instruction_journal;
  context.instruction_journal_capacity = binding->instruction_journal_capacity;
  binding->result = nba97_gameload_main(&context, &binding->progress);
  toEntry(binding->progress.machine, *machine);
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  *outcome = binding->progress.transferred
                 ? NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED
                 : NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED;
  return 1;
}

int nba97_gameload_entry_with_recovered_main(
    Nba97GameloadEntryContext *context, Nba97GameloadMainBinding *binding,
    Nba97GameloadEntryProgress *progress,
    Nba97GameloadMainAdapterProgress *adapter) {
  if (!adapter)
    return NBA97_TEXT_ARGUMENT;
  std::memset(adapter, 0, sizeof *adapter);
  if (!context || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  Composition composition{context->io, context->user, binding, adapter};
  Nba97GameloadEntryContext composed = *context;
  composed.io = composedIo;
  composed.user = &composition;
  return nba97_gameload_entry(&composed, progress);
}
