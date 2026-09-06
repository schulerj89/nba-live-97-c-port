#include "game_stamina_handicap_adapter.h"

#include <cstddef>
#include <cstdint>

namespace {
bool valid_machine(const Nba97GameStaminaHandicapMachine &machine) {
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
} // namespace

extern "C" int
nba97_game_stamina_handicap_from_match_tick(void *opaque,
                                            const Nba97MatchTickCall *call,
                                            Nba97GamePeriodValue *result) {
  auto *binding = static_cast<Nba97GameStaminaHandicapBinding *>(opaque);
  if (binding == nullptr || call == nullptr || result != nullptr ||
      call->pc != UINT32_C(0x80068e60) || call->entry != UINT32_C(0x80068504) ||
      call->count != 0u || call->args[0] != 0u || call->args[1] != 0u ||
      binding->entry_machine_ready != 1u ||
      !valid_machine(binding->entry_machine) ||
      !valid_memory(binding->memory) ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
              .known_mask != 0x0fu ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x80068e68) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr)) {
    if (binding != nullptr)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  Nba97GameStaminaHandicapContext context{};
  context.memory = binding->memory;
  context.operation_budget = binding->operation_budget;
  context.machine = binding->entry_machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->event = *call;
  binding->result = nba97_game_stamina_handicap(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_COMPLETE) {
    ++binding->completions;
    return 1;
  }
  return 0;
}
