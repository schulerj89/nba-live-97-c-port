#ifndef NBA97_GAME_COURT_ROSTER_STARTUP_H
#define NBA97_GAME_COURT_ROSTER_STARTUP_H
#include "game_text_objects.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97GameCourtRosterEventKind {NBA97_COURT_ROSTER_STORE=0,NBA97_COURT_ROSTER_STRCMP_A0_17=1};
typedef struct Nba97GameCourtRosterEvent {
    uint32_t pc,address,value,argument[2];
    uint8_t kind,width,completed;
} Nba97GameCourtRosterEvent;
typedef struct Nba97GameCourtRosterContext {Nba97GameTextMemory memory;size_t access_budget;} Nba97GameCourtRosterContext;
typedef struct Nba97GameCourtRosterProgress {
    size_t accesses,events,stores,comparisons,matches_completed;
    uint32_t stopped_pc,stopped_address,match_result;
    uint8_t special_roster_seen,completed;
} Nba97GameCourtRosterProgress;
/* Complete479B8..47CB8 prefix, full4781C and536A0, literal-address getters
 * 56790/567A0/567B0, and55F00/55F0C word wrappers. The9CB5C BIOS tail thunk
 * selects A0/17 strcmp. This adapter owns its required case-sensitive equality
 * semantics over actual retained bytes; only zero/nonzero is consumed. The
 * BIOS service is modeled read-only, byte-by-byte left then right until a
 * difference or sharedNUL; it is NOT ROM execution or an exact ROMread trace.
 * Journal value for strcmp is0(equal)/1(different), not a claimed rawBIOSv0.
 * BIOS semantic-read failures report PC000000A0; other PCs are source loads/
 * stores. No asynchronous writers or source-code/stack aliases are modeled.
 *
 * Caller supplies actualFC664 roster pointers, resident paired strings at
 * B7254/B726C/B7284, writable rosters/tags/DCF10/threehalfwords and the actual
 * scratchpad mapping1F800000. Scratchpad is never invented zeroed storage.
 * Original pointer arithmetic wraps32; repeated source reads and the17-byte
 * signedpointer loop retain their order. Prefix clearsDCF10 but does not set
 * other unknown incoming values. Matching is called TWICE for selected slots;
 * second result and cached roster pointer are not replaced by the first call.
 * 47A2C intentionally writes scratchpad+4 from the just-read+14 value XOR1.
 * The laterclamps happen only if any firstcallmatched, affect all24rosters,
 * then overwrite selected fields for every tag other thanFF, including tag0.
 *
 * Memory metadata/lifetimes are fixed, source regions disjoint; native backing
 * aliases follow textmemory. Context/progress/journal do not overlap each
 * other or any retained data/knownness. Reached canonicalbyteknowledge only;
 * no preflight/default ratings/implicitclear. Budget and ownership refusals
 * retain exact earlier prefixes, are not original rejection branches, and
 * are not resumable. Stage all owned memory together for atomic publication.
 * This is not479B8's remaining interactive/resource/packet intervals, the
 * existing courtbridge, realresourcearrival, full52C20 or naturalmatchentry.
 */
int nba97_game_court_roster_startup(Nba97GameCourtRosterContext*,
    Nba97GameCourtRosterEvent*,size_t capacity,Nba97GameCourtRosterProgress*);
int nba97_game_court_roster_match(Nba97GameCourtRosterContext*,uint32_t index,
    Nba97GameCourtRosterEvent*,size_t capacity,Nba97GameCourtRosterProgress*);
int nba97_game_court_roster_scratch_init(Nba97GameCourtRosterContext*,
    Nba97GameCourtRosterEvent*,size_t capacity,Nba97GameCourtRosterProgress*);
#ifdef __cplusplus
}
#endif
#endif
