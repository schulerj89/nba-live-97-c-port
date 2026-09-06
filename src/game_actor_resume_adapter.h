#ifndef NBA97_GAME_ACTOR_RESUME_ADAPTER_H
#define NBA97_GAME_ACTOR_RESUME_ADAPTER_H

#include "recovered/game_actor_resume.h"
#include "recovered/game_period_expiry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameActorResumeBinding {
  size_t operation_budget;
  Nba97GameActorResumeIo io;
  void *user;
  Nba97GameActorResumeAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameActorResumeProgress progress;
  Nba97GamePeriodExpiryEvent event;
  int result;
  size_t invocations;
} Nba97GameActorResumeBinding;

/* Bind only period expiry's full-machine 0x800676CC child event. The event
 * already contains JAL ra and delay-slot a1=1; all actor child services remain
 * explicit through the binding's full-machine callback. */
int nba97_game_actor_resume_from_period_expiry(
    void *, const Nba97GameTextMemory *, const Nba97GamePeriodExpiryEvent *,
    Nba97GamePeriodExpiryMachine *);

#ifdef __cplusplus
}
#endif
#endif
