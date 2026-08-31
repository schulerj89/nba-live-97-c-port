#ifndef NBA97_GAME_SCORING_ACTOR_AI_H
#define NBA97_GAME_SCORING_ACTOR_AI_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97ScoringActorAiCall {
    uint32_t pc,entry,argument[3];
    unsigned argument_count;
} Nba97ScoringActorAiCall;

/* The callback is an actual synchronous source call. A successful return
 * means that call's effects are already visible through access; there is no
 * permissive default and none of these boundaries has a consumed return. */
typedef int (*Nba97ScoringActorAiService)(void*,const Nba97ScoringActorAiCall*);

typedef struct Nba97ScoringActorAiContext {
    Nba97PlayerFrameAccess access;
    Nba97ScoringActorAiService service;
    void* user;
    size_t operation_budget;
} Nba97ScoringActorAiContext;

typedef struct Nba97ScoringActorAiProgress {
    size_t operations,reads,stores,services,cpu_leaves,players_visited;
    uint32_t stopped_pc,stopped_address,stopped_entry;
    uint8_t completed;
} Nba97ScoringActorAiProgress;

enum {NBA97_SCORING_ACTOR_AI_SERVICE_REQUIRED=-20};

/* Complete GAME6E7AC and its call-free GAME58AA8/GAME6E734 leaves. All raw
 * addresses are retained source addresses. GAME56FFC (audio/event routing)
 * and GAME7F074/GAME7F20C (UI/actor presentation) are required boundaries.
 * Refusal preserves the exact completed prefix; this is neither transactional
 * nor resumable and does not prove natural gameplay. */
int nba97_game_scoring_actor_ai(Nba97ScoringActorAiContext*,uint32_t* selected,
    Nba97ScoringActorAiProgress*);

#ifdef __cplusplus
}
#endif
#endif
