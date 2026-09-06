#include "game_period_music_start_adapter.h"

#include <cstring>

namespace {
bool valid_parent_registers(
    const Nba97GameFirstPeriodStartupRegisters &registers) {
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
    const auto &region = memory.region[index];
    if (region.data == nullptr || region.size == 0u ||
        static_cast<uint64_t>(region.size) > UINT64_C(0x100000000) ||
        static_cast<uint64_t>(region.base) + region.size >
            UINT64_C(0x100000000))
      return false;
    for (size_t earlier = 0u; earlier != index; ++earlier) {
      const auto &other = memory.region[earlier];
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

extern "C" void nba97_game_period_music_start_first_period_binding_init(
    Nba97GamePeriodMusicStartFirstPeriodBinding *binding,
    size_t operation_budget, Nba97GamePeriodMusicStartIo io, void *user,
    Nba97GamePeriodMusicStartAccess *journal, size_t journal_capacity,
    Nba97GameFirstPeriodStartupIo fallback, void *fallback_user) {
  if (binding == nullptr)
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

extern "C" int nba97_game_period_music_start_from_first_period(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameFirstPeriodStartupEvent *event,
    Nba97GameFirstPeriodStartupRegisters *registers) {
  auto *binding =
      static_cast<Nba97GamePeriodMusicStartFirstPeriodBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      registers == nullptr)
    return 0;

  const bool kind = event->kind == NBA97_GAME_FIRST_PERIOD_STARTUP_295D0;
  const bool entry = event->entry == UINT32_C(0x800295d0);
  const bool pc = event->pc == UINT32_C(0x800673f8);
  const bool delay = event->delay_slot_pc == UINT32_C(0x800673fc);
  const bool return_address =
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].word == UINT32_C(0x80067400);
  if (!kind && !entry && !pc && !delay && !return_address) {
    ++binding->fallback_invocations;
    return binding->fallback != nullptr
               ? binding->fallback(binding->fallback_user, memory, event,
                                   registers)
               : 0;
  }

  binding->result = NBA97_TEXT_ARGUMENT;
  if (!kind || !entry || !pc || event->delay_slot_pc != UINT32_C(0x800673fc) ||
      event->argument_count != 0u || !valid_parent_registers(*registers) ||
      !valid_memory(*memory) ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].word != UINT32_C(0x80067400) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;

  Nba97GamePeriodMusicStartContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine.registers = *registers;
  context.machine.hi = {0u, 0u};
  context.machine.lo = {0u, 0u};
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->event = *event;
  binding->result = nba97_game_period_music_start(&context, &binding->progress);

  /* The parent has no HI/LO fields. Preserve any valid GPR prefix even when a
   * child corrupts only HI/LO and the full-machine owner reports ARGUMENT. */
  if (valid_parent_registers(binding->progress.machine.registers))
    *registers = binding->progress.machine.registers;
  if (binding->result == NBA97_TEXT_COMPLETE) {
    ++binding->completions;
    return 1;
  }
  return 0;
}
