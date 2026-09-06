#include "game_match_buffer_initialize_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ChildRun {
  Nba97GameMatchBufferInitializeBinding *binding;
};

template <typename Machine>
bool machineValid(const Machine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 15 ||
      machine.hi.known_mask > 15 || machine.lo.known_mask > 15)
    return false;
  for (unsigned i = 0; i < 32; ++i)
    if (machine.registers.gpr[i].known_mask > 15)
      return false;
  return true;
}

template <typename Destination, typename Source>
void copyMachine(Destination &destination, const Source &source) {
  for (unsigned i = 0; i < 32; ++i) {
    destination.registers.gpr[i].word = source.registers.gpr[i].word;
    destination.registers.gpr[i].known_mask =
        source.registers.gpr[i].known_mask;
  }
  destination.hi.word = source.hi.word;
  destination.hi.known_mask = source.hi.known_mask;
  destination.lo.word = source.lo.word;
  destination.lo.known_mask = source.lo.known_mask;
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

int composeZero(ChildRun &run, const Nba97GameTextMemory *memory,
                const Nba97GameMatchBufferInitializeEvent *event,
                Nba97GameMatchBufferInitializeMachine *machine) {
  auto &binding = *run.binding;
  if (!memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) || event->pc != UINT32_C(0x8006433c) ||
      event->delay_slot_pc != UINT32_C(0x80064340) ||
      event->entry != UINT32_C(0x800a3a74) ||
      event->kind != NBA97_GAME_MATCH_BUFFER_INITIALIZE_ZERO_800A3A74 ||
      event->invocation != 1 ||
      event->argument_count != 2 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x80064344) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word !=
          UINT32_C(0x800f9ffc) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word != 0x378u) {
    binding.nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding.zero_invocations;
  auto &gpr = machine->registers.gpr;
  const auto incoming_v0 = gpr[NBA97_MATCH_INITIALIZE_V0];
  const auto incoming_t0 = gpr[NBA97_MATCH_INITIALIZE_T0];
  const auto incoming_t1 = gpr[NBA97_MATCH_INITIALIZE_T0 + 1];
  gpr[NBA97_MATCH_INITIALIZE_AT] = {0, 15};
  gpr[NBA97_MATCH_INITIALIZE_A2] = {0, 15};
  gpr[NBA97_MATCH_INITIALIZE_T0 + 2] = {0, 15};
  Nba97GameMemoryZeroContext context{
      *memory, binding.zero_operation_budget, UINT32_C(0x800f9ffc), 0x378u,
      incoming_v0.word,
      static_cast<std::uint8_t>(incoming_v0.known_mask == 15)};
  binding.zero_result =
      nba97_game_memory_zero(&context, &binding.zero_progress);
  gpr[NBA97_MATCH_INITIALIZE_A0] = {
      binding.zero_progress.working_destination, 15};
  gpr[NBA97_MATCH_INITIALIZE_A1] = {
      binding.zero_progress.working_count, 15};
  gpr[NBA97_MATCH_INITIALIZE_V0] = incoming_v0;
  if (binding.zero_progress.stores) {
    gpr[NBA97_MATCH_INITIALIZE_T0] = {4, 15};
    gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = {0, 15};
  } else {
    gpr[NBA97_MATCH_INITIALIZE_T0] = incoming_t0;
    gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = incoming_t1;
  }
  binding.nested_result = binding.zero_result;
  return binding.zero_result == NBA97_TEXT_COMPLETE;
}

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameMatchBufferInitializeEvent *event,
             Nba97GameMatchBufferInitializeMachine *machine) {
  auto &run = *static_cast<ChildRun *>(opaque);
  auto &binding = *run.binding;
  binding.nested_result = NBA97_TEXT_COMPLETE;
  if (event &&
      event->kind == NBA97_GAME_MATCH_BUFFER_INITIALIZE_ZERO_800A3A74)
    return composeZero(run, memory, event, machine);
  if (!event || event->pc != UINT32_C(0x80064370) ||
      event->delay_slot_pc != UINT32_C(0x80064374) ||
      event->entry != UINT32_C(0x80076ad0) ||
      event->kind != NBA97_GAME_MATCH_BUFFER_INITIALIZE_CHILD_80076AD0 ||
      event->argument_count != 0) {
    binding.nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding.child_80076AD0_invocations;
  if (!binding.io) {
    binding.nested_result = NBA97_TEXT_IO_REFUSED;
    return 0;
  }
  int accepted = binding.io(binding.user, memory, event, machine);
  if (accepted != 1)
    binding.nested_result = NBA97_TEXT_IO_REFUSED;
  return accepted;
}
} // namespace

int nba97_game_match_buffer_initialize_from_match_state_reset(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchStateResetEvent *event,
    Nba97GameMatchStateResetMachine *machine) {
  auto *binding = static_cast<Nba97GameMatchBufferInitializeBinding *>(opaque);
  const bool target = event &&
      (event->kind == NBA97_GAME_MATCH_STATE_RESET_8006432C ||
       event->entry == UINT32_C(0x8006432c) ||
       event->pc == UINT32_C(0x80065af8));
  if (!target)
    return binding && binding->fallback ?
        binding->fallback(binding->fallback_user, memory, event, machine) : 0;
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) || event->pc != UINT32_C(0x80065af8) ||
      event->delay_slot_pc != UINT32_C(0x80065afc) ||
      event->entry != UINT32_C(0x8006432c) ||
      event->kind != NBA97_GAME_MATCH_STATE_RESET_8006432C ||
      event->invocation != 1 ||
      event->argument_count != 0 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x80065b00) ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->event = *event;
  binding->nested_result = NBA97_TEXT_COMPLETE;
  binding->zero_result = NBA97_TEXT_COMPLETE;
  binding->zero_invocations = 0;
  binding->child_80076AD0_invocations = 0;
  std::memset(&binding->zero_progress, 0, sizeof binding->zero_progress);
  ChildRun child{binding};
  Nba97GameMatchBufferInitializeContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  copyMachine(context.machine, *machine);
  context.io = dispatch;
  context.user = &child;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result =
      nba97_game_match_buffer_initialize(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_IO_REFUSED &&
      binding->nested_result != NBA97_TEXT_COMPLETE)
    binding->result = binding->nested_result;
  if (binding->result != NBA97_TEXT_ARGUMENT ||
      binding->progress.stopped_pc != 0)
    copyMachine(*machine, binding->progress.machine);
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
