#ifndef NBA97_GAME_PLAYER_PROJECTION_H
#define NBA97_GAME_PLAYER_PROJECTION_H
#include "game_player_geometry.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97PlayerProjectionMathKind {
    NBA97_PROJECTION_ROTATION, NBA97_PROJECTION_TRANSLATION,
    NBA97_PROJECTION_VERTEX, NBA97_PROJECTION_SCREEN_LOAD,
    NBA97_PROJECTION_THREE, NBA97_PROJECTION_CLIP,
    NBA97_PROJECTION_AVERAGE_THREE, NBA97_PROJECTION_SCREEN,
    NBA97_PROJECTION_MAC0, NBA97_PROJECTION_DEPTH
};
/* Original numeric allocation bases are needed only when a reached packet
 * splice observes address bits. Unknown is canonical {0,0}; never manufacture
 * bases from native pointers or allocation numbers. */
typedef struct Nba97PlayerProjectionAddress {uint32_t word;uint8_t known;} Nba97PlayerProjectionAddress;
typedef struct Nba97GamePlayerProjectionInput {
    Nba97GameBodyBuffer* buffers;size_t buffer_count;
    const Nba97PlayerProjectionAddress* addresses;size_t address_count;
    Nba97GameBodyReference context_f0ed4,bank_1ede8,ordering_102924;
    Nba97GameBodyReference mask_1f80000c,index_1029b0,suppress_dcf10;
    Nba97PlayerMath math;void* math_user;
} Nba97GamePlayerProjectionInput;
typedef struct Nba97GamePlayerProjectionProgress {
    Nba97GameBodyReference stopped_reference;
    size_t writes,math_calls;uint32_t stopped_pc;uint8_t completed;
} Nba97GamePlayerProjectionProgress;
enum {NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT=-8};
/* Complete525AC,54660,5483C,54A18,54ADC and55F0C/55F18/55F44.
 * All six input references name live slots, not snapshots of their contents.
 * Reuses normalized50768 views and actual5200C/55368 matrices. Math shares
 * retained camera, FIFOs and AVSZ3 scale; it may not mutate these buffers.
 * Source stack must not alias supplied allocations. Reached LW/SW require
 * source alignment. Low24 LWL/SWL spans require word-aligned bases in this
 * native domain (otherwise UNSUPPORTED_ALIGNMENT, not an invented CPU trap).
 * Packet high tag bytes survive low24 stores, including their knownness.
 * Counts/banks are not repaired or clamped. Source zero/one-count behavior,
 * dead next-corner prefetches, opposite winding tests, and link/store order
 * remain intact. Bounds/knowledge/journal refusal retains the precise prefix;
 * clone both memory and geometry for atomic publication; do not retry in place.
 */
int nba97_game_player_projection(const Nba97GamePlayerProjectionInput*,
    Nba97GamePlayerGeometryWrite*,size_t,Nba97GamePlayerProjectionProgress*);
/* Named complete leaves for independent resource consumers and validation.
 * group must be54660,5483C or54ADC; assembled must be54A18. Four references
 * are source arguments a0,a1,a2,a3 where applicable; count is raw a3 for
 * groups, raw a2 for assembled. ordering/mask/bias are actual stack words. */
int nba97_game_player_project_group(const Nba97GamePlayerProjectionInput*,uint32_t group,
    Nba97GameBodyReference xyz,Nba97GameBodyReference packets,Nba97GameBodyReference depth,
    uint32_t count,Nba97GameBodyReference ordering,uint32_t mask,uint32_t bias,
    Nba97GamePlayerGeometryWrite*,size_t,Nba97GamePlayerProjectionProgress*);
int nba97_game_player_assemble(const Nba97GamePlayerProjectionInput*,
    Nba97GameBodyReference packets,Nba97GameBodyReference corners,uint32_t count,
    Nba97GameBodyReference depths,Nba97GameBodyReference ordering,
    Nba97GamePlayerGeometryWrite*,size_t,Nba97GamePlayerProjectionProgress*);
#ifdef __cplusplus
}
#endif
#endif
