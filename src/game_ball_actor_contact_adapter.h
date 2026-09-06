#ifndef NBA97_GAME_BALL_ACTOR_CONTACT_ADAPTER_H
#define NBA97_GAME_BALL_ACTOR_CONTACT_ADAPTER_H

#include "recovered/game_actor_resume.h"
#include "recovered/game_ball_actor_contact.h"
#include "recovered/game_rule_delay.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameBallActorContactBinding {
  Nba97GameActorResumeIo actor_resume_io;
  void *actor_resume_user;
  size_t child_operation_budget;
  Nba97GameActorResumeProgress actor_resume[3];
  Nba97GameRuleDelayProgress rule_delay[4];
  Nba97GameBallActorContactEvent actor_resume_event[3];
  Nba97GameBallActorContactEvent rule_delay_event[4];
  size_t actor_resume_count, rule_delay_count, unresolved_count;
  int child_result;
} Nba97GameBallActorContactBinding;

/* Execute the complete owner while composing every source 0x800582DC and
 * 0x800295C8 site with its complete full-machine owner. */
int nba97_game_ball_actor_contact_run(Nba97GameBallActorContactContext *,
                                      Nba97GameBallActorContactProgress *,
                                      Nba97GameBallActorContactBinding *);

#ifdef __cplusplus
}
#endif
#endif
