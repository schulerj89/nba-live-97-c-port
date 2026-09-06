#ifndef NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_ADAPTER_H
#define NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_ADAPTER_H

#include "recovered/game_actor_contact_eligibility.h"
#include "recovered/game_actor_contact_gate.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameActorContactEligibilityBinding {
  size_t operation_budget;
  Nba97GameActorContactEligibilityIo io;
  void *user;
  Nba97GameActorContactEligibilityAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameActorContactEligibilityProgress progress;
  int result;
  size_t invocations;
} Nba97GameActorContactEligibilityBinding;

typedef struct Nba97GameActorContactEligibilityGeometryBinding {
  Nba97GameActorContactEligibilityIo fallback;
  void *fallback_user;
  int result;
  size_t geometry_invocations;
  size_t fallback_invocations;
} Nba97GameActorContactEligibilityGeometryBinding;

void nba97_game_actor_contact_eligibility_binding_init(
    Nba97GameActorContactEligibilityBinding *binding, size_t operation_budget,
    Nba97GameActorContactEligibilityIo io, void *user,
    Nba97GameActorContactEligibilityAccess *access_journal,
    size_t access_journal_capacity);

int nba97_game_actor_contact_eligibility_from_actor_contact_gate(
    void *user, const Nba97GameTextMemory *memory,
    const Nba97GameActorContactGateEvent *event,
    Nba97GameActorContactGateMachine *machine);

void nba97_game_actor_contact_eligibility_geometry_binding_init(
    Nba97GameActorContactEligibilityGeometryBinding *binding,
    Nba97GameActorContactEligibilityIo fallback, void *fallback_user);

int nba97_game_actor_contact_eligibility_geometry_child(
    void *user, const Nba97GameTextMemory *memory,
    const Nba97GameActorContactEligibilityEvent *event,
    Nba97GameActorContactEligibilityMachine *machine);

#ifdef __cplusplus
}
#endif
#endif
