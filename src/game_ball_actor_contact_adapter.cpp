#include "game_ball_actor_contact_adapter.h"

namespace {
struct Dispatch {
  Nba97GameBallActorContactBinding *b;
  Nba97GameBallActorContactIo saved;
  void *saved_user;
};

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameBallActorContactEvent *event,
             Nba97GameBallActorContactMachine *machine) {
  auto *d = static_cast<Dispatch *>(opaque);
  auto *b = d->b;
  if (event->entry == UINT32_C(0x800295c8)) {
    const bool valid_site = (event->pc == UINT32_C(0x80060788) ||
                             event->pc == UINT32_C(0x80060b38) ||
                             event->pc == UINT32_C(0x80060b6c) ||
                             event->pc == UINT32_C(0x80060c7c));
    if (!valid_site || event->delay_slot_pc != event->pc + 4u ||
        event->kind != NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_800295C8 ||
        event->argument_count != 1 ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
            event->pc + 8u ||
        b->rule_delay_count >= 4) {
      b->child_result = NBA97_TEXT_ARGUMENT;
      return 0;
    }
    Nba97GameRuleDelayContext c{};
    c.machine = *machine;
    const Nba97GameBallActorContactMachine before = *machine;
    const size_t index = b->rule_delay_count++;
    b->rule_delay_event[index] = *event;
    b->child_result = nba97_game_rule_delay(&c, &b->rule_delay[index]);
    if (b->child_result == NBA97_TEXT_ARGUMENT)
      *machine = before;
    else
      *machine = b->rule_delay[index].machine;
    return b->child_result == NBA97_TEXT_COMPLETE;
  }
  if (event->entry == UINT32_C(0x800582dc)) {
    const bool valid_site = event->pc == UINT32_C(0x800609b4) ||
                            event->pc == UINT32_C(0x800609e0) ||
                            event->pc == UINT32_C(0x80060ab4);
    if (!valid_site || event->delay_slot_pc != event->pc + 4u ||
        event->kind != NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_800582DC ||
        event->argument_count != 2 ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
            event->pc + 8u ||
        b->actor_resume_count >= 3) {
      b->child_result = NBA97_TEXT_ARGUMENT;
      return 0;
    }
    const size_t index = b->actor_resume_count++;
    b->actor_resume_event[index] = *event;
    const Nba97GameBallActorContactMachine before = *machine;
    Nba97GameActorResumeContext c{};
    c.memory = *memory;
    c.machine = *machine;
    c.operation_budget = b->child_operation_budget;
    c.io = b->actor_resume_io;
    c.user = b->actor_resume_user;
    b->child_result = nba97_game_actor_resume(&c, &b->actor_resume[index]);
    if (b->child_result == NBA97_TEXT_ARGUMENT)
      *machine = before;
    else
      *machine = b->actor_resume[index].machine;
    return b->child_result == NBA97_TEXT_COMPLETE;
  }
  ++b->unresolved_count;
  return d->saved ? d->saved(d->saved_user, memory, event, machine) : 0;
}
} // namespace

int nba97_game_ball_actor_contact_run(
    Nba97GameBallActorContactContext *context,
    Nba97GameBallActorContactProgress *progress,
    Nba97GameBallActorContactBinding *binding) {
  if (!context || !progress || !binding)
    return NBA97_TEXT_ARGUMENT;
  Dispatch d{binding, context->io, context->user};
  binding->actor_resume_count = 0;
  binding->rule_delay_count = 0;
  binding->unresolved_count = 0;
  binding->child_result = NBA97_TEXT_COMPLETE;
  context->io = dispatch;
  context->user = &d;
  int result = nba97_game_ball_actor_contact(context, progress);
  context->io = d.saved;
  context->user = d.saved_user;
  return result;
}
