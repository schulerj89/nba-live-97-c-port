#ifndef NBA97_GAME_ACTOR_CONTACT_GATE_ADAPTER_H
#define NBA97_GAME_ACTOR_CONTACT_GATE_ADAPTER_H

#include "recovered/game_contact_dispatch.h"
#include "recovered/game_actor_contact_gate.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameActorContactGateBinding {
  size_t operation_budget;
  Nba97GameActorContactGateIo io;
  void *user;
  Nba97GameActorContactGateAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameActorContactGateProgress progress;
  Nba97GameActorContactGateMachine entry_machine;
  Nba97GameContactDispatchEvent event;
  int child_result;
  size_t invocations;
} Nba97GameActorContactGateBinding;

/* Bind the complete owner only to AJ's actual 0x8006104C child event. The
 * 0x8005F948 contact test remains an explicit full-machine dependency. */
int nba97_game_actor_contact_gate_from_contact_dispatch(
    void *, const Nba97GameTextMemory *, const Nba97GameContactDispatchEvent *,
    Nba97GameContactDispatchMachine *);

#ifdef __cplusplus
}
#endif
#endif
