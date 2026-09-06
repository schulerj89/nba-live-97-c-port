#ifndef NBA97_GAME_BALL_CONTACT_GATE_ADAPTER_H
#define NBA97_GAME_BALL_CONTACT_GATE_ADAPTER_H

#include "game_ball_actor_contact_adapter.h"
#include "recovered/game_ball_contact_gate.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameBallContactGateBinding {
  size_t child_operation_budget;
  Nba97GameBallActorContactIo io;
  void *user;
  Nba97GameBallActorContactAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameBallActorContactBinding contact_binding;
  Nba97GameBallActorContactProgress contact_progress;
  Nba97GameBallActorContactMachine entry_machine;
  Nba97GameBallContactGateEvent event;
  int child_result;
  size_t invocations;
} Nba97GameBallContactGateBinding;

/* Compose the gate's sole 0x800602CC boundary with the complete contact owner.
 * Unresolved contact children remain explicit through io/contact_binding. */
int nba97_game_ball_contact_gate_run(
    Nba97GameBallContactGateContext *, Nba97GameBallContactGateProgress *,
    Nba97GameBallContactGateBinding *);

#ifdef __cplusplus
}
#endif
#endif
