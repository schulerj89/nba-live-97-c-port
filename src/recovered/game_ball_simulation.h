#ifndef NBA97_GAME_BALL_SIMULATION_H
#define NBA97_GAME_BALL_SIMULATION_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {NBA97_BALL_SIMULATION_SERVICE_REQUIRED=-14};
typedef struct Nba97BallSimulationCall {uint32_t pc,entry,argument[2];unsigned count;} Nba97BallSimulationCall;
/* Actual synchronous named service against the same mutable retained memory.
 * Return BODY_OK only after its source effects complete. No successful stubs.
 * Source v0 is separate: unconsumed bits may remain unknown. The currently
 * requested services do not have a consumed return value. Callbacks may change
 * live state; the owner preserves captured locals and reloads only at actual
 * source accesses. On refusal their completed mutation prefix stays visible. */
typedef int (*Nba97BallSimulationService)(void*,const Nba97BallSimulationCall*,Nba97PlayerFrameValue*);
typedef struct Nba97BallSimulationContext {Nba97PlayerFrameAccess access;Nba97BallSimulationService service;void* user;size_t operation_budget;} Nba97BallSimulationContext;
typedef struct Nba97BallSimulationProgress {
 size_t operations,reads,stores,services,attachments,substeps;
 uint32_t stopped_pc,stopped_address;uint8_t completed;
} Nba97BallSimulationProgress;
/* Complete6EF60 with captured original a0 ball address. Real globals, resources,
 * actor aliases and byte knowledge are required; no zero state is supplied.
 * Reuses actual57F5C/58120 and7066C. Refusals keep every prior source effect.
 * Access is the checked live FrameAccess contract: resolve each reached span,
 * preserve byte knowledge, reject every reached noncanonical metadata byte,
 * and never give unrelated unknown padding a fabricated value. Reads are
 * observational; stores validate before mutating. No whole-object preflight.
 * Private ABI stack/code must not alias visible memory. Budget is host safety,
 * not original elapsed time; progress is not a resumable source PC. */
int nba97_game_ball_simulate(Nba97BallSimulationContext*,uint32_t ball,Nba97BallSimulationProgress*);
int nba97_game_ball_backboard(Nba97BallSimulationContext*,uint32_t ball,Nba97GamePeriodValue* return_v0,Nba97BallSimulationProgress*);
int nba97_game_ball_rim(Nba97BallSimulationContext*,uint32_t ball,Nba97GamePeriodValue* return_v0,Nba97BallSimulationProgress*);
#ifdef __cplusplus
}
#endif
#endif
