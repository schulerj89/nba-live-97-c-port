#include "game_match_state_reset_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ChildRun {
  Nba97GameMatchStateResetBinding *binding;
};

struct ParentRun {
  Nba97GameMatchInitializeIo fallback;
  void *fallback_user;
  Nba97GameMatchStateResetBinding *binding;
};

bool registersValid(const Nba97GameMatchInitializeRegisters &registers) {
  if (registers.gpr[0].word || registers.gpr[0].known_mask != 15)
    return false;
  for (unsigned i = 0; i < 32; ++i)
    if (registers.gpr[i].known_mask > 15)
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

bool resetTarget(const Nba97GameMatchInitializeEvent *event) {
  return event && (event->kind == NBA97_MATCH_INITIALIZE_CHILD_800659F0 ||
                   event->pc == 0x8002dbf8u || event->entry == 0x800659f0u);
}

int composeRoster(ChildRun &run, const Nba97GameTextMemory *memory,
                  Nba97GameMatchStateResetMachine *machine) {
  auto &binding = *run.binding;
  ++binding.roster_invocations;
  Nba97GameRosterBindingsContext context{};
  context.memory = *memory;
  context.operation_budget = binding.roster_operation_budget;
  context.registers = machine->registers;
  context.access_journal = binding.roster_journal;
  context.access_journal_capacity = binding.roster_journal_capacity;
  binding.roster_result =
      nba97_game_roster_bindings(&context, &binding.roster_progress);
  if (registersValid(binding.roster_progress.registers))
    machine->registers = binding.roster_progress.registers;
  binding.nested_result = binding.roster_result;
  return binding.roster_result == NBA97_TEXT_COMPLETE;
}

int composeZero(ChildRun &run, const Nba97GameTextMemory *memory,
                const Nba97GameMatchStateResetEvent *event,
                Nba97GameMatchStateResetMachine *machine) {
  auto &binding = *run.binding;
  ++binding.zero_invocations;
  if (!event->invocation || event->invocation > 4) {
    binding.nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  std::size_t index = event->invocation - 1;
  auto &gpr = machine->registers.gpr;
  /* All four source counts are known positive large-path values. A callback
   * may make live s0, hence a0, partial. In that case the original zero core
   * reaches its first SWR with this exact scratch prefix but cannot form a
   * mapped address; the narrow child is deliberately not invoked. */
  if (gpr[NBA97_MATCH_INITIALIZE_A1].known_mask != 15 ||
      (gpr[NBA97_MATCH_INITIALIZE_A1].word & 0x80000000u) ||
      gpr[NBA97_MATCH_INITIALIZE_A1].word < 4) {
    binding.nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  gpr[NBA97_MATCH_INITIALIZE_AT] = {0, 15};
  gpr[NBA97_MATCH_INITIALIZE_A2] = {0, 15};
  gpr[NBA97_MATCH_INITIALIZE_T0 + 2] = {0, 15};
  if (gpr[NBA97_MATCH_INITIALIZE_A0].known_mask != 15) {
    binding.nested_result = NBA97_TEXT_UNKNOWN;
    return 0;
  }

  const auto incoming_v0 = gpr[NBA97_MATCH_INITIALIZE_V0];
  const auto incoming_t0 = gpr[NBA97_MATCH_INITIALIZE_T0];
  const auto incoming_t1 = gpr[NBA97_MATCH_INITIALIZE_T0 + 1];
  const std::uint32_t initial_a0 = gpr[NBA97_MATCH_INITIALIZE_A0].word;
  Nba97GameMemoryZeroContext context{
      *memory,
      binding.zero_operation_budget[index],
      initial_a0,
      gpr[NBA97_MATCH_INITIALIZE_A1].word,
      incoming_v0.word,
      static_cast<std::uint8_t>(incoming_v0.known_mask == 15)};
  binding.zero_result[index] =
      nba97_game_memory_zero(&context, &binding.zero_progress[index]);
  const auto &progress = binding.zero_progress[index];
  gpr[NBA97_MATCH_INITIALIZE_A0] = {progress.working_destination, 15};
  gpr[NBA97_MATCH_INITIALIZE_A1] = {progress.working_count, 15};
  gpr[NBA97_MATCH_INITIALIZE_V0] = incoming_v0;
  if (progress.stores) {
    std::uint32_t alignment = initial_a0 & 3u;
    gpr[NBA97_MATCH_INITIALIZE_T0] = {4u - alignment, 15};
    gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = {alignment, 15};
  } else {
    gpr[NBA97_MATCH_INITIALIZE_T0] = incoming_t0;
    gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = incoming_t1;
  }
  binding.nested_result = binding.zero_result[index];
  return binding.zero_result[index] == NBA97_TEXT_COMPLETE;
}

int dispatchChild(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97GameMatchStateResetEvent *event,
                  Nba97GameMatchStateResetMachine *machine) {
  auto &run = *static_cast<ChildRun *>(opaque);
  auto &binding = *run.binding;
  binding.nested_result = NBA97_TEXT_COMPLETE;
  if (event->kind == NBA97_GAME_MATCH_STATE_RESET_ZERO &&
      event->entry == 0x800a3a74u)
    return nba97_game_match_state_reset_compose_zero(run.binding, memory, event,
                                                     machine);
  if (event->kind == NBA97_GAME_MATCH_STATE_RESET_80063D58 &&
      event->entry == 0x80063d58u)
    return composeRoster(run, memory, machine);
  if (!binding.io) {
    binding.nested_result = NBA97_TEXT_IO_REFUSED;
    return 0;
  }
  int accepted = binding.io(binding.user, memory, event, machine);
  if (accepted == 1)
    ++binding.unresolved_callbacks_completed;
  else
    binding.nested_result = NBA97_TEXT_IO_REFUSED;
  return accepted;
}

int dispatchParent(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GameMatchInitializeEvent *event,
                   Nba97GameMatchInitializeRegisters *registers) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (resetTarget(event))
    return nba97_game_match_state_reset_from_match_initialize(
        run.binding, memory, event, registers);
  if (!run.fallback)
    return 0;
  return run.fallback(run.fallback_user, memory, event, registers);
}
} // namespace

int nba97_game_match_state_reset_compose_zero(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchStateResetEvent *event,
    Nba97GameMatchStateResetMachine *machine) {
  auto *binding = static_cast<Nba97GameMatchStateResetBinding *>(opaque);
  static constexpr std::uint32_t pcs[4] = {0x80065a0cu, 0x80065a18u,
                                           0x80065a24u, 0x80065a30u};
  static constexpr std::uint32_t sizes[4] = {0x4b0u, 0x1320u, 0xc4u, 0xc4u};
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      event->kind != NBA97_GAME_MATCH_STATE_RESET_ZERO ||
      event->entry != 0x800a3a74u || !event->invocation ||
      event->invocation > 4 || event->pc != pcs[event->invocation - 1] ||
      event->delay_slot_pc != event->pc + 4u || event->argument_count != 2 ||
      !registersValid(machine->registers) || machine->hi.known_mask > 15 ||
      machine->lo.known_mask > 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          event->pc + 8u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word !=
          sizes[event->invocation - 1]) {
    if (binding)
      binding->nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ChildRun run{binding};
  return composeZero(run, memory, event, machine);
}

int nba97_game_match_state_reset_from_match_initialize(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchInitializeEvent *event,
    Nba97GameMatchInitializeRegisters *registers) {
  auto *binding = static_cast<Nba97GameMatchStateResetBinding *>(opaque);
  if (!binding || !memory || !event || !registers || event->pc != 0x8002dbf8u ||
      event->delay_slot_pc != 0x8002dbfcu || event->entry != 0x800659f0u ||
      event->kind != NBA97_MATCH_INITIALIZE_CHILD_800659F0 ||
      event->argument_count != 0 || !registersValid(*registers) ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].word != 0x8002dc00u ||
      !memoryValid(*memory) ||
      (!binding->access_journal && binding->access_journal_capacity) ||
      (!binding->roster_journal && binding->roster_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->event = *event;
  Nba97GameMatchStateResetMachine machine{};
  machine.registers = *registers;
  if (binding->hi_lo_provider) {
    int provided = binding->hi_lo_provider(binding->hi_lo_user, memory, event,
                                           &machine.hi, &machine.lo);
    if (provided != 1) {
      binding->result = NBA97_TEXT_IO_REFUSED;
      return 0;
    }
  }
  if (machine.hi.known_mask > 15 || machine.lo.known_mask > 15) {
    binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ChildRun child{binding};
  Nba97GameMatchStateResetContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = machine;
  context.io = dispatchChild;
  context.user = &child;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->nested_result = NBA97_TEXT_COMPLETE;
  binding->result = nba97_game_match_state_reset(&context, &binding->progress);
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

int nba97_game_match_initialize_with_state_reset(
    const Nba97GameMatchInitializeContext *parent,
    Nba97GameMatchStateResetBinding *binding,
    Nba97GameMatchInitializeProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  binding->invocations = 0;
  binding->completions = 0;
  binding->unresolved_callbacks_completed = 0;
  binding->zero_invocations = 0;
  binding->roster_invocations = 0;
  binding->result = NBA97_TEXT_ARGUMENT;
  binding->nested_result = NBA97_TEXT_COMPLETE;
  binding->roster_result = NBA97_TEXT_COMPLETE;
  std::memset(&binding->event, 0, sizeof binding->event);
  std::memset(&binding->progress, 0, sizeof binding->progress);
  std::memset(binding->zero_progress, 0, sizeof binding->zero_progress);
  std::memset(binding->zero_result, 0, sizeof binding->zero_result);
  std::memset(&binding->roster_progress, 0, sizeof binding->roster_progress);
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameMatchInitializeContext context = *parent;
  context.io = dispatchParent;
  context.user = &run;
  int result = nba97_game_match_initialize(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x8002dbf8u &&
      binding->invocations)
    return binding->result;
  return result;
}
