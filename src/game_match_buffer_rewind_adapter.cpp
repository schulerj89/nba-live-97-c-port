#include "game_match_buffer_rewind_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct OwnerRun {
  Nba97GameMatchBufferRewindBinding *binding;
};

struct ParentRun {
  Nba97GameMatchStateResetIo fallback;
  void *fallback_user;
  Nba97GameMatchBufferRewindBinding *binding;
};

bool machineValid(const Nba97GameMatchBufferRewindMachine &machine) {
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

bool rewindTarget(const Nba97GameMatchStateResetEvent *event) {
  return event && (event->kind == NBA97_GAME_MATCH_STATE_RESET_80076AD0 ||
                   event->entry == 0x80076ad0u || event->pc == 0x80065ae8u);
}

int dispatchZero(void *opaque, const Nba97GameTextMemory *memory,
                 const Nba97GameMatchBufferRewindEvent *event,
                 Nba97GameMatchBufferRewindMachine *machine) {
  auto &run = *static_cast<OwnerRun *>(opaque);
  return nba97_game_match_buffer_rewind_compose_zero(run.binding, memory, event,
                                                     machine);
}

int dispatchParent(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GameMatchStateResetEvent *event,
                   Nba97GameMatchStateResetMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (rewindTarget(event))
    return nba97_game_match_buffer_rewind_from_match_state_reset(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.fallback_user, memory, event, machine);
}
} // namespace

int nba97_game_match_buffer_rewind_compose_zero(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchBufferRewindEvent *event,
    Nba97GameMatchBufferRewindMachine *machine) {
  auto *binding = static_cast<Nba97GameMatchBufferRewindBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->kind != NBA97_GAME_MATCH_BUFFER_REWIND_ZERO ||
      event->pc != 0x80076af8u || event->delay_slot_pc != 0x80076afcu ||
      event->entry != 0x800a3a74u || event->invocation != 1 ||
      event->argument_count != 2 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word != 0x80076b00u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word != 0x800f1918u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word != 4) {
    if (binding)
      binding->nested_result = binding->zero_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->zero_invocations;
  auto &gpr = machine->registers.gpr;
  const auto incoming_v0 = gpr[NBA97_MATCH_INITIALIZE_V0];
  const auto incoming_t0 = gpr[NBA97_MATCH_INITIALIZE_T0];
  const auto incoming_t1 = gpr[NBA97_MATCH_INITIALIZE_T0 + 1];
  gpr[NBA97_MATCH_INITIALIZE_AT] = {0, 15};
  gpr[NBA97_MATCH_INITIALIZE_A2] = {0, 15};
  gpr[NBA97_MATCH_INITIALIZE_T0 + 2] = {0, 15};
  Nba97GameMemoryZeroContext context{
      *memory,
      binding->zero_operation_budget,
      0x800f1918u,
      4,
      incoming_v0.word,
      static_cast<std::uint8_t>(incoming_v0.known_mask == 15)};
  binding->zero_result =
      nba97_game_memory_zero(&context, &binding->zero_progress);
  gpr[NBA97_MATCH_INITIALIZE_V0] = incoming_v0;
  gpr[NBA97_MATCH_INITIALIZE_A0] = {binding->zero_progress.working_destination,
                                    15};
  gpr[NBA97_MATCH_INITIALIZE_A1] = {binding->zero_progress.working_count, 15};
  if (binding->zero_progress.stores) {
    gpr[NBA97_MATCH_INITIALIZE_T0] = {4, 15};
    gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = {0, 15};
  } else {
    gpr[NBA97_MATCH_INITIALIZE_T0] = incoming_t0;
    gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = incoming_t1;
  }
  binding->nested_result = binding->zero_result;
  if (binding->zero_result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->zero_completions;
  return 1;
}

int nba97_game_match_buffer_rewind_from_match_state_reset(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchStateResetEvent *event,
    Nba97GameMatchStateResetMachine *machine) {
  auto *binding = static_cast<Nba97GameMatchBufferRewindBinding *>(opaque);
  if (!binding || !memory || !event || !machine ||
      event->kind != NBA97_GAME_MATCH_STATE_RESET_80076AD0 ||
      event->pc != 0x80065ae8u || event->delay_slot_pc != 0x80065aecu ||
      event->entry != 0x80076ad0u || event->invocation != 1 ||
      event->argument_count != 0 || !machineValid(*machine) ||
      !memoryValid(*memory) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word != 0x80065af0u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->event = *event;
  OwnerRun run{binding};
  Nba97GameMatchBufferRewindContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = dispatchZero;
  context.user = &run;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->nested_result = NBA97_TEXT_COMPLETE;
  binding->result =
      nba97_game_match_buffer_rewind(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_IO_REFUSED &&
      binding->nested_result != NBA97_TEXT_COMPLETE)
    binding->result = binding->nested_result;
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_match_state_reset_with_match_buffer_rewind(
    const Nba97GameMatchStateResetContext *parent,
    Nba97GameMatchBufferRewindBinding *binding,
    Nba97GameMatchStateResetProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(&binding->event, 0, sizeof binding->event);
  std::memset(&binding->progress, 0, sizeof binding->progress);
  std::memset(&binding->zero_progress, 0, sizeof binding->zero_progress);
  binding->result = NBA97_TEXT_COMPLETE;
  binding->nested_result = NBA97_TEXT_COMPLETE;
  binding->zero_result = NBA97_TEXT_COMPLETE;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameMatchStateResetContext context = *parent;
  context.io = dispatchParent;
  context.user = &run;
  int result = nba97_game_match_state_reset(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x80065ae8u)
    return binding->result;
  return result;
}

int nba97_game_match_buffer_rewind_from_match_buffer_initialize(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchBufferInitializeEvent *event,
    Nba97GameMatchBufferInitializeMachine *machine) {
  auto *binding = static_cast<Nba97GameMatchBufferRewindBinding *>(opaque);
  if (!binding || !memory || !event || !machine ||
      event->kind != NBA97_GAME_MATCH_BUFFER_INITIALIZE_CHILD_80076AD0 ||
      event->entry != 0x80076ad0u || event->pc != 0x80064370u ||
      event->delay_slot_pc != 0x80064374u || event->invocation != 1 ||
      event->argument_count != 0 ||
      machine->registers.gpr[31].word != 0x80064378u ||
      machine->registers.gpr[31].known_mask != 15 || !memoryValid(*memory) ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  Nba97GameMatchBufferRewindContext context{};
  context.machine.registers = machine->registers;
  context.machine.hi = {machine->hi.word, machine->hi.known_mask};
  context.machine.lo = {machine->lo.word, machine->lo.known_mask};
  if (!machineValid(context.machine)) {
    binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->buffer_event = *event;
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.io = nba97_game_match_buffer_rewind_compose_zero;
  context.user = binding;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->nested_result = NBA97_TEXT_COMPLETE;
  binding->result =
      nba97_game_match_buffer_rewind(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_IO_REFUSED &&
      binding->nested_result != NBA97_TEXT_COMPLETE)
    binding->result = binding->nested_result;
  machine->registers = binding->progress.machine.registers;
  machine->hi = {binding->progress.machine.hi.word,
                 binding->progress.machine.hi.known_mask};
  machine->lo = {binding->progress.machine.lo.word,
                 binding->progress.machine.lo.known_mask};
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
