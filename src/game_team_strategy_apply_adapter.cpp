#include "game_team_strategy_apply_adapter.h"

#include <cstring>

namespace {

bool valid_machine(const Nba97GameMatchStateResetMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u ||
      machine.hi.known_mask > 15u || machine.lo.known_mask > 15u)
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (machine.registers.gpr[index].known_mask > 15u)
      return false;
  return true;
}

bool valid_memory(const Nba97GameTextMemory &memory) {
  if (memory.count != 0u && memory.region == nullptr)
    return false;
  for (size_t index = 0u; index != memory.count; ++index) {
    const Nba97GameTextRegion &region = memory.region[index];
    if (region.data == nullptr || region.size == 0u ||
        region.size > UINT64_C(0x100000000) ||
        static_cast<uint64_t>(region.base) + region.size >
            UINT64_C(0x100000000))
      return false;
    for (size_t earlier = 0u; earlier != index; ++earlier) {
      const Nba97GameTextRegion &other = memory.region[earlier];
      if (static_cast<uint64_t>(region.base) <
              static_cast<uint64_t>(other.base) + other.size &&
          static_cast<uint64_t>(other.base) <
              static_cast<uint64_t>(region.base) + region.size)
        return false;
    }
  }
  return true;
}

} // namespace

extern "C" void nba97_game_team_strategy_apply_reset_binding_init(
    Nba97GameTeamStrategyApplyResetBinding *binding, size_t operation_budget,
    Nba97GameTeamStrategyApplyIo io, void *user,
    Nba97GameTeamStrategyApplyAccess *access_journal,
    size_t access_journal_capacity, Nba97GameMatchStateResetIo fallback,
    void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operation_budget;
  binding->io = io;
  binding->user = user;
  binding->access_journal = access_journal;
  binding->access_journal_capacity = access_journal_capacity;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_team_strategy_apply_from_reset(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchStateResetEvent *event,
    Nba97GameMatchStateResetMachine *machine) {
  auto *binding = static_cast<Nba97GameTeamStrategyApplyResetBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return 0;

  const bool target_kind =
      event->kind == NBA97_GAME_MATCH_STATE_RESET_80065820;
  const bool target_entry = event->entry == UINT32_C(0x80065820);
  const bool target_pc = event->pc == UINT32_C(0x80065abc) ||
                         event->pc == UINT32_C(0x80065ac4);
  if (!target_kind && !target_entry && !target_pc) {
    ++binding->fallback_invocations;
    if (binding->fallback == nullptr)
      return 0;
    return binding->fallback(binding->fallback_user, memory, event, machine);
  }

  const bool first_site =
      event->pc == UINT32_C(0x80065abc) &&
      event->delay_slot_pc == UINT32_C(0x80065ac0) &&
      event->invocation == 1u &&
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x80065ac4);
  const bool second_site =
      event->pc == UINT32_C(0x80065ac4) &&
      event->delay_slot_pc == UINT32_C(0x80065ac8) &&
      event->invocation == 2u &&
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x80065acc);
  binding->result = NBA97_TEXT_ARGUMENT;
  if (!target_kind || !target_entry || event->argument_count != 1u ||
      !valid_machine(*machine) || !valid_memory(*memory) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      (!first_site && !second_site) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;

  Nba97GameTeamStrategyApplyContext context{};
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
      nba97_game_team_strategy_apply(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result == NBA97_TEXT_COMPLETE) {
    ++binding->completions;
    return 1;
  }
  return 0;
}
