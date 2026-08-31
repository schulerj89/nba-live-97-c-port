#ifndef NBA97_GAME_PLAYER_INITIALIZATION_H
#define NBA97_GAME_PLAYER_INITIALIZATION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameAnimationField {
    NBA97_ANIM_46=0, NBA97_ANIM_48, NBA97_ANIM_4A, NBA97_ANIM_4C,
    NBA97_ANIM_4E, NBA97_ANIM_50, NBA97_ANIM_54, NBA97_ANIM_58,
    NBA97_ANIM_5C, NBA97_ANIM_60, NBA97_ANIM_64, NBA97_ANIM_70,
    NBA97_ANIM_78, NBA97_ANIM_94, NBA97_ANIM_96, NBA97_ANIM_9A,
    NBA97_ANIM_FIELD_COUNT
};
typedef struct Nba97GameAnimationState {
    uint16_t field[16];
    uint16_t known; /* One bit per field; UNKNOWN payloads must be zero. */
    int32_t height10;
    uint8_t height_known;
} Nba97GameAnimationState;
typedef struct Nba97GameMotionHeaderView {
    uint16_t flags; /* Actual normalized header+0. */
    uint8_t mode2,count7; /* Actual header bytes; not inferred from frame count. */
    uint8_t available; /* 1 actual header;0 absent;2 unresolved reference. */
} Nba97GameMotionHeaderView;
typedef struct Nba97GameAnimationResetInput {
    Nba97GameAnimationState previous;
    uint16_t motion_index; /* Both views resolve this source directory index. */
    Nba97GameMotionHeaderView motion[2]; /* Primary1EC98 / secondary170C8. */
} Nba97GameAnimationResetInput;
typedef struct Nba97GameAnimationResetEffects {
    Nba97GameAnimationState state;
    uint16_t written;
} Nba97GameAnimationResetEffects;

enum Nba97GamePlayerInitResult {
    NBA97_PLAYER_INIT_OK=1, NBA97_PLAYER_INIT_ARGUMENT=0,
    NBA97_PLAYER_INIT_UNRESOLVED=-1, NBA97_PLAYER_INIT_MOTION_REFERENCE=-2,
    NBA97_PLAYER_INIT_STORAGE=-3
};

/* Exact56FFC(entity,1) path, including56F5C->56AA4 then56EBC->5699C.
 * These callees execute here, not as callback stubs. Force0 and independent
 * general motion-setter calls are outside this interface. Motion derives from
 * nonzero height ?37:field4E; actual primary/secondary header views must match.
 * Unknown prior fields survive as UNKNOWN when retained/copied. An unknown
 * value needed to decide a branch returns UNRESOLVED, never a guessed frame.
 * The status9A bit clears preserve unknown provenance until a later producer.
 * Effects identify every written halfword; writing an unknown source value is
 * explicit (written bit1, known bit0), not permission to store native zero.
 * Return1 describes a projection, not a fully known or renderable actor. All
 * failures leave output unchanged; input/output byte overlap is supported. */
int nba97_game_player_animation_force_reset(Nba97GameAnimationResetEffects* out,
                                           const Nba97GameAnimationResetInput* input);

typedef struct Nba97GamePlayerInitializationInput {
    uint16_t side_word; /* Source header+14, native owned ten-player range0..5. */
    int16_t period;
    int32_t direction10,special_center;
    uint32_t duration,previous_cumulative48;
    uint16_t previous_b4,header32;
    uint8_t cumulative_known,sum_known;
    int16_t formation[5][3]; /* Five actual source signed coordinate/angle triples. */
    uint8_t player_byte_d[5]; /* Resolved through existing entity+20 player refs. */
    uint8_t player_byte_d_known[5];
    Nba97GameAnimationState previous_animation[5];
    Nba97GameMotionHeaderView motion0[2]; /* Actual normalized directory0 headers. */
} Nba97GamePlayerInitializationInput;

typedef struct Nba97GamePlayerEntityInitialization {
    uint8_t value[244],written[244],known[244];
    uint8_t entity_index,table_slot; /* FDCC0 registration resolves this entity. */
} Nba97GamePlayerEntityInitialization;
typedef struct Nba97GamePlayerInitializationEffects {
    uint8_t header_value[196],header_written[196];
    Nba97GamePlayerEntityInitialization entity[5]; /* Source local0..4 order. */
    uint16_t unresolved_written_bytes;
} Nba97GamePlayerInitializationEffects;

/* Complete65B18 direct effects and actual56FFC(force1) transitive path.
 * Source byte offsets in this effect object are a field-write description,
 * never a fabricated full entity/header. Apply only written fields. Each byte
 * additionally says whether the written value is known. Unwritten bytes have
 * no effect; unknown written bytes are not zero defaults and must be resolved
 * from owned prior animation state before a concrete runtime can consume them.
 * Preserves wrapping header sums, signed coordinate transforms, player+D center
 * mirroring, original animation-frame retention/synchronization and source
 * write widths. Does not change player/status bindings, opponent indices or
 * controller selection. FDCC0 registration uses address-free entity indices.
 * Missing needed data/ref, invalid tokens, or outside-ten-player side returns
 * a native error atomically. No loading, GTE, input, source VM or PS1 pointers.
 */
int nba97_game_player_initialize(Nba97GamePlayerInitializationEffects* out,
                                 const Nba97GamePlayerInitializationInput* input);

#ifdef __cplusplus
}
#endif
#endif
