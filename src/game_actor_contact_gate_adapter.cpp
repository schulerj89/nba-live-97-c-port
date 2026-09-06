#include "game_actor_contact_gate_adapter.h"

int nba97_game_actor_contact_gate_from_contact_dispatch(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameContactDispatchEvent *event,
    Nba97GameContactDispatchMachine *machine) {
  auto *binding = static_cast<Nba97GameActorContactGateBinding *>(opaque);
  if (!binding || !memory || !event || !machine ||
      event->pc != UINT32_C(0x8006104c) ||
      event->delay_slot_pc != UINT32_C(0x80061050) ||
      event->entry != UINT32_C(0x8005faa8) ||
      event->kind != NBA97_GAME_CONTACT_DISPATCH_CHILD_8005FAA8 ||
      event->argument_count != 2 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          event->pc + 8u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->child_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  Nba97GameActorContactGateContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->event = *event;
  binding->entry_machine = *machine;
  ++binding->invocations;
  binding->child_result =
      nba97_game_actor_contact_gate(&context, &binding->progress);
  if (binding->child_result != NBA97_TEXT_ARGUMENT ||
      binding->progress.stopped_pc != 0)
    *machine = binding->progress.machine;
  return binding->child_result == NBA97_TEXT_COMPLETE;
}
