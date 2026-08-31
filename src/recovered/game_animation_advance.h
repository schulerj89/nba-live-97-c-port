#ifndef NBA97_GAME_ANIMATION_ADVANCE_H
#define NBA97_GAME_ANIMATION_ADVANCE_H
#include "game_player_initialization.h"
#include "game_period.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97GameAnimationExtraField {
    NBA97_ANIM_EXTRA_14=0,NBA97_ANIM_EXTRA_16,NBA97_ANIM_EXTRA_18,
    NBA97_ANIM_EXTRA_52,NBA97_ANIM_EXTRA_56,NBA97_ANIM_EXTRA_5A,NBA97_ANIM_EXTRA_5E,
    NBA97_ANIM_EXTRA_62,NBA97_ANIM_EXTRA_66,NBA97_ANIM_EXTRA_72,NBA97_ANIM_EXTRA_74,
    NBA97_ANIM_EXTRA_76,NBA97_ANIM_EXTRA_7A,NBA97_ANIM_EXTRA_7C,NBA97_ANIM_EXTRA_7E,
    NBA97_ANIM_EXTRA_80,NBA97_ANIM_EXTRA_9C,NBA97_ANIM_EXTRA_9E,NBA97_ANIM_EXTRA_A0,
    NBA97_ANIM_EXTRA_C4,NBA97_ANIM_EXTRA_E4,NBA97_ANIM_EXTRA_E6,NBA97_ANIM_EXTRA_EA,
    NBA97_ANIM_EXTRA_COUNT
};
typedef struct Nba97GameAnimationActor {
    Nba97GameAnimationState animation; /* Existing fields and height10; no duplicates. */
    uint16_t extra[NBA97_ANIM_EXTRA_COUNT];
    uint32_t extra_known; /* One bit per extra halfword; unknown payload is zero. */
    uint8_t queue_blend[2][4],queue_blend_known[2]; /* Primary68..6B, secondary6C..6F. */
    Nba97GamePeriodValue word00,previous_height2c,actor1a;
} Nba97GameAnimationActor;
typedef struct Nba97GameAnimationEffects {
    Nba97GameAnimationActor state;
    uint16_t animation_written;
    uint32_t extra_written;
    uint8_t queue_blend_written[2];
    uint64_t store_count; /* Includes repeated original writes; footprint masks above. */
} Nba97GameAnimationEffects;
typedef struct Nba97GameAnimationClipView {
    Nba97GameMotionHeaderView header;
    uint8_t step3; /* Actual normalized header+3;640D8 halves it on first flag8 conversion. */
} Nba97GameAnimationClipView;
enum Nba97GameAnimationMap {
    NBA97_ANIMATION_MAP_B850C=0,NBA97_ANIMATION_MAP_B8538,NBA97_ANIMATION_MAP_B8564,
    NBA97_ANIMATION_MAP_B8590,NBA97_ANIMATION_MAP_B85BC,NBA97_ANIMATION_MAP_B85E8,
    NBA97_ANIMATION_MAP_B8614,NBA97_ANIMATION_MAP_COUNT
};
typedef struct Nba97GameAnimationMapView {
    const uint16_t* words; /* Actual original signed-halfword lookup window. */
    size_t count;
    int32_t first_index; /* Explicit owned index range; source has no22-entry guard. */
} Nba97GameAnimationMapView;
typedef struct Nba97GameAnimationResources {
    Nba97GameAnimationClipView clip[2][84];
    Nba97GameAnimationMapView map[7];
} Nba97GameAnimationResources;
typedef struct Nba97GameAnimationAdvanceInput {
    Nba97GameAnimationActor previous;
    Nba97GamePeriodValue tick_fdb6c,controlled_fdbcc; /* Canonical raw signed-halfword sources. */
    const Nba97GameAnimationResources* resources;
} Nba97GameAnimationAdvanceInput;
enum Nba97GameAnimationAdvanceResult {
    NBA97_ANIMATION_OK=1,NBA97_ANIMATION_ARGUMENT=0,
    NBA97_ANIMATION_UNRESOLVED=-1,NBA97_ANIMATION_REFERENCE=-2,
    NBA97_ANIMATION_SOURCE_NONTERMINATING=-3
};
unsigned nba97_game_animation_extra_offset(unsigned field);
/* Complete579FC and its5703C/572C0/56DE0/56D30 callees. No physics6CFE0,
 * input61760, pose57B18 or fabricated actor1A transition. Actual queued clip
 * flags can write vertical velocity18 fromC4. Original zero-step infinite loops
 * report SOURCE_NONTERMINATING, never a substituted timing byte. Source quirks
 * include discarded remainder on clip transitions, old-header primary mode2
 * checks, shared blend80 and stale queue tail aux bytes. Unknown retained/copied
 * fields stay unknown. Needed unknown reads or unowned resources fail explicitly.
 * Effects are atomic; input/output byte overlap is supported. Resources remain
 * caller-owned and immutable throughout the call; no original bytes embedded. */
int nba97_game_animation_advance(Nba97GameAnimationEffects* out,
                                const Nba97GameAnimationAdvanceInput* input);
/* Standalone audited572C0 frame advance and5703C base-clip remapping, useful
 * for callers with the exact same typed source fields. Same effect/error rules. */
int nba97_game_animation_advance_frames(Nba97GameAnimationEffects* out,
                                       const Nba97GameAnimationAdvanceInput* input);
int nba97_game_animation_remap(Nba97GameAnimationEffects* out,
                              const Nba97GameAnimationAdvanceInput* input);
#ifdef __cplusplus
}
#endif
#endif
