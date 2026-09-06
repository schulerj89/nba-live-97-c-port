#include "game_team_header_initialize_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GameMatchStateResetIo fallback;
  void *fallback_user;
  Nba97GameTeamHeaderInitializeBinding *binding;
};

bool machineValid(const Nba97GameMatchStateResetMachine &machine) {
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

bool teamTarget(const Nba97GameMatchStateResetEvent *event) {
  return event && (event->kind == NBA97_GAME_MATCH_STATE_RESET_800655B0 ||
                   event->entry == 0x800655b0u || event->pc == 0x80065a88u ||
                   event->pc == 0x80065a94u);
}

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameMatchStateResetEvent *event,
             Nba97GameMatchStateResetMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (teamTarget(event))
    return nba97_game_team_header_initialize_from_match_state_reset(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.fallback_user, memory, event, machine);
}
} // namespace

int nba97_game_team_header_initialize_from_match_state_reset(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchStateResetEvent *event,
    Nba97GameMatchStateResetMachine *machine) {
  auto *binding = static_cast<Nba97GameTeamHeaderInitializeBinding *>(opaque);
  std::size_t index = 0;
  if (event && event->pc == 0x80065a94u)
    index = 1;
  if (!binding || !memory || !event || !machine ||
      event->kind != NBA97_GAME_MATCH_STATE_RESET_800655B0 ||
      event->entry != 0x800655b0u ||
      (event->pc != 0x80065a88u && event->pc != 0x80065a94u) ||
      event->delay_slot_pc != event->pc + 4u ||
      event->invocation != index + 1 || event->argument_count != 2 ||
      !machineValid(*machine) || !memoryValid(*memory) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          event->pc + 8u ||
      (binding && !binding->access_journal[index] &&
       binding->access_journal_capacity[index])) {
    if (binding)
      binding->result[index] = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->event[index] = *event;
  Nba97GameTeamHeaderInitializeContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget[index];
  context.machine.registers = machine->registers;
  context.machine.hi = machine->hi;
  context.machine.lo = machine->lo;
  context.access_journal = binding->access_journal[index];
  context.access_journal_capacity = binding->access_journal_capacity[index];
  binding->result[index] =
      nba97_game_team_header_initialize(&context, &binding->progress[index]);
  machine->registers = binding->progress[index].machine.registers;
  machine->hi = binding->progress[index].machine.hi;
  machine->lo = binding->progress[index].machine.lo;
  if (binding->result[index] != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_match_state_reset_with_team_header_initialize(
    const Nba97GameMatchStateResetContext *parent,
    Nba97GameTeamHeaderInitializeBinding *binding,
    Nba97GameMatchStateResetProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(binding->event, 0, sizeof binding->event);
  std::memset(binding->progress, 0, sizeof binding->progress);
  binding->result[0] = NBA97_TEXT_COMPLETE;
  binding->result[1] = NBA97_TEXT_COMPLETE;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameMatchStateResetContext context = *parent;
  context.io = dispatch;
  context.user = &run;
  int result = nba97_game_match_state_reset(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED &&
      (progress->stopped_pc == 0x80065a88u ||
       progress->stopped_pc == 0x80065a94u) &&
      binding->invocations)
    return binding->result[progress->stopped_pc == 0x80065a94u ? 1 : 0];
  return result;
}
