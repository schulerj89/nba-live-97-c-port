#include "game_texture_window_command_adapter.h"

#include <cstring>

namespace {

bool valid_machine(const Nba97GameDrawPacketMachine &machine) {
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

extern "C" void nba97_game_texture_window_command_packet_binding_init(
    Nba97GameTextureWindowCommandPacketBinding *binding,
    size_t operation_budget,
    Nba97GameTextureWindowCommandAccess *access_journal,
    size_t access_journal_capacity, Nba97GameDrawPacketIo fallback,
    void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operation_budget;
  binding->access_journal = access_journal;
  binding->access_journal_capacity = access_journal_capacity;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_texture_window_command_from_packet(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameDrawPacketEvent *event,
    Nba97GameDrawPacketMachine *machine) {
  auto *binding =
      static_cast<Nba97GameTextureWindowCommandPacketBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return 0;

  const bool target_kind =
      event->kind == NBA97_GAME_DRAW_PACKET_CHILD_8009A824;
  const bool target_entry = event->entry == UINT32_C(0x8009a824);
  if (!target_kind && !target_entry) {
    ++binding->fallback_invocations;
    if (binding->fallback == nullptr)
      return 0;
    return binding->fallback(binding->fallback_user, memory, event, machine);
  }

  binding->result = NBA97_TEXT_ARGUMENT;
  if (!target_kind || !target_entry || event->pc != UINT32_C(0x8009a3d4) ||
      event->delay_slot_pc != UINT32_C(0x8009a3d8) ||
      event->argument_count != 1u || !valid_machine(*machine) ||
      !valid_memory(*memory) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x8009a3dc) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;

  Nba97GameTextureWindowCommandContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->event = *event;
  binding->result =
      nba97_game_texture_window_command(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result == NBA97_TEXT_COMPLETE) {
    ++binding->completions;
    return 1;
  }
  return 0;
}
