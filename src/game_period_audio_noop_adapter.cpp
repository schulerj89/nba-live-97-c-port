#include "game_period_audio_noop_adapter.h"

#include <cstdint>

namespace {
bool registersValid(const Nba97GameFirstPeriodStartupRegisters &registers) {
  if (registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
      registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 15)
    return false;
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (registers.gpr[i].known_mask > 15)
      return false;
  return true;
}

void copyRegisters(Nba97GamePeriodAudioNoopMachine &destination,
                   const Nba97GameFirstPeriodStartupRegisters &source) {
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
    destination.registers.gpr[i].word = source.gpr[i].word;
    destination.registers.gpr[i].known_mask = source.gpr[i].known_mask;
  }
}

void copyRegisters(Nba97GameFirstPeriodStartupRegisters &destination,
                   const Nba97GamePeriodAudioNoopMachine &source) {
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
    destination.gpr[i].word = source.registers.gpr[i].word;
    destination.gpr[i].known_mask = source.registers.gpr[i].known_mask;
  }
}
} // namespace

int nba97_game_period_audio_noop_from_first_period_startup(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameFirstPeriodStartupEvent *event,
    Nba97GameFirstPeriodStartupRegisters *registers) {
  auto *binding = static_cast<Nba97GamePeriodAudioNoopBinding *>(opaque);
  if (!event)
    return 0;
  const bool assigned =
      event->kind == NBA97_GAME_FIRST_PERIOD_STARTUP_2A254 ||
      event->entry == UINT32_C(0x8002a254) ||
      event->pc == UINT32_C(0x80067434) ||
      event->delay_slot_pc == UINT32_C(0x80067438) ||
      (registers &&
       registers->gpr[NBA97_MATCH_INITIALIZE_RA].word ==
           UINT32_C(0x8006743c));
  if (!assigned)
    return binding && binding->fallback
        ? binding->fallback(binding->fallback_user, memory, event, registers)
        : 0;
  if (!binding || !memory || !registers || !registersValid(*registers) ||
      event->kind != NBA97_GAME_FIRST_PERIOD_STARTUP_2A254 ||
      event->entry != UINT32_C(0x8002a254) ||
      event->pc != UINT32_C(0x80067434) ||
      event->delay_slot_pc != UINT32_C(0x80067438) ||
      event->argument_count != 1 ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x8006743c) ||
      registers->gpr[NBA97_MATCH_INITIALIZE_A0].known_mask != 15 ||
      registers->gpr[NBA97_MATCH_INITIALIZE_A0].word != 1) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->event = *event;
  binding->entry_registers = *registers;
  Nba97GamePeriodAudioNoopContext context{};
  copyRegisters(context.machine, *registers);
  context.machine.hi = {0, 0};
  context.machine.lo = {0, 0};
  binding->result = nba97_game_period_audio_noop(&context, &binding->progress);
  copyRegisters(*registers, binding->progress.machine);
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
