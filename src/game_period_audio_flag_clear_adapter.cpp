#include "game_period_audio_flag_clear_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
bool validRegisters(const Nba97GameFirstPeriodStartupRegisters &registers) {
  if (registers.gpr[0].word != 0u || registers.gpr[0].known_mask != 15u)
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (registers.gpr[index].known_mask > 15u)
      return false;
  return true;
}

bool identifiesTarget(const Nba97GameFirstPeriodStartupEvent &event,
                      const Nba97GameFirstPeriodStartupRegisters &registers) {
  return event.kind == NBA97_GAME_FIRST_PERIOD_STARTUP_2A244 ||
         event.entry == UINT32_C(0x8002a244) ||
         event.pc == UINT32_C(0x80067400) ||
         event.delay_slot_pc == UINT32_C(0x80067404) ||
         (registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15u &&
          registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
              UINT32_C(0x80067408));
}

bool exactTarget(const Nba97GameFirstPeriodStartupEvent &event,
                 const Nba97GameFirstPeriodStartupRegisters &registers) {
  return event.kind == NBA97_GAME_FIRST_PERIOD_STARTUP_2A244 &&
         event.entry == UINT32_C(0x8002a244) &&
         event.pc == UINT32_C(0x80067400) &&
         event.delay_slot_pc == UINT32_C(0x80067404) && event.operation == 3u &&
         event.argument_count == 0u &&
         registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15u &&
         registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == UINT32_C(0x80067408);
}

void copyToOwner(const Nba97GameFirstPeriodStartupRegisters &source,
                 Nba97GamePeriodAudioFlagClearMachine &destination) {
  destination.registers = source;
  destination.hi = {0u, 0u};
  destination.lo = {0u, 0u};
}
} // namespace

extern "C" void nba97_game_period_audio_flag_clear_binding_init(
    Nba97GamePeriodAudioFlagClearBinding *binding, std::size_t operationBudget,
    Nba97GamePeriodAudioFlagClearAccess *journal, std::size_t journalCapacity,
    Nba97GameFirstPeriodStartupIo fallback, void *fallbackUser) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operationBudget;
  binding->access_journal = journal;
  binding->access_journal_capacity = journalCapacity;
  binding->fallback = fallback;
  binding->fallback_user = fallbackUser;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_period_audio_flag_clear_from_first_period(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameFirstPeriodStartupEvent *event,
    Nba97GameFirstPeriodStartupRegisters *registers) {
  auto *binding = static_cast<Nba97GamePeriodAudioFlagClearBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      registers == nullptr)
    return 0;
  if (!identifiesTarget(*event, *registers)) {
    if (binding->fallback == nullptr)
      return 0;
    return binding->fallback(binding->fallback_user, memory, event, registers);
  }
  binding->result = NBA97_TEXT_ARGUMENT;
  if (!exactTarget(*event, *registers) || !validRegisters(*registers) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;

  Nba97GamePeriodAudioFlagClearContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  copyToOwner(*registers, context.machine);
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->event = *event;
  binding->result =
      nba97_game_period_audio_flag_clear(&context, &binding->progress);
  if (validRegisters(binding->progress.machine.registers))
    *registers = binding->progress.machine.registers;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

extern "C" int nba97_game_first_period_startup_with_audio_flag_clear(
    const Nba97GameFirstPeriodStartupContext *parent,
    Nba97GamePeriodAudioFlagClearBinding *binding,
    Nba97GameFirstPeriodStartupProgress *progress) {
  if (parent == nullptr || binding == nullptr || progress == nullptr)
    return NBA97_TEXT_ARGUMENT;
  const std::size_t operationBudget = binding->operation_budget;
  auto *const journal = binding->access_journal;
  const std::size_t journalCapacity = binding->access_journal_capacity;
  nba97_game_period_audio_flag_clear_binding_init(binding, operationBudget,
                                                  journal, journalCapacity,
                                                  parent->io, parent->user);
  Nba97GameFirstPeriodStartupContext context = *parent;
  context.io = nba97_game_period_audio_flag_clear_from_first_period;
  context.user = binding;
  const int result = nba97_game_first_period_startup(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED &&
      progress->stopped_pc == UINT32_C(0x80067400) &&
      binding->invocations != 0u)
    return binding->result;
  return result;
}
