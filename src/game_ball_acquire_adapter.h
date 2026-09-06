#ifndef NBA97_GAME_BALL_ACQUIRE_ADAPTER_H
#define NBA97_GAME_BALL_ACQUIRE_ADAPTER_H

#include "recovered/game_ball_acquire.h"
#include "recovered/game_rule_delay.h"
#include "game_ball_actor_contact_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameBallAcquireDelaySite {
    NBA97_GAME_BALL_ACQUIRE_CHANGE_LONG_DELAY = 0,
    NBA97_GAME_BALL_ACQUIRE_CHANGE_SHORT_DELAY,
    NBA97_GAME_BALL_ACQUIRE_SAME_LONG_DELAY,
    NBA97_GAME_BALL_ACQUIRE_SAME_SHORT_DELAY,
    NBA97_GAME_BALL_ACQUIRE_DELAY_SITE_COUNT
};

typedef struct Nba97GameBallAcquireAdapterProgress {
    size_t delay_invocations;
    size_t delay_site_invocations[NBA97_GAME_BALL_ACQUIRE_DELAY_SITE_COUNT];
    size_t duration_10000_invocations;
    size_t duration_20000_invocations;
    size_t unresolved_callbacks_completed;
    int delay_result;
    Nba97GameBallAcquireEvent event[NBA97_GAME_BALL_ACQUIRE_DELAY_SITE_COUNT];
    Nba97GameRuleDelayProgress delay[NBA97_GAME_BALL_ACQUIRE_DELAY_SITE_COUNT];
} Nba97GameBallAcquireAdapterProgress;

typedef struct Nba97GameBallAcquireNaturalProgress {
    size_t acquisition_operation_budget;
    Nba97GameBallAcquireIo acquisition_io;
    void* acquisition_user;
    size_t acquisition_count;
    size_t unresolved_contact_callbacks_completed;
    int acquisition_result;
    Nba97GameBallActorContactEvent acquisition_event;
    Nba97GameBallAcquireProgress acquisition;
    Nba97GameBallAcquireAdapterProgress acquisition_adapter;
} Nba97GameBallAcquireNaturalProgress;

/* Compose one of AK's four source-proven 0x800295C8 calls through the actual
 * recovered full-machine leaf. The leaf consumes live ra and preserves every
 * GPR/HI/LO value and known byte. */
int nba97_game_ball_acquire_rule_delay(
    const Nba97GameBallAcquireEvent*, Nba97GameBallAcquireMachine*,
    Nba97GameRuleDelayProgress*);

/* Run the actual AK owner and compose every 0x800295C8 site. Calls to
 * 0x8002AB70, 0x80072C40, 0x80029590, 0x80035318, and 0x8005CE4C remain the
 * caller's typed Nba97GameBallAcquireIo boundaries. In particular, the
 * existing narrow RNG API is not promoted into a fabricated full-machine ABI. */
int nba97_game_ball_acquire_with_rule_delay(
    const Nba97GameBallAcquireContext*, Nba97GameBallAcquireProgress*,
    Nba97GameBallAcquireAdapterProgress*);

/* Run the actual 0x800602CC contact owner through its established production
 * binding and compose its sole 0x8006089C acquisition event into AK. Other AH
 * events continue through the contact context's typed callback. */
int nba97_game_ball_actor_contact_with_ball_acquire(
    Nba97GameBallActorContactContext*, Nba97GameBallActorContactProgress*,
    Nba97GameBallActorContactBinding*, Nba97GameBallAcquireNaturalProgress*);

#ifdef __cplusplus
}
#endif
#endif
