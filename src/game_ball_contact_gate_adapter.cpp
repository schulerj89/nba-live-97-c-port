#include "game_ball_contact_gate_adapter.h"

namespace {
struct Dispatch {
  Nba97GameBallContactGateBinding *binding;
};

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameBallContactGateEvent *event,
             Nba97GameBallContactGateMachine *machine) {
  auto *state = static_cast<Dispatch *>(opaque);
  auto *binding = state ? state->binding : nullptr;
  if (!binding || !memory || !event || !machine ||
      event->pc != UINT32_C(0x80060ed4) ||
      event->delay_slot_pc != UINT32_C(0x80060ed8) ||
      event->entry != UINT32_C(0x800602cc) ||
      event->kind != NBA97_GAME_BALL_CONTACT_GATE_CHILD_800602CC ||
      event->argument_count != 3 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          event->pc + 8u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->child_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  Nba97GameBallActorContactContext context{};
  context.memory = *memory;
  context.operation_budget = binding->child_operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->event = *event;
  binding->entry_machine = *machine;
  ++binding->invocations;
  binding->child_result = nba97_game_ball_actor_contact_run(
      &context, &binding->contact_progress, &binding->contact_binding);
  if (binding->child_result != NBA97_TEXT_ARGUMENT ||
      binding->contact_progress.stopped_pc != 0)
    *machine = binding->contact_progress.machine;
  return binding->child_result == NBA97_TEXT_COMPLETE;
}
} // namespace

int nba97_game_ball_contact_gate_run(
    Nba97GameBallContactGateContext *context,
    Nba97GameBallContactGateProgress *progress,
    Nba97GameBallContactGateBinding *binding) {
  if (!context || !progress || !binding)
    return NBA97_TEXT_ARGUMENT;
  Dispatch state{binding};
  Nba97GameBallContactGateIo saved = context->io;
  void *saved_user = context->user;
  binding->invocations = 0;
  binding->child_result = NBA97_TEXT_COMPLETE;
  context->io = dispatch;
  context->user = &state;
  int result = nba97_game_ball_contact_gate(context, progress);
  context->io = saved;
  context->user = saved_user;
  return result;
}
