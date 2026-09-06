#include "game_text_submission_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GameCountdownUiUpdateIo fallback;
  void *user;
  Nba97GameTextSubmissionBinding *binding;
};

bool machineValid(const Nba97GameTextSubmissionMachine &machine) {
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

bool readKnownWord(const Nba97GameTextMemory &memory, std::uint32_t address,
                   std::uint32_t &value) {
  if (address & 3u)
    return false;
  for (std::size_t i = 0; i < memory.count; ++i) {
    const auto &region = memory.region[i];
    const std::uint64_t offset = std::uint64_t(address) - region.base;
    if (address < region.base || offset > region.size ||
        4 > region.size - std::size_t(offset))
      continue;
    value = 0;
    for (unsigned byte = 0; byte < 4; ++byte) {
      if (region.known && region.known[std::size_t(offset) + byte] != 1)
        return false;
      value |= std::uint32_t(region.data[std::size_t(offset) + byte])
               << (byte * 8u);
    }
    return true;
  }
  return false;
}

void toClear(const Nba97GameTextSubmissionMachine &source,
             Nba97GameClearOrderingTableMachine &target) {
  for (unsigned i = 0; i < 32; ++i) {
    target.registers.gpr[i].word = source.registers.gpr[i].word;
    target.registers.gpr[i].known_mask = source.registers.gpr[i].known_mask;
  }
  target.hi.word = source.hi.word;
  target.hi.known_mask = source.hi.known_mask;
  target.lo.word = source.lo.word;
  target.lo.known_mask = source.lo.known_mask;
}

void fromClear(const Nba97GameClearOrderingTableMachine &source,
               Nba97GameTextSubmissionMachine &target) {
  for (unsigned i = 0; i < 32; ++i) {
    target.registers.gpr[i].word = source.registers.gpr[i].word;
    target.registers.gpr[i].known_mask = source.registers.gpr[i].known_mask;
  }
  target.hi.word = source.hi.word;
  target.hi.known_mask = source.hi.known_mask;
  target.lo.word = source.lo.word;
  target.lo.known_mask = source.lo.known_mask;
}

bool clearAssigned(const Nba97GameTextSubmissionEvent &event) {
  return event.kind == NBA97_GAME_TEXT_SUBMISSION_CHILD_80099960 ||
         event.entry == 0x80099960u || event.pc == 0x800310a8u ||
         event.pc == 0x800310b4u || event.delay_slot_pc == 0x800310acu ||
         event.delay_slot_pc == 0x800310b8u;
}

int childDispatch(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97GameTextSubmissionEvent *event,
                  Nba97GameTextSubmissionMachine *machine) {
  auto &binding = *static_cast<Nba97GameTextSubmissionBinding *>(opaque);
  if (!event || !memory || !machine)
    return 0;
  if (!clearAssigned(*event)) {
    if (!binding.io)
      return 0;
    return binding.io(binding.user, memory, event, machine);
  }
  const bool first = event->pc == 0x800310a8u;
  const std::size_t index = first ? 0 : 1;
  const bool exact =
      event->kind == NBA97_GAME_TEXT_SUBMISSION_CHILD_80099960 &&
      event->entry == 0x80099960u &&
      event->pc == (first ? 0x800310a8u : 0x800310b4u) &&
      event->delay_slot_pc == (first ? 0x800310acu : 0x800310b8u) &&
      event->invocation == index + 1 && event->argument_count == 2 &&
      machine->registers.gpr[5].known_mask == 15 &&
      machine->registers.gpr[5].word == 1 &&
      machine->registers.gpr[31].known_mask == 15 &&
      machine->registers.gpr[31].word == (first ? 0x800310b0u : 0x800310bcu);
  if (!exact)
    return 0;
  Nba97GameClearOrderingTableContext context{};
  context.memory = *memory;
  context.operation_budget = binding.clear_operation_budget;
  toClear(*machine, context.machine);
  context.io = binding.clear_io;
  context.user = binding.clear_user;
  context.access_journal = binding.clear_access_journal;
  context.access_journal_capacity = binding.clear_access_journal_capacity;
  ++binding.clear_invocations;
  binding.clear_result[index] =
      nba97_game_clear_ordering_table(&context, &binding.clear_progress[index]);
  fromClear(binding.clear_progress[index].machine, *machine);
  if (binding.clear_result[index] != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding.clear_completions;
  return 1;
}

bool assigned(const Nba97GameCountdownUiUpdateEvent *event,
              const Nba97GameCountdownUiUpdateMachine *machine) {
  return (event &&
          (event->kind == NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80030D18 ||
           event->pc == 0x800329e8u || event->delay_slot_pc == 0x800329ecu ||
           event->entry == 0x80030d18u)) ||
         (machine && machine->registers.gpr[31].known_mask == 15 &&
          machine->registers.gpr[31].word == 0x800329f0u);
}

int parentDispatch(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GameCountdownUiUpdateEvent *event,
                   Nba97GameCountdownUiUpdateMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (assigned(event, machine))
    return nba97_game_text_submission_from_countdown_ui_update(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.user, memory, event, machine);
}
} // namespace

int nba97_game_text_submission_from_countdown_ui_update(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameCountdownUiUpdateEvent *event,
    Nba97GameCountdownUiUpdateMachine *machine) {
  auto *binding = static_cast<Nba97GameTextSubmissionBinding *>(opaque);
  std::uint32_t fifth = 0;
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->kind != NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80030D18 ||
      event->pc != 0x800329e8u || event->delay_slot_pc != 0x800329ecu ||
      event->entry != 0x80030d18u || event->invocation != 1 ||
      event->argument_count != 5 ||
      machine->registers.gpr[4].known_mask != 15 ||
      machine->registers.gpr[4].word != 0xc9u ||
      machine->registers.gpr[5].known_mask != 15 ||
      machine->registers.gpr[5].word != 0x800249fcu ||
      machine->registers.gpr[6].known_mask != 15 ||
      machine->registers.gpr[6].word != 0x1ecu ||
      machine->registers.gpr[7].known_mask != 15 ||
      machine->registers.gpr[7].word != 0x14u ||
      machine->registers.gpr[29].known_mask != 15 ||
      !readKnownWord(*memory, machine->registers.gpr[29].word + 0x10u, fifth) ||
      fifth != 2 || machine->registers.gpr[31].known_mask != 15 ||
      machine->registers.gpr[31].word != 0x800329f0u ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->clear_access_journal &&
       binding->clear_access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97GameTextSubmissionContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = childDispatch;
  context.user = binding;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_game_text_submission(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_countdown_ui_update_with_text_submission(
    const Nba97GameCountdownUiUpdateContext *parent,
    Nba97GameTextSubmissionBinding *binding,
    Nba97GameCountdownUiUpdateProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(&binding->event, 0, sizeof binding->event);
  std::memset(&binding->progress, 0, sizeof binding->progress);
  std::memset(binding->clear_progress, 0, sizeof binding->clear_progress);
  binding->result = NBA97_TEXT_COMPLETE;
  binding->clear_result[0] = NBA97_TEXT_COMPLETE;
  binding->clear_result[1] = NBA97_TEXT_COMPLETE;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameCountdownUiUpdateContext context = *parent;
  context.io = parentDispatch;
  context.user = &run;
  const int result = nba97_game_countdown_ui_update(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x800329e8u)
    return binding->result;
  return result;
}
