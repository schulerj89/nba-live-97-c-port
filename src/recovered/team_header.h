#ifndef NBA97_TEAM_HEADER_H
#define NBA97_TEAM_HEADER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97TeamHeaderRefKind {
    NBA97_TEAM_REF_UNKNOWN=0,
    NBA97_TEAM_REF_NULL=1,
    NBA97_TEAM_REF_ENTITY=2,
    NBA97_TEAM_REF_OPAQUE_WORD=3
};
typedef struct Nba97TeamHeaderRef {
    uint32_t payload;
    uint8_t kind;
} Nba97TeamHeaderRef;

typedef struct Nba97TeamHeaderInput {
    uint16_t side_word,opponent_side_word; /* Ordinary caller pairs0/5 or5/0. */
    uint8_t count,injury_slot,difficulty,rank54,rank57;
    uint16_t lineup[5]; /* Raw first five header+16 halfwords; no lookup here. */
    Nba97TeamHeaderRef table12,table24; /* Decimal indices in source20BEC. */
} Nba97TeamHeaderInput;

typedef struct Nba97TeamHeaderEntityEffect {
    uint16_t table_slot,entity_id,opponent_d6;
} Nba97TeamHeaderEntityEffect;
typedef struct Nba97TeamHeaderEffects {
    Nba97TeamHeaderRef word08,word0c;
    int32_t direction10;
    uint16_t field62,count66,count68,field72,field74;
    uint16_t saved_lineup[5]; /* Header+98..A1, indexed0..4. */
    uint16_t status[12]; /* Side's status records, halfword+20, stride22(hex). */
    Nba97TeamHeaderEntityEffect entity[5]; /* Source write order: local4..0. */
    /* Native side selectors0home/1away resolve owned references corresponding
     * to header+4,+6C,+7C. These selectors are not original pointer values. */
    uint8_t opponent_side,metadata_side,alias_side;
    uint8_t field34,field38,field39;
} Nba97TeamHeaderEffects;

/* All655B0 owned effects for the ordinary side0/5 caller, before65328/65DB0.
 * Full original owner800655B0..80065820 has156 instructions. This semantic
 * effect object does not clear or fabricate other bytes of the C4 team header,
 * entity/status records, or entity table. Caller resolves/applies its effects.
 * Count/injury/difficulty/ranks use all raw unsigned-byte values; the first
 * five lineup halfwords copy verbatim. Count0 still registers five entities.
 * Threshold subtraction preserves low16 bits, including synthetic underflow.
 * Home word08 is the entity0 reference registered in this call. Other word08/
 * word0c values copy table12/24 tokens; these fields are not claimed as bounds.
 * UNKNOWN/NULL require payload0, ENTITY accepts IDs0..9, OPAQUE_WORD accepts
 * every32-bit value (including0). No token is dereferenced or normalized.
 * Null pointers, noncomplementary side pairs, invalid kind/payload refuse with
 * return0 and unchanged output. All input tokens validate, even if not read by
 * that side's source branch. Success returns1; overlapping byte ranges are
 * supported by capturing input before output publication. No runtime, period,
 * strategy application, allocation, profile persistence or RNG is performed. */
int nba97_team_header_initialize(Nba97TeamHeaderEffects* out,
                                const Nba97TeamHeaderInput* input);

#ifdef __cplusplus
}
#endif
#endif
