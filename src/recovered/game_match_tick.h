#ifndef NBA97_GAME_MATCH_TICK_H
#define NBA97_GAME_MATCH_TICK_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97MatchTickCall {
    uint32_t pc,entry,args[2];unsigned count;
} Nba97MatchTickCall;
/* Every generic callback is an actual synchronous GAME/platform service. A
 * successful callback means its source effects are already visible through
 * access. Only callbacks whose v0 is consumed receive a non-null result. */
typedef int (*Nba97MatchTickService)(void*,const Nba97MatchTickCall*,Nba97GamePeriodValue*);
/* These four distinct boundaries bind the already recovered owners without
 * disguising them as successful generic stubs. The argument is the original
 * call PC; ball additionally receives the exact captured 68D90 a0. */
typedef int (*Nba97MatchTickPlayerUpdate)(void*,uint32_t);
typedef int (*Nba97MatchTickBallSimulation)(void*,uint32_t,uint32_t);
typedef int (*Nba97MatchTickNetTransform)(void*,uint32_t);
typedef int (*Nba97MatchTickMatchFrame)(void*,uint32_t);
typedef struct Nba97MatchTickContext {
    Nba97PlayerFrameAccess access;
    Nba97MatchTickService service;
    Nba97MatchTickPlayerUpdate player_update;
    Nba97MatchTickBallSimulation ball_simulation;
    Nba97MatchTickNetTransform net_transform;
    Nba97MatchTickMatchFrame match_frame;
    void* user;size_t operation_budget;
    /* GAME68D38 can carry caller s6 into 67A60 before this function assigns
     * s6, and GAME68F98 can publish it when the timing block is skipped.
     * This is an original compiler/register quirk, not zero state. */
    Nba97GamePeriodValue incoming_s6;
} Nba97MatchTickContext;
typedef struct Nba97MatchTickProgress {
    size_t operations,reads,stores,services,player_updates,ball_ticks;
    size_t net_transforms,frame_pumps,simulation_steps,outer_restarts;
    uint32_t stopped_pc,stopped_address,stopped_entry;
    uint8_t completed;
} Nba97MatchTickProgress;
enum {
    NBA97_MATCH_TICK_SERVICE_REQUIRED=-15,
    NBA97_MATCH_TICK_PLAYER_UPDATE_REQUIRED=-16,
    NBA97_MATCH_TICK_BALL_SIMULATION_REQUIRED=-17,
    NBA97_MATCH_TICK_NET_TRANSFORM_REQUIRED=-18,
    NBA97_MATCH_TICK_MATCH_FRAME_REQUIRED=-19
};
/* Complete GAME68BF8 plus its reached GAME2DD84 frame pump. Source loops are
 * retained and bounded only by operation_budget. Refusal publishes the exact
 * completed memory/call prefix; this API is not resumable or transactional.
 * Live globals are reread only where GAME does, including FDC48 capture then
 * FDC3C publication before 6EF60. No callback may be a successful no-op. */
int nba97_game_match_tick(Nba97MatchTickContext*,Nba97MatchTickProgress*);

#ifdef __cplusplus
}
#endif
#endif
