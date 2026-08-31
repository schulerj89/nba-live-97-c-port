#ifndef NBA97_GAME_PLAYER_GEOMETRY_H
#define NBA97_GAME_PLAYER_GEOMETRY_H
#include "game_body_geometry.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Named operations used by55368; no CPU/GTE instruction decoder. The two
 * products use rotation/V0, sf=12,lm=0, with either zero or current translation.
 * A successful math call changes only its retained geometry state. It must not
 * mutate input buffers, descriptors, or this request. Return BODY_* results. */
enum Nba97PlayerMathKind {
    NBA97_PLAYER_ROTATION, NBA97_PLAYER_TRANSLATION, NBA97_PLAYER_VERTEX,
    NBA97_PLAYER_ROTATE, NBA97_PLAYER_TRANSFORM, NBA97_PLAYER_IR, NBA97_PLAYER_MAC
};
typedef struct Nba97PlayerMathRequest {uint32_t pc,word;unsigned kind,index;} Nba97PlayerMathRequest;
typedef int (*Nba97PlayerMath)(void*,const Nba97PlayerMathRequest*,Nba97GamePeriodValue*);
typedef struct Nba97GamePlayerGeometryInput {
    Nba97GameBodyBuffer* buffers;size_t buffer_count;
    /* These are locations of the actual mutable global pointer/scalar slots,
     * not their captured values. Slots may alias any owned allocation. */
    Nba97GameBodyReference context_f0ed4,root_10292c;
    Nba97GameBodyReference work_f1c4c,work_f9cf8,work_f9c54,work_f9d00;
    Nba97GameBodyReference foot_f9d04,foot_fea38,hand_f0fb4,hand_fc62c;
    Nba97GameBodyReference angle_103edc,trig_b3254;
    Nba97PlayerMath math;void* math_user;
} Nba97GamePlayerGeometryInput;
typedef struct Nba97GamePlayerGeometryWrite {
    Nba97GameBodyReference destination;uint32_t pc,word;uint8_t width;
} Nba97GamePlayerGeometryWrite;
typedef struct Nba97GamePlayerGeometryProgress {
    Nba97GameBodyReference stopped_reference;
    size_t writes,math_calls;uint32_t stopped_pc;
    uint8_t parts_completed,completed;
} Nba97GamePlayerGeometryProgress;

/* Complete55368,20parts. Consumes actual50768 references,5200C-produced root/
 * work matrices and actual primary/secondary pose angles. This does not create
 * those producers or project body vertices. Body references retain aliases;
 * allocation addresses are not invented. All reached raw accesses validate
 * bounds/alignment/canonical metadata; untouched bytes remain untouched. V0Z
 * and rotationword4 require only low16known, while validating the whole reached
 *4byte span; their discarded highhalves may remain unknown.
 * Scalar reads of reference cells require actual original addresses. A partial
 * halfword overwrite of a reference likewise refuses ADDRESS_REQUIRED, because
 * the untouched encoded half cannot be reconstructed from an allocation id.
 * Pointer reads retain unknown/unresolved values until a dereference; the final
 * parent prefetch is still performed, including bounds/alignment validation.
 * Original wrap, IR saturation vs MAC endpoint output, mirror signs, pose-bank
 * switch, and signed16 endpoint truncation are preserved, not repaired.
 * Journal exhaustion refuses before a store. All earlier CPU stores AND math
 * effects remain; clone both buffers/cells and math state for atomic publication.
 * Scratch stack cannot alias input allocations in this native entry. Descriptors,
 * journal/progress and metadata arrays must be disjoint from mutable byte arrays;
 * allocations are nonoverlapping, with aliases expressed using the same id.
 */
int nba97_game_player_geometry(const Nba97GamePlayerGeometryInput*,
    Nba97GamePlayerGeometryWrite*,size_t,Nba97GamePlayerGeometryProgress*);
#ifdef __cplusplus
}
#endif
#endif
