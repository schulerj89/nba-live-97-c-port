#include "game_opponent_contact_adapter.h"

#include <cstring>

namespace {
bool valid_machine(const Nba97GameOpponentContactMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u ||
      machine.hi.known_mask > 15u || machine.lo.known_mask > 15u)
    return false;
  for (unsigned i = 0; i != 32u; ++i)
    if (machine.registers.gpr[i].known_mask > 15u)
      return false;
  return true;
}

bool valid_memory(const Nba97GameTextMemory &memory) {
  if (memory.count != 0u && memory.region == nullptr)
    return false;
  for (size_t i = 0; i != memory.count; ++i) {
    const Nba97GameTextRegion &a = memory.region[i];
    if (a.data == nullptr || a.size == 0u || a.size > UINT64_C(0x100000000) ||
        static_cast<uint64_t>(a.base) + a.size > UINT64_C(0x100000000))
      return false;
    for (size_t j = 0; j != i; ++j) {
      const Nba97GameTextRegion &b = memory.region[j];
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

extern "C" void nba97_game_opponent_contact_binding_init(
    Nba97GameOpponentContactBinding *binding, size_t operation_budget,
    Nba97GameOpponentContactIo io, void *user,
    Nba97GameOpponentContactAccess *access_journal,
    size_t access_journal_capacity) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operation_budget;
  binding->io = io;
  binding->user = user;
  binding->access_journal = access_journal;
  binding->access_journal_capacity = access_journal_capacity;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_opponent_contact_from_actor_contact_eligibility(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameActorContactEligibilityEvent *event,
    Nba97GameActorContactEligibilityMachine *machine) {
  auto *binding = static_cast<Nba97GameOpponentContactBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return 0;
  ++binding->invocations;
  std::memset(&binding->progress, 0, sizeof(binding->progress));
  binding->result = NBA97_TEXT_ARGUMENT;
  if (event->pc != UINT32_C(0x8005fa2c) ||
      event->delay_slot_pc != UINT32_C(0x8005fa30) ||
      event->entry != UINT32_C(0x8005f888) ||
      event->kind != NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_OTHER_TEAM_8005F888 ||
      event->argument_count != 2u || !valid_machine(*machine) ||
      machine->registers.gpr[31].known_mask != 15u ||
      machine->registers.gpr[31].word != UINT32_C(0x8005fa34) ||
      !valid_memory(*memory) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;

  Nba97GameOpponentContactContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_game_opponent_contact(&context, &binding->progress);
  *machine = binding->progress.machine;
  return binding->result == NBA97_TEXT_COMPLETE ? 1 : 0;
}
