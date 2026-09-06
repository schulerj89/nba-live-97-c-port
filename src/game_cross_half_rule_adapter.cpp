#include "game_cross_half_rule_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
bool valid_machine(const Nba97GameCrossHalfRuleMachine &machine) {
  if (machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0u ||
      machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu ||
      machine.hi.known_mask > 0x0fu || machine.lo.known_mask > 0x0fu)
    return false;
  for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
       ++index)
    if (machine.registers.gpr[index].known_mask > 0x0fu)
      return false;
  return true;
}

bool valid_memory(const Nba97GameTextMemory &memory) {
  if (memory.count != 0u && memory.region == nullptr)
    return false;
  for (std::size_t index = 0u; index != memory.count; ++index) {
    const auto &region = memory.region[index];
    if (region.data == nullptr || region.size == 0u ||
        static_cast<std::uint64_t>(region.size) > UINT64_C(0x100000000) ||
        static_cast<std::uint64_t>(region.base) + region.size >
            UINT64_C(0x100000000))
      return false;
    for (std::size_t earlier = 0u; earlier != index; ++earlier) {
      const auto &other = memory.region[earlier];
      if (static_cast<std::uint64_t>(region.base) <
              static_cast<std::uint64_t>(other.base) + other.size &&
          static_cast<std::uint64_t>(other.base) <
              static_cast<std::uint64_t>(region.base) + region.size)
        return false;
    }
  }
  return true;
}

bool claims_rule_delay(const Nba97GameCrossHalfRuleEvent &event) {
  return event.kind == NBA97_GAME_CROSS_HALF_RULE_CHILD_800295C8 ||
         event.entry == UINT32_C(0x800295c8) ||
         event.pc == UINT32_C(0x800682d0);
}

bool valid_rule_delay(const Nba97GameCrossHalfRuleEvent &event,
                      const Nba97GameCrossHalfRuleMachine &machine) {
  const auto &a0 = machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0];
  const auto &ra = machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  return event.kind == NBA97_GAME_CROSS_HALF_RULE_CHILD_800295C8 &&
         event.entry == UINT32_C(0x800295c8) &&
         event.pc == UINT32_C(0x800682d0) &&
         event.delay_slot_pc == UINT32_C(0x800682d4) &&
         event.argument_count == 1u && event.operation != 0u &&
         event.invocation == 1u && a0.known_mask == 0x0fu &&
         (a0.word == 5000u || a0.word == 20000u) && ra.known_mask == 0x0fu &&
         ra.word == UINT32_C(0x800682d8);
}
} // namespace

extern "C" int nba97_game_cross_half_rule_compose_child(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameCrossHalfRuleEvent *event,
    Nba97GameCrossHalfRuleMachine *machine) {
  auto *binding = static_cast<Nba97GameCrossHalfRuleBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return 0;

  if (!claims_rule_delay(*event)) {
    ++binding->fallback_invocations;
    if (binding->io == nullptr)
      return 0;
    const int accepted = binding->io(binding->user, memory, event, machine);
    if (accepted == 1)
      ++binding->fallback_completions;
    return accepted;
  }

  binding->rule_delay_result = NBA97_TEXT_ARGUMENT;
  std::memset(&binding->rule_delay_progress, 0,
              sizeof(binding->rule_delay_progress));
  if (!valid_memory(*memory) || !valid_machine(*machine) ||
      !valid_rule_delay(*event, *machine))
    return 0;

  Nba97GameRuleDelayContext context{};
  context.machine = *machine;
  ++binding->rule_delay_invocations;
  binding->rule_delay_event = *event;
  binding->rule_delay_result =
      nba97_game_rule_delay(&context, &binding->rule_delay_progress);
  if (binding->rule_delay_result != NBA97_TEXT_ARGUMENT)
    *machine = binding->rule_delay_progress.machine;
  return binding->rule_delay_result == NBA97_TEXT_COMPLETE;
}

extern "C" int
nba97_game_cross_half_rule_from_match_tick(void *opaque,
                                           const Nba97MatchTickCall *call,
                                           Nba97GamePeriodValue *result) {
  auto *binding = static_cast<Nba97GameCrossHalfRuleBinding *>(opaque);
  if (binding == nullptr || call == nullptr || result != nullptr ||
      call->pc != UINT32_C(0x80068e30) || call->entry != UINT32_C(0x8006817c) ||
      call->count != 0u || call->args[0] != 0u || call->args[1] != 0u ||
      binding->entry_machine_ready != 1u ||
      !valid_machine(binding->entry_machine) ||
      !valid_memory(binding->memory) ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
              .known_mask != 0x0fu ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x80068e38) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr)) {
    if (binding != nullptr)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  Nba97GameCrossHalfRuleContext context{};
  context.memory = binding->memory;
  context.operation_budget = binding->operation_budget;
  context.machine = binding->entry_machine;
  context.io = nba97_game_cross_half_rule_compose_child;
  context.user = binding;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->event = *call;
  binding->result = nba97_game_cross_half_rule(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_COMPLETE) {
    ++binding->completions;
    return 1;
  }
  return 0;
}
