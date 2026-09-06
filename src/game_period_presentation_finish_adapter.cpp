#include "game_period_presentation_finish_adapter.h"

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

bool machineGprsValid(
    const Nba97GamePeriodPresentationFinishMachine &machine) {
  if (machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
      machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 15)
    return false;
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (machine.registers.gpr[i].known_mask > 15)
      return false;
  return true;
}

void copyRegisters(Nba97GamePeriodPresentationFinishMachine &destination,
                   const Nba97GameFirstPeriodStartupRegisters &source) {
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
    destination.registers.gpr[i].word = source.gpr[i].word;
    destination.registers.gpr[i].known_mask = source.gpr[i].known_mask;
  }
}

void copyRegisters(Nba97GameFirstPeriodStartupRegisters &destination,
                   const Nba97GamePeriodPresentationFinishMachine &source) {
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
    destination.gpr[i].word = source.registers.gpr[i].word;
    destination.gpr[i].known_mask = source.registers.gpr[i].known_mask;
  }
}
} // namespace

int nba97_game_period_presentation_finish_from_first_period_startup(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameFirstPeriodStartupEvent *event,
    Nba97GameFirstPeriodStartupRegisters *registers) {
  auto *binding =
      static_cast<Nba97GamePeriodPresentationFinishBinding *>(opaque);
  if (!event)
    return 0;
  const bool assigned =
      event->kind == NBA97_GAME_FIRST_PERIOD_STARTUP_2DDCC ||
      event->entry == UINT32_C(0x8002ddcc) ||
      event->pc == UINT32_C(0x80067424) ||
      event->delay_slot_pc == UINT32_C(0x80067428) ||
      (registers &&
       registers->gpr[NBA97_MATCH_INITIALIZE_RA].word ==
           UINT32_C(0x8006742c));
  if (!assigned)
    return binding && binding->fallback
        ? binding->fallback(binding->fallback_user, memory, event, registers)
        : 0;
  if (!binding || !memory || !registers || !registersValid(*registers) ||
      event->kind != NBA97_GAME_FIRST_PERIOD_STARTUP_2DDCC ||
      event->entry != UINT32_C(0x8002ddcc) ||
      event->pc != UINT32_C(0x80067424) ||
      event->delay_slot_pc != UINT32_C(0x80067428) ||
      event->argument_count != 0 ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      registers->gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x8006742c) ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->event = *event;
  Nba97GamePeriodPresentationFinishContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  copyRegisters(context.machine, *registers);
  context.machine.hi = {0, 0};
  context.machine.lo = {0, 0};
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result =
      nba97_game_period_presentation_finish(&context, &binding->progress);
  if (machineGprsValid(binding->progress.machine))
    copyRegisters(*registers, binding->progress.machine);
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
