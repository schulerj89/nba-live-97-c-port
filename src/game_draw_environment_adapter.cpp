#include "game_draw_environment_adapter.h"

#include <cstring>

namespace {

bool valid_registers(const Nba97GameSceneStartupRegisters &registers) {
  if (registers.gpr[0].word != 0u || registers.gpr[0].known_mask != 15u)
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (registers.gpr[index].known_mask > 15u)
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

extern "C" void nba97_game_draw_environment_scene_binding_init(
    Nba97GameDrawEnvironmentSceneBinding *binding, size_t operation_budget,
    Nba97GameDrawEnvironmentIo io, void *user,
    Nba97GameDrawEnvironmentHiLoProvider hi_lo, void *hi_lo_user,
    Nba97GameDrawEnvironmentAccess *access_journal,
    size_t access_journal_capacity, Nba97GameSceneStartupIo fallback,
    void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operation_budget;
  binding->io = io;
  binding->user = user;
  binding->hi_lo = hi_lo;
  binding->hi_lo_user = hi_lo_user;
  binding->access_journal = access_journal;
  binding->access_journal_capacity = access_journal_capacity;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_draw_environment_from_scene(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameSceneStartupEvent *event,
    Nba97GameSceneStartupRegisters *registers) {
  auto *binding = static_cast<Nba97GameDrawEnvironmentSceneBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      registers == nullptr)
    return 0;

  const bool draw_kind = event->kind == NBA97_GAME_SCENE_STARTUP_DRAW_80099ACC;
  const bool draw_entry = event->entry == UINT32_C(0x80099acc);
  if (!draw_kind && !draw_entry) {
    ++binding->fallback_invocations;
    if (binding->fallback == nullptr) {
      binding->result = NBA97_TEXT_IO_REFUSED;
      return 0;
    }
    return binding->fallback(binding->fallback_user, memory, event, registers);
  }

  ++binding->invocations;
  std::memset(&binding->progress, 0, sizeof(binding->progress));
  binding->result = NBA97_TEXT_ARGUMENT;
  const bool first_site = event->pc == UINT32_C(0x80048f4c) &&
                          event->delay_slot_pc == UINT32_C(0x80048f50);
  const bool second_site = event->pc == UINT32_C(0x80048fa0) &&
                           event->delay_slot_pc == UINT32_C(0x80048fa4);
  if (!draw_kind || !draw_entry || (!first_site && !second_site) ||
      event->argument_count != 1u || !valid_registers(*registers) ||
      !valid_memory(*memory) ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].word != event->pc + 8u ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;

  Nba97GameDrawEnvironmentWord hi{0u, 0u};
  Nba97GameDrawEnvironmentWord lo{0u, 0u};
  if (binding->hi_lo != nullptr &&
      binding->hi_lo(binding->hi_lo_user, event, &hi, &lo) != 1) {
    binding->result = NBA97_TEXT_IO_REFUSED;
    return 0;
  }
  if (hi.known_mask > 15u || lo.known_mask > 15u)
    return 0;

  Nba97GameDrawEnvironmentContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine.registers = *registers;
  context.machine.hi = hi;
  context.machine.lo = lo;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_game_draw_environment(&context, &binding->progress);
  *registers = binding->progress.machine.registers;
  return binding->result == NBA97_TEXT_COMPLETE ? 1 : 0;
}
