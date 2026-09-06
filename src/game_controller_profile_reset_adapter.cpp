#include "game_controller_profile_reset_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GameMatchStateResetIo fallback;
  void *fallback_user;
  Nba97GameControllerProfileResetBinding *binding;
};

struct ChildRun {
  Nba97GameControllerProfileResetBinding *binding;
  std::size_t zeroInvocations;
};

bool memoryValid(const Nba97GameTextMemory &memory) {
  if (!memory.region && memory.count)
    return false;
  for (std::size_t i = 0; i < memory.count; ++i) {
    const auto &a = memory.region[i];
    if (!a.data || !a.size ||
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

template <class Machine> bool machineValid(const Machine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 15 || machine.hi.known_mask > 15 ||
      machine.lo.known_mask > 15)
    return false;
  for (const auto &word : machine.registers.gpr)
    if (word.known_mask > 15)
      return false;
  return true;
}

void copyToChild(const Nba97GameMatchStateResetMachine &source,
                 Nba97GameControllerProfileResetMachine &target) {
  for (unsigned i = 0; i < 32; ++i) {
    target.registers.gpr[i].word = source.registers.gpr[i].word;
    target.registers.gpr[i].known_mask = source.registers.gpr[i].known_mask;
  }
  target.hi.word = source.hi.word;
  target.hi.known_mask = source.hi.known_mask;
  target.lo.word = source.lo.word;
  target.lo.known_mask = source.lo.known_mask;
}

void copyToParent(const Nba97GameControllerProfileResetMachine &source,
                  Nba97GameMatchStateResetMachine &target) {
  for (unsigned i = 0; i < 32; ++i) {
    target.registers.gpr[i].word = source.registers.gpr[i].word;
    target.registers.gpr[i].known_mask = source.registers.gpr[i].known_mask;
  }
  target.hi.word = source.hi.word;
  target.hi.known_mask = source.hi.known_mask;
  target.lo.word = source.lo.word;
  target.lo.known_mask = source.lo.known_mask;
}

bool resetEvent(const Nba97GameMatchStateResetEvent &event) {
  return event.pc == 0x80065a38u && event.delay_slot_pc == 0x80065a3cu &&
         event.entry == 0x80083490u &&
         event.kind == NBA97_GAME_MATCH_STATE_RESET_80083490 &&
         event.argument_count == 1 && event.invocation == 1;
}

bool resetTarget(const Nba97GameMatchStateResetEvent &event) {
  return event.pc == 0x80065a38u || event.entry == 0x80083490u ||
         event.kind == NBA97_GAME_MATCH_STATE_RESET_80083490;
}

int composeZero(void *opaque, const Nba97GameTextMemory *memory,
                const Nba97GameControllerProfileResetEvent *event,
                Nba97GameControllerProfileResetMachine *machine) {
  auto &run = *static_cast<ChildRun *>(opaque);
  auto &binding = *run.binding;
  ++binding.zero_invocations;
  ++run.zeroInvocations;
  if (!memory || !event || !machine || event->pc != 0x800834d8u ||
      event->delay_slot_pc != 0x800834dcu || event->entry != 0x800a3a74u ||
      event->kind != NBA97_GAME_CONTROLLER_PROFILE_RESET_ZERO_800A3A74 ||
      event->argument_count != 2 || event->invocation != run.zeroInvocations ||
      !machineValid(*machine)) {
    binding.nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  auto &gpr = machine->registers.gpr;
  const auto incomingV0 = gpr[2];
  const auto incomingT0 = gpr[8];
  const auto incomingT1 = gpr[9];
  gpr[1] = {0, 15};  // 0x800A3A78 SLTI for the fixed count is false.
  gpr[6] = {0, 15};  // Entry 0x800A3A74 and its ANDI delay both force a2.
  gpr[10] = {0, 15}; // Replication shifts retain zero through first SWR.
  if (gpr[5].known_mask != 15 || gpr[5].word != 0x24u) {
    binding.nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  if (gpr[4].known_mask != 15) {
    binding.nested_result = NBA97_TEXT_UNKNOWN;
    return 0;
  }

  const std::uint32_t initialA0 = gpr[4].word;
  Nba97GameMemoryZeroContext context{
      *memory,         binding.zero_operation_budget,
      initialA0,       0x24u,
      incomingV0.word, static_cast<std::uint8_t>(incomingV0.known_mask == 15)};
  binding.nested_result =
      nba97_game_memory_zero(&context, &binding.zero_progress);
  const auto &progress = binding.zero_progress;
  gpr[4] = {progress.working_destination, 15};
  gpr[5] = {progress.working_count, 15};
  gpr[2] = incomingV0; // The narrow zero owner's bool cannot retain byte masks.
  if (progress.stores) {
    const std::uint32_t alignment = initialA0 & 3u;
    gpr[8] = {4u - alignment, 15};
    gpr[9] = {alignment, 15};
  } else {
    gpr[8] = incomingT0;
    gpr[9] = incomingT1;
  }
  return binding.nested_result == NBA97_TEXT_COMPLETE;
}

int dispatchParent(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GameMatchStateResetEvent *event,
                   Nba97GameMatchStateResetMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (event && resetTarget(*event))
    return nba97_game_controller_profile_reset_from_match_state_reset(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.fallback_user, memory, event, machine);
}
} // namespace

void nba97_game_controller_profile_reset_binding_init(
    Nba97GameControllerProfileResetBinding *binding,
    std::size_t operationBudget, std::size_t zeroOperationBudget) {
  if (!binding)
    return;
  std::memset(binding, 0, sizeof *binding);
  binding->operation_budget = operationBudget;
  binding->zero_operation_budget = zeroOperationBudget;
  binding->result = NBA97_TEXT_ARGUMENT;
  binding->nested_result = NBA97_TEXT_COMPLETE;
}

int nba97_game_controller_profile_reset_from_match_state_reset(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchStateResetEvent *event,
    Nba97GameMatchStateResetMachine *machine) {
  auto *binding = static_cast<Nba97GameControllerProfileResetBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !resetEvent(*event) ||
      !machineValid(*machine) || !memoryValid(*memory) ||
      machine->registers.gpr[31].known_mask != 15 ||
      machine->registers.gpr[31].word != 0x80065a40u ||
      machine->registers.gpr[4].known_mask != 15 ||
      machine->registers.gpr[4].word != 0 ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->event = *event;
  Nba97GameControllerProfileResetMachine childMachine{};
  copyToChild(*machine, childMachine);
  ChildRun child{binding, 0};
  Nba97GameControllerProfileResetContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = childMachine;
  context.io = composeZero;
  context.user = &child;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->nested_result = NBA97_TEXT_COMPLETE;
  binding->result =
      nba97_game_controller_profile_reset(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_IO_REFUSED &&
      binding->nested_result != NBA97_TEXT_COMPLETE)
    binding->result = binding->nested_result;
  if (machineValid(binding->progress.machine))
    copyToParent(binding->progress.machine, *machine);
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_match_state_reset_with_controller_profile_reset(
    const Nba97GameMatchStateResetContext *parent,
    Nba97GameControllerProfileResetBinding *binding,
    Nba97GameMatchStateResetProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  const auto operationBudget = binding->operation_budget;
  const auto zeroOperationBudget = binding->zero_operation_budget;
  auto *journal = binding->access_journal;
  const auto journalCapacity = binding->access_journal_capacity;
  nba97_game_controller_profile_reset_binding_init(binding, operationBudget,
                                                   zeroOperationBudget);
  binding->access_journal = journal;
  binding->access_journal_capacity = journalCapacity;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameMatchStateResetContext context = *parent;
  context.io = dispatchParent;
  context.user = &run;
  const int result = nba97_game_match_state_reset(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x80065a38u &&
      binding->invocations)
    return binding->result;
  return result;
}
