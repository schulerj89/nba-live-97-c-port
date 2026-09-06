#include "game_match_buffer_pending_adapter.h"

#include <cstdint>

namespace {
bool registersValid(const Nba97GamePeriodStartupRegisters &registers) {
  if (registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
      registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 15)
    return false;
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (registers.gpr[i].known_mask > 15)
      return false;
  return true;
}

bool firstCall(const Nba97GamePeriodStartupEvent &event,
               const Nba97GamePeriodStartupRegisters &registers) {
  return event.pc == UINT32_C(0x800674f0) &&
      event.delay_slot_pc == UINT32_C(0x800674f4) &&
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15 &&
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x800674f8);
}

bool secondCall(const Nba97GamePeriodStartupEvent &event,
                const Nba97GamePeriodStartupRegisters &registers) {
  return event.pc == UINT32_C(0x80067500) &&
      event.delay_slot_pc == UINT32_C(0x80067504) &&
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15 &&
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x80067508);
}

void copyRegisters(Nba97GameMatchBufferPendingMachine &destination,
                   const Nba97GamePeriodStartupRegisters &source) {
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
    destination.registers.gpr[i].word = source.gpr[i].word;
    destination.registers.gpr[i].known_mask = source.gpr[i].known_mask;
  }
}

void copyRegisters(Nba97GamePeriodStartupRegisters &destination,
                   const Nba97GameMatchBufferPendingMachine &source) {
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
    destination.gpr[i].word = source.registers.gpr[i].word;
    destination.gpr[i].known_mask = source.registers.gpr[i].known_mask;
  }
}
} // namespace

int nba97_game_match_buffer_pending_from_period_startup(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GamePeriodStartupEvent *event,
    Nba97GamePeriodStartupRegisters *registers) {
  auto *binding =
      static_cast<Nba97GameMatchBufferPendingPeriodBinding *>(opaque);
  if (!event)
    return 0;
  const bool assigned =
      event->kind == NBA97_GAME_PERIOD_STARTUP_76B28 ||
      event->entry == UINT32_C(0x80076b28) ||
      event->pc == UINT32_C(0x800674f0) ||
      event->pc == UINT32_C(0x80067500) ||
      event->delay_slot_pc == UINT32_C(0x800674f4) ||
      event->delay_slot_pc == UINT32_C(0x80067504) ||
      (registers &&
       (registers->gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            UINT32_C(0x800674f8) ||
        registers->gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            UINT32_C(0x80067508)));
  if (!assigned)
    return binding && binding->fallback
        ? binding->fallback(binding->fallback_user, memory, event, registers)
        : 0;
  if (!binding || !memory || !registers || !registersValid(*registers) ||
      event->kind != NBA97_GAME_PERIOD_STARTUP_76B28 ||
      event->entry != UINT32_C(0x80076b28) || event->argument_count != 0 ||
      (!firstCall(*event, *registers) && !secondCall(*event, *registers)) ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  const bool first = event->pc == UINT32_C(0x800674f0);
  ++binding->invocations;
  if (first) {
    ++binding->first_invocations;
    binding->first_event = *event;
  } else {
    ++binding->second_invocations;
    binding->second_event = *event;
  }
  Nba97GameMatchBufferPendingContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  copyRegisters(context.machine, *registers);
  context.machine.hi = {0, 0};
  context.machine.lo = {0, 0};
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result =
      nba97_game_match_buffer_pending(&context, &binding->progress);
  if (binding->result != NBA97_TEXT_ARGUMENT ||
      binding->progress.stopped_pc != 0)
    copyRegisters(*registers, binding->progress.machine);
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
