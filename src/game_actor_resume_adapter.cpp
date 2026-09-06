#include "game_actor_resume_adapter.h"

#include <cstdint>

int nba97_game_actor_resume_from_period_expiry(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GamePeriodExpiryEvent *event,
    Nba97GamePeriodExpiryMachine *machine) {
  auto *binding = static_cast<Nba97GameActorResumeBinding *>(opaque);
  if (!binding || !memory || !event || !machine ||
      event->kind != NBA97_GAME_PERIOD_EXPIRY_CHILD_800582DC ||
      event->pc != UINT32_C(0x800676cc) ||
      event->delay_slot_pc != UINT32_C(0x800676d0) ||
      event->entry != UINT32_C(0x800582dc) || event->argument_count != 2 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask != 0x0f ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word != 1 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 0x0f ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          event->pc + 8u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  Nba97GameActorResumeContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->event = *event;
  ++binding->invocations;
  binding->result = nba97_game_actor_resume(&context, &binding->progress);
  if (binding->result != NBA97_TEXT_ARGUMENT ||
      binding->progress.stopped_pc != 0)
    *machine = binding->progress.machine;
  return binding->result == NBA97_TEXT_COMPLETE;
}
