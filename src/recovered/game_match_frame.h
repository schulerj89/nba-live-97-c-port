#ifndef NBA97_GAME_MATCH_FRAME_H
#define NBA97_GAME_MATCH_FRAME_H
#include "game_player_frame.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete49018 CPU sequencing, including the535C8/55F0C scratch sentinel
 * check. This does not invent match entry, resources, device state or callees.
 * Every reached IO entry must execute its actual native owner/platform service.
 * args contain only source-consumed arguments, not incidental register values.
 */
typedef struct Nba97MatchFrameCall {uint32_t pc,entry,args[2];} Nba97MatchFrameCall;
typedef int (*Nba97MatchFrameIo)(void*,const Nba97MatchFrameCall*,Nba97GamePeriodValue*);
typedef struct Nba97MatchFrameContext {
    Nba97PlayerFrameAccess access;Nba97MatchFrameIo io;void* user;
    size_t operation_budget;
} Nba97MatchFrameContext;
typedef struct Nba97MatchFrameProgress {
    size_t operations,reads,stores,calls,frames,actor_updates,submissions;
    uint32_t stopped_pc,stopped_address,stopped_entry;
    uint8_t completed;
} Nba97MatchFrameProgress;
enum {NBA97_MATCH_FRAME_IO_REQUIRED=-12};
/* Required services include530FC poses,48FF4/4900C interrupt state,99960 table
 * initialization,56074/5605C geometry controls,51098 camera,75D40,4B1A4 nets,
 * 52914 players,4AC68 court,57F5C/58120/581C0 attachment,49300/49D34 ball,
 * 35BEC labels,994F4 sync,4A044 actor work,99CA4/99ACC display/draw environment,
 * 99A58 submission,319B0 text retirement.48FF4 returns known original status;
 * other returns are unused. Missing services refuse at the reached call.
 * Synchronous callees may mutate RAM and geometry, never context descriptors.
 * Metadata/byte-knowledge rules are identical to game_player_frame.h.
 * Source545C4/545E0 change private ABI stacks only; those bytes cannot alias
 * visible storage. Public scratch004/030..03C remain actual required inputs.
 * Failure retains all preceding writes/calls; no automatic interrupt restore,
 * state rollback, table clear or retry occurs. Clone/rebind whole owners before
 * running if atomic publication is needed. A bounded failure is not a frame.
 */
int nba97_game_match_frame(Nba97MatchFrameContext*,Nba97MatchFrameProgress*);
#ifdef __cplusplus
}
#endif
#endif
