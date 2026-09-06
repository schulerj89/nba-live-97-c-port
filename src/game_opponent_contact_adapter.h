#ifndef NBA97_GAME_OPPONENT_CONTACT_ADAPTER_H
#define NBA97_GAME_OPPONENT_CONTACT_ADAPTER_H

#include "recovered/game_actor_contact_eligibility.h"
#include "recovered/game_opponent_contact.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameOpponentContactBinding {
  size_t operation_budget;
  Nba97GameOpponentContactIo io;
  void *user;
  Nba97GameOpponentContactAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameOpponentContactProgress progress;
  int result;
  size_t invocations;
} Nba97GameOpponentContactBinding;

void nba97_game_opponent_contact_binding_init(
    Nba97GameOpponentContactBinding *, size_t, Nba97GameOpponentContactIo,
    void *, Nba97GameOpponentContactAccess *, size_t);

/* Compose only actor-contact eligibility's actual 0x8005FA2C other-team
 * full-machine event. The parent's callback already supplies both actor
 * arguments, JAL ra, delay state, retained memory, and byte knownness. */
int nba97_game_opponent_contact_from_actor_contact_eligibility(
    void *, const Nba97GameTextMemory *,
    const Nba97GameActorContactEligibilityEvent *,
    Nba97GameActorContactEligibilityMachine *);

#ifdef __cplusplus
}
#endif
#endif
