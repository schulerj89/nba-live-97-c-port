#include "game_actor_collision_response_adapter.h"

#include "recovered/game_controller_selection.h"

#include <cstring>

namespace {

bool valid_machine(const Nba97GameActorCollisionResponseMachine &machine) {
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

int32_t signed_word(uint32_t word) {
  return word <= static_cast<uint32_t>(INT32_MAX)
             ? static_cast<int32_t>(word)
             : -1 - static_cast<int32_t>(UINT32_MAX - word);
}

uint32_t wrapping_abs(uint32_t word) {
  return signed_word(word) < 0 ? 0u - word : word;
}

} // namespace

extern "C" void nba97_game_actor_collision_response_binding_init(
    Nba97GameActorCollisionResponseBinding *binding, size_t operation_budget,
    Nba97GameActorCollisionResponseIo io, void *user,
    Nba97GameActorCollisionResponseAccess *access_journal,
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

extern "C" int nba97_game_actor_collision_response_from_opponent_contact(
    void *user, const Nba97GameTextMemory *memory,
    const Nba97GameOpponentContactEvent *event,
    Nba97GameOpponentContactMachine *machine) {
  auto *binding = static_cast<Nba97GameActorCollisionResponseBinding *>(user);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return 0;
  ++binding->invocations;
  std::memset(&binding->progress, 0, sizeof(binding->progress));
  binding->result = NBA97_TEXT_ARGUMENT;
  if (event->pc != UINT32_C(0x8005f92c) ||
      event->delay_slot_pc != UINT32_C(0x8005f930) ||
      event->entry != UINT32_C(0x8005f3bc) || event->argument_count != 2u ||
      !valid_machine(*machine) || !valid_memory(*memory) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x8005f934) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;
  Nba97GameActorCollisionResponseContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result =
      nba97_game_actor_collision_response(&context, &binding->progress);
  *machine = binding->progress.machine;
  return binding->result == NBA97_TEXT_COMPLETE ? 1 : 0;
}

extern "C" void nba97_game_actor_collision_response_geometry_binding_init(
    Nba97GameActorCollisionResponseGeometryBinding *binding,
    Nba97GameActorCollisionResponseIo fallback, void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_actor_collision_response_geometry_child(
    void *user, const Nba97GameTextMemory *memory,
    const Nba97GameActorCollisionResponseEvent *event,
    Nba97GameActorCollisionResponseMachine *machine) {
  auto *binding =
      static_cast<Nba97GameActorCollisionResponseGeometryBinding *>(user);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return 0;
  if (event->kind != NBA97_GAME_ACTOR_COLLISION_RESPONSE_GEOMETRY_8007066C) {
    ++binding->fallback_invocations;
    if (binding->fallback == nullptr) {
      binding->result = NBA97_TEXT_IO_REFUSED;
      return 0;
    }
    const int accepted =
        binding->fallback(binding->fallback_user, memory, event, machine);
    binding->result = accepted == 1
                          ? (valid_machine(*machine) ? NBA97_TEXT_COMPLETE
                                                     : NBA97_TEXT_ARGUMENT)
                          : NBA97_TEXT_IO_REFUSED;
    return accepted;
  }
  ++binding->geometry_invocations;
  binding->result = NBA97_TEXT_ARGUMENT;
  if (event->pc != UINT32_C(0x8005f424) ||
      event->delay_slot_pc != UINT32_C(0x8005f428) ||
      event->entry != UINT32_C(0x8007066c) || event->argument_count != 2u ||
      !valid_machine(*machine) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x8005f42c) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask != 15u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask != 15u)
    return 0;
  const uint32_t source_a0 =
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word;
  const uint32_t source_a1 =
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word;
  const int32_t distance = nba97_game_selection_distance(
      signed_word(source_a0), signed_word(source_a1));
  machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      wrapping_abs(source_a0);
  machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word =
      wrapping_abs(source_a1);
  machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0].word =
      static_cast<uint32_t>(distance);
  machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask = 15u;
  machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask = 15u;
  machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask = 15u;
  binding->result = NBA97_TEXT_COMPLETE;
  return 1;
}
