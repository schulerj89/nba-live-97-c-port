#include "game_substitution_candidate_select_adapter.h"

#include <cstring>

namespace {
bool machine_valid(const Nba97GameTeamStrategyApplyMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u ||
      machine.hi.known_mask > 15u || machine.lo.known_mask > 15u)
    return false;
  for (unsigned i = 0u; i != 32u; ++i)
    if (machine.registers.gpr[i].known_mask > 15u)
      return false;
  return true;
}
bool memory_valid(const Nba97GameTextMemory &memory) {
  if (memory.count && !memory.region)
    return false;
  for (size_t i = 0u; i != memory.count; ++i) {
    const auto &a = memory.region[i];
    if (!a.data || !a.size || a.size > UINT64_C(0x100000000) ||
        static_cast<uint64_t>(a.base) + a.size > UINT64_C(0x100000000))
      return false;
    for (size_t j = 0u; j != i; ++j) {
      const auto &b = memory.region[j];
      if (static_cast<uint64_t>(a.base) <
              static_cast<uint64_t>(b.base) + b.size &&
          static_cast<uint64_t>(b.base) <
              static_cast<uint64_t>(a.base) + a.size)
        return false;
    }
  }
  return true;
}
} // namespace

extern "C" void nba97_game_substitution_candidate_select_strategy_binding_init(
    Nba97GameSubstitutionCandidateSelectStrategyBinding *binding,
    size_t operation_budget, Nba97GameSubstitutionCandidateSelectIo io,
    void *user, Nba97GameSubstitutionCandidateSelectAccess *journal,
    size_t journal_capacity, Nba97GameTeamStrategyApplyIo fallback,
    void *fallback_user) {
  if (!binding)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operation_budget;
  binding->io = io;
  binding->user = user;
  binding->access_journal = journal;
  binding->access_journal_capacity = journal_capacity;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_substitution_candidate_select_from_strategy(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameTeamStrategyApplyEvent *event,
    Nba97GameTeamStrategyApplyMachine *machine) {
  auto *binding =
      static_cast<Nba97GameSubstitutionCandidateSelectStrategyBinding *>(
          opaque);
  if (!binding || !memory || !event || !machine)
    return 0;
  const bool kind = event->kind == NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC;
  const bool entry = event->entry == UINT32_C(0x80064dbc);
  const bool pc = event->pc == UINT32_C(0x800659c4);
  if (!kind && !entry && !pc) {
    ++binding->fallback_invocations;
    return binding->fallback ? binding->fallback(binding->fallback_user, memory,
                                                 event, machine)
                             : 0;
  }
  binding->result = NBA97_TEXT_ARGUMENT;
  if (!kind || !entry || !pc || event->delay_slot_pc != UINT32_C(0x800659c8) ||
      event->argument_count != 4u || event->invocation != 1u ||
      !machine_valid(*machine) || !memory_valid(*memory) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x800659cc) ||
      (binding->access_journal_capacity && !binding->access_journal))
    return 0;

  Nba97GameSubstitutionCandidateSelectContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->event = *event;
  binding->result =
      nba97_game_substitution_candidate_select(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result == NBA97_TEXT_COMPLETE) {
    ++binding->completions;
    return 1;
  }
  return 0;
}
