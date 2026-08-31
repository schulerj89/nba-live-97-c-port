#ifndef NBA97_GAME_PLAYER_ATTRIBUTES_H
#define NBA97_GAME_PLAYER_ATTRIBUTES_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97GameAttributePlayer {
    uint8_t byte09,byte20,byte1e,byte1b,byte14,byte1c,byte15;
} Nba97GameAttributePlayer;
typedef struct Nba97GameAttributeEntity {
    uint32_t word00;
    uint16_t player_reference;
    uint8_t word00_known,player_known;
} Nba97GameAttributeEntity;
typedef struct Nba97GamePlayerAttributesInput {
    const Nba97GameAttributePlayer* players;
    size_t player_count;
    Nba97GameAttributeEntity entity[11]; /* Physical F4-stride pool, including ball. */
    uint32_t divisor64; /* Actual signed FDB64 word, not a forced positive rate. */
    uint16_t flag21498;
    uint8_t first_entity,first_known,divisor_known,flag_known;
} Nba97GamePlayerAttributesInput;
enum Nba97GameAttributeField { NBA97_ATTRIBUTE_3A=0,NBA97_ATTRIBUTE_3C,
    NBA97_ATTRIBUTE_3E,NBA97_ATTRIBUTE_40,NBA97_ATTRIBUTE_42,NBA97_ATTRIBUTE_44 };
typedef struct Nba97GameAttributeEntityEffect { uint16_t field[6];uint8_t written; } Nba97GameAttributeEntityEffect;
enum Nba97GameAttributeTail { NBA97_ATTRIBUTE_4D9EC=0,NBA97_ATTRIBUTE_35A44,NBA97_ATTRIBUTE_38A18 };
typedef struct Nba97GamePlayerAttributesEffects {
    Nba97GameAttributeEntityEffect entity[11];
    uint32_t height165f48[11];
    uint16_t height_written;
    uint8_t visited_entities,stopped_entity,tail_count,tail[3];
} Nba97GamePlayerAttributesEffects;
enum Nba97GamePlayerAttributesResult {
    NBA97_ATTRIBUTES_COMPLETE=1,NBA97_ATTRIBUTES_TAILS_REQUIRED=2,
    NBA97_ATTRIBUTES_RATING_DIVIDE_TRAP=3,NBA97_ATTRIBUTES_RATE_DIVIDE_TRAP=4,
    NBA97_ATTRIBUTES_ARGUMENT=0,NBA97_ATTRIBUTES_UNRESOLVED=-1,
    NBA97_ATTRIBUTES_REFERENCE=-2
};
/* Complete63EDC direct owner(127 instructions), including real51ED8(11).
 * Ten consecutive physical entities start at actual20BEC[0], not ten separate
 * table lookups.51ED8 stores player+9*624 using raw word00 shifted left2;
 * high-bit wrap/aliasing is retained. Native height storage is eleven entries.
 * All six attribute fields preserve signed division, arithmetic-right-shift
 * rounding, low16 stores, negative values and actual zero-divisor traps.
 *
 * COMPLETE closes the flag21498==0 route. TAILS_REQUIRED means apply effects,
 * then execute4D9EC,35A44,38A18(FFFFFFFF) in order; those callees are not owned.
 * Source traps publish exact preceding effects with no tails. A reached unknown
 * or unowned reference also publishes its prefix as an explicit native refusal,
 * not original success. Argument errors leave output unchanged. No state is
 * silently repaired. stopped_entity is255 after ten visits; otherwise it names
 * the physical visit where execution stopped (including an unowned index).
 * Only written fields are effects. Previous entity/table values are not needed.
 * Inputs and player bytes may overlap output; publication follows consumed reads.
 * Known flags must be0/1; unknown value payloads must be zero metadata. */
int nba97_game_player_attributes(Nba97GamePlayerAttributesEffects* out,
                                 const Nba97GamePlayerAttributesInput* input);
#ifdef __cplusplus
}
#endif
#endif
