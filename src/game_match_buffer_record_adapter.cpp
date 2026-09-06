#include "game_match_buffer_record_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct OwnerRun {
  Nba97GameMatchBufferRecordBinding *binding;
};

struct ParentRun {
  Nba97GamePeriodStartupIo fallback;
  void *fallbackUser;
  Nba97GameMatchBufferRecordBinding *binding;
};

bool memoryValid(const Nba97GameTextMemory &memory) {
  if (!memory.region && memory.count)
    return false;
  for (std::size_t i = 0; i < memory.count; ++i) {
    const auto &a = memory.region[i];
    if (!a.data || !a.size || std::uint64_t(a.size) > UINT64_C(0x100000000) ||
        std::uint64_t(a.base) + std::uint64_t(a.size) > UINT64_C(0x100000000))
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

bool machineValid(const Nba97GameMatchBufferRecordMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 15 || machine.hi.known_mask > 15 ||
      machine.lo.known_mask > 15)
    return false;
  for (unsigned i = 0; i < 32; ++i)
    if (machine.registers.gpr[i].known_mask > 15)
      return false;
  return true;
}

bool registersValid(const Nba97GamePeriodStartupRegisters &registers) {
  if (registers.gpr[0].word || registers.gpr[0].known_mask != 15)
    return false;
  for (unsigned i = 0; i < 32; ++i)
    if (registers.gpr[i].known_mask > 15)
      return false;
  return true;
}

bool parentTarget(const Nba97GamePeriodStartupEvent &event,
                  const Nba97GamePeriodStartupRegisters &registers) {
  return event.kind == NBA97_GAME_PERIOD_STARTUP_76B3C ||
         event.entry == 0x80076b3cu || event.pc == 0x800674f8u ||
         event.pc == 0x80067508u || event.delay_slot_pc == 0x800674fcu ||
         event.delay_slot_pc == 0x8006750cu ||
         (registers.gpr[31].known_mask == 15 &&
          (registers.gpr[31].word == 0x80067500u ||
           registers.gpr[31].word == 0x80067510u));
}

bool parentEvent(const Nba97GamePeriodStartupEvent &event) {
  const bool first =
      event.pc == 0x800674f8u && event.delay_slot_pc == 0x800674fcu;
  const bool second =
      event.pc == 0x80067508u && event.delay_slot_pc == 0x8006750cu;
  return (first || second) && event.entry == 0x80076b3cu &&
         event.kind == NBA97_GAME_PERIOD_STARTUP_76B3C &&
         event.argument_count == 0;
}

int composeRewind(OwnerRun &run, const Nba97GameTextMemory *memory,
                  Nba97GameMatchBufferRecordMachine *machine) {
  auto &binding = *run.binding;
  ++binding.rewind_invocations;
  std::memset(&binding.rewind.progress, 0, sizeof binding.rewind.progress);
  std::memset(&binding.rewind.zero_progress, 0,
              sizeof binding.rewind.zero_progress);
  binding.rewind.operation_budget = binding.rewind_operation_budget;
  binding.rewind.zero_operation_budget = binding.zero_operation_budget;
  binding.rewind.access_journal = binding.rewind_journal;
  binding.rewind.access_journal_capacity = binding.rewind_journal_capacity;
  binding.rewind.zero_invocations = 0;
  binding.rewind.zero_completions = 0;
  binding.rewind.nested_result = NBA97_TEXT_COMPLETE;
  Nba97GameMatchBufferRewindContext context{};
  context.memory = *memory;
  context.operation_budget = binding.rewind_operation_budget;
  context.machine = *machine;
  context.io = nba97_game_match_buffer_rewind_compose_zero;
  context.user = &binding.rewind;
  context.access_journal = binding.rewind_journal;
  context.access_journal_capacity = binding.rewind_journal_capacity;
  binding.rewind.result =
      nba97_game_match_buffer_rewind(&context, &binding.rewind.progress);
  if (binding.rewind.result == NBA97_TEXT_IO_REFUSED &&
      binding.rewind.nested_result != NBA97_TEXT_COMPLETE)
    binding.rewind.result = binding.rewind.nested_result;
  *machine = binding.rewind.progress.machine;
  binding.nested_result = binding.rewind.result;
  return binding.rewind.result == NBA97_TEXT_COMPLETE;
}

int dispatchOwner(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97GameMatchBufferRecordEvent *event,
                  Nba97GameMatchBufferRecordMachine *machine) {
  auto &run = *static_cast<OwnerRun *>(opaque);
  auto &binding = *run.binding;
  if (!event || !memory || !machine || !machineValid(*machine)) {
    binding.nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  if (event->kind == NBA97_GAME_MATCH_BUFFER_RECORD_REWIND_80076AD0 ||
      event->entry == 0x80076ad0u || event->pc == 0x80076b50u) {
    if (event->kind != NBA97_GAME_MATCH_BUFFER_RECORD_REWIND_80076AD0 ||
        event->entry != 0x80076ad0u || event->pc != 0x80076b50u ||
        event->delay_slot_pc != 0x80076b54u || event->argument_count != 0 ||
        event->invocation != 1 || machine->registers.gpr[31].known_mask != 15 ||
        machine->registers.gpr[31].word != 0x80076b58u) {
      binding.nested_result = NBA97_TEXT_ARGUMENT;
      return 0;
    }
    return composeRewind(run, memory, machine);
  }
  if (event->kind == NBA97_GAME_MATCH_BUFFER_RECORD_COMPRESS_800767FC ||
      event->entry == 0x800767fcu || event->pc == 0x80076e58u) {
    ++binding.compression_invocations;
    if (event->kind != NBA97_GAME_MATCH_BUFFER_RECORD_COMPRESS_800767FC ||
        event->entry != 0x800767fcu || event->pc != 0x80076e58u ||
        event->delay_slot_pc != 0x80076e5cu || event->argument_count != 4 ||
        event->invocation != 1 || machine->registers.gpr[31].known_mask != 15 ||
        machine->registers.gpr[31].word != 0x80076e60u ||
        machine->registers.gpr[7].known_mask != 15 ||
        machine->registers.gpr[7].word != 0x82u) {
      binding.nested_result = NBA97_TEXT_ARGUMENT;
      return 0;
    }
    if (!binding.io) {
      binding.nested_result = NBA97_TEXT_IO_REFUSED;
      return 0;
    }
    const int accepted = binding.io(binding.user, memory, event, machine);
    if (accepted != 1)
      binding.nested_result = NBA97_TEXT_IO_REFUSED;
    return accepted;
  }
  binding.nested_result = NBA97_TEXT_ARGUMENT;
  return 0;
}

int dispatchParent(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GamePeriodStartupEvent *event,
                   Nba97GamePeriodStartupRegisters *registers) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (event && registers && parentTarget(*event, *registers))
    return nba97_game_match_buffer_record_from_period_startup(
        run.binding, memory, event, registers);
  if (!run.fallback)
    return 0;
  return run.fallback(run.fallbackUser, memory, event, registers);
}
} // namespace

void nba97_game_match_buffer_record_binding_init(
    Nba97GameMatchBufferRecordBinding *binding, std::size_t operationBudget,
    std::size_t rewindOperationBudget, std::size_t zeroOperationBudget) {
  if (!binding)
    return;
  std::memset(binding, 0, sizeof *binding);
  binding->operation_budget = operationBudget;
  binding->rewind_operation_budget = rewindOperationBudget;
  binding->zero_operation_budget = zeroOperationBudget;
  binding->result = NBA97_TEXT_ARGUMENT;
  binding->nested_result = NBA97_TEXT_COMPLETE;
}

int nba97_game_match_buffer_record_from_period_startup(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GamePeriodStartupEvent *event,
    Nba97GamePeriodStartupRegisters *registers) {
  auto *binding = static_cast<Nba97GameMatchBufferRecordBinding *>(opaque);
  if (!binding || !memory || !event || !registers || !parentEvent(*event) ||
      !registersValid(*registers) || !memoryValid(*memory) ||
      registers->gpr[31].known_mask != 15 ||
      registers->gpr[31].word != event->pc + 8u ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->rewind_journal && binding->rewind_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  if (binding->invocations >= 2) {
    binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  binding->event[binding->invocations] = *event;
  ++binding->invocations;
  OwnerRun run{binding};
  Nba97GameMatchBufferRecordContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine.registers = *registers;
  context.machine.hi = {0, 0};
  context.machine.lo = {0, 0};
  context.io = dispatchOwner;
  context.user = &run;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->nested_result = NBA97_TEXT_COMPLETE;
  binding->result =
      nba97_game_match_buffer_record(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_IO_REFUSED &&
      binding->nested_result != NBA97_TEXT_COMPLETE)
    binding->result = binding->nested_result;
  if (registersValid(binding->progress.machine.registers))
    *registers = binding->progress.machine.registers;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_period_startup_with_match_buffer_record(
    const Nba97GamePeriodStartupContext *parent,
    Nba97GameMatchBufferRecordBinding *binding,
    Nba97GamePeriodStartupProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  const auto operationBudget = binding->operation_budget;
  const auto rewindOperationBudget = binding->rewind_operation_budget;
  const auto zeroOperationBudget = binding->zero_operation_budget;
  const auto io = binding->io;
  void *const user = binding->user;
  auto *const journal = binding->access_journal;
  const auto journalCapacity = binding->access_journal_capacity;
  auto *const rewindJournal = binding->rewind_journal;
  const auto rewindJournalCapacity = binding->rewind_journal_capacity;
  nba97_game_match_buffer_record_binding_init(
      binding, operationBudget, rewindOperationBudget, zeroOperationBudget);
  binding->io = io;
  binding->user = user;
  binding->access_journal = journal;
  binding->access_journal_capacity = journalCapacity;
  binding->rewind_journal = rewindJournal;
  binding->rewind_journal_capacity = rewindJournalCapacity;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GamePeriodStartupContext context = *parent;
  context.io = dispatchParent;
  context.user = &run;
  const int result = nba97_game_period_startup(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED &&
      (progress->stopped_pc == 0x800674f8u ||
       progress->stopped_pc == 0x80067508u) &&
      binding->invocations)
    return binding->result;
  return result;
}
