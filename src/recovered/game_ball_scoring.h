#ifndef NBA97_GAME_BALL_SCORING_H
#define NBA97_GAME_BALL_SCORING_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { NBA97_BALL_SCORING_SERVICE_REQUIRED=-14 };
typedef struct Nba97BallScoringCall {
    uint32_t pc,entry,argument[3];
    unsigned count,return_bytes;
} Nba97BallScoringCall;
typedef int (*Nba97BallScoringService)(void*,const Nba97BallScoringCall*,Nba97PlayerFrameValue*);
typedef struct Nba97BallScoringContext {
    Nba97PlayerFrameAccess access;
    Nba97BallScoringService service;
    void* user;
    size_t operation_budget;
} Nba97BallScoringContext;
typedef struct Nba97BallScoringProgress {
    size_t operations,reads,stores,services,grid_tests;
    uint32_t stopped_pc,stopped_address;
    uint8_t completed;
} Nba97BallScoringProgress;

/* Complete GAME6DC18 caller with its CPU-only immediate leaves. The captured
 * ball address is the original a0 from GAME6F5D0. Required services execute
 * synchronously against this same live memory and retain their mutation
 * prefix on refusal. No default court, actor, AI, audio, RNG or UI state. */
int nba97_game_ball_scoring(Nba97BallScoringContext*,uint32_t ball,
                            Nba97BallScoringProgress*);
#ifdef __cplusplus
}
#endif
#endif
