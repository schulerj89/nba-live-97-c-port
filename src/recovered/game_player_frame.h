#ifndef NBA97_GAME_PLAYER_FRAME_H
#define NBA97_GAME_PLAYER_FRAME_H
#include "game_player_projection.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 52914 and its shadow/indicator CPU leaves. Original addresses are required:
 * these are actual mutable globals, entity tables, resources and packet links,
 * not invented addresses assigned to native objects. Reads/writes validate only
 * the reached span (all metadata before unknown-data refusal). Metadata values
 * are canonical 0/1. Opaque AA468 copies preserve each byte's knowledge.
 * READ/WRITE widths1/2/3/4 use little-endian bytes; width3 is aligned low24.
 * WRITE_POINTER is a fullword source pointer store, allowing a retained-resource
 * adapter to preserve normalized reference identity. Unknown value bytes have
 * zero bits in word, without clearing the underlying unknown memory bytes.
 * Reads are observational; successful writes perform exactly that store and
 * do not alter the request/value or other source state. All
 * earlier writes survive any refusal. Memory/metadata/descriptors must not alias.
 */
enum Nba97PlayerFrameAccessKind {NBA97_FRAME_READ,NBA97_FRAME_WRITE,NBA97_FRAME_WRITE_POINTER};
/* A fullword opaque copy may also carry the normalized reference identity.
 * This is native representation metadata, not an extra source memory access.
 * It must encode the SAME word; unknown references have mask0/word0. */
typedef struct Nba97PlayerFrameValue {
    uint32_t word;uint8_t known_mask;
    Nba97GameBodyReference reference;uint8_t is_reference;
} Nba97PlayerFrameValue;
typedef int (*Nba97PlayerFrameAccess)(void*,uint32_t pc,uint32_t address,
    unsigned width,unsigned kind,Nba97PlayerFrameValue*);
enum Nba97PlayerFrameMathKind {
    NBA97_FRAME_PROJECT_ONE=16,NBA97_FRAME_IR0,NBA97_FRAME_FLAGS,NBA97_FRAME_DEPTH
};
/* Other math kinds are NBA97_PROJECTION_*. Same retained geometry as all three
 * child owners; math must not mutate RAM. Return BODY_* status codes. */
typedef int (*Nba97PlayerFrameChild)(void*,uint32_t call_pc,uint32_t entry);
typedef struct Nba97PlayerFrameContext {
    Nba97PlayerFrameAccess access;Nba97PlayerMath math;
    /* Required actual5200C/55368/525AC composition, not a success stub. */
    Nba97PlayerFrameChild child;void* user;size_t operation_budget;
} Nba97PlayerFrameContext;
typedef struct Nba97PlayerFrameProgress {
    size_t operations,reads,stores,math_calls,child_calls,actors,shadows,indicators,links;
    uint32_t stopped_pc,stopped_address;uint8_t completed;
} Nba97PlayerFrameProgress;
enum {NBA97_FRAME_CHILD_REQUIRED=-9,NBA97_FRAME_MATH_REQUIRED=-10,
      NBA97_FRAME_ARITHMETIC_TRAP=-11};
/* Private ABI stack is excluded, including545C4/545E0's actual switch to
 * scratchSP1F8003F8 and saved callerSP word there. These are NOT timer calls.
 * Source stack/code cannot alias any visible input allocation; this entry does
 * not invent an original numeric SP or expose its private saved-register bytes.
 * All visible52914 effects and live child/reload order are retained. This is a
 * player rendering pass, not a camera/resource loader or whole game frame. */
int nba97_game_player_frame(Nba97PlayerFrameContext*,Nba97PlayerFrameProgress*);
int nba97_game_player_shadow(Nba97PlayerFrameContext*,Nba97PlayerFrameProgress*);
int nba97_game_player_indicator(Nba97PlayerFrameContext*,Nba97PlayerFrameProgress*);
/* Exactly the aligned40byte AA468 domain reached by4C828. Exposes overlapping
 * source/destination behavior for validation, including backward redundant
 * LWL/LWR and SWL/SWR accesses. Other widths/alignment are outside this entry. */
int nba97_game_player_copy40(Nba97PlayerFrameContext*,uint32_t source,
    uint32_t destination,Nba97PlayerFrameProgress*);
#ifdef __cplusplus
}
#endif
#endif
