#ifndef NBA97_GAME_CONTROLLER_SELECTION_H
#define NBA97_GAME_CONTROLLER_SELECTION_H
#include "game_controllers.h"
#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_SELECTION_ENTITY_COUNT=11, NBA97_SELECTION_UNKNOWN_REF=255 };
enum { NBA97_SELECTION_OK=1, NBA97_SELECTION_INVALID=0,
       NBA97_SELECTION_UNRESOLVED=-1, NBA97_SELECTION_OUTSIDE_STORAGE=-2 };

typedef struct Nba97GameSelectionWord {
    uint32_t word;
    uint8_t known; /* UNKNOWN requires zero payload; KNOWN accepts all32 bits. */
} Nba97GameSelectionWord;
typedef struct Nba97GameSelectionController {
    int16_t team_base;
    Nba97GameControllerSelection selected;
} Nba97GameSelectionController;
typedef struct Nba97GameSelectionEntity {
    int16_t claim;
    int32_t x,y; /* Source entity+8/+C, before signed wrapped difference >>8. */
} Nba97GameSelectionEntity;
typedef struct Nba97GameSelectionInput {
    Nba97GameSelectionController controller[8];
    Nba97GameSelectionEntity entity[11]; /* Contiguous source records, strideF4. */
    uint8_t controller_table[8]; /* FDC50 entries resolve controller record0..7. */
    uint8_t entity_table[11]; /* 20BEC entries resolve entity record0..10. */
    uint8_t ball; /* FDC48 reference; UNKNOWN_REF permitted until accessed. */
    int16_t tail_entity; /* Signed FE8CA lookup index, not a physical entity ID. */
    int16_t tail_state; /* Signed FE8CC. */
    Nba97GameSelectionWord incoming_s6;
} Nba97GameSelectionInput;

typedef struct Nba97GameSelectionWrite {
    uint32_t raw_s6;
    uint16_t selected_word;
    uint8_t logical_controller,controller_record,entity_record;
} Nba97GameSelectionWrite;
typedef struct Nba97GameSelectionEffects {
    Nba97GameControllerSelection selected[8];
    int16_t claim[11];
    uint8_t selected_written[8],claim_written[11];
    Nba97GameSelectionWrite writes[8]; /* Source order: selected, then claim. */
    uint8_t write_count;
    int16_t tail_state;
    uint8_t tail_state_written,call_7a36c;
    /* Diagnostic search scratch only. Original restores caller s6 at return;
     * never write this token back as a persistent CPU/controller register. */
    Nba97GameSelectionWord scratch_s6;
} Nba97GameSelectionEffects;

/* Complete30-instruction GAME7066C distance helper. All32-bit operands and
 * results preserve source wrapping, signed comparisons and arithmetic shifts,
 * including INT_MIN negation/overflow quirks. No sqrt or float substitution. */
int32_t nba97_game_selection_distance(int32_t x,int32_t y);

/* GAME653E8, complete114-instruction owner with7066C in native C. Input is a
 * captured state; output owns only selected words/claims and the FE8CC effect.
 * All8 controller-table entries may alias; entity-table entries may alias.
 * Searches walk five physically contiguous entities starting at table0 or5,
 * but selected writes dereference table[s6] independently. Do not flatten this
 * distinction. Nonnegative team_base0 selects table0; every positive base uses5.
 * References are record indices or UNKNOWN_REF. Unknown references/selections
 * validate as representations; a needed unknown reference or unknown stale s6
 * returns UNRESOLVED. A required index outside owned records returns
 * OUTSIDE_STORAGE. These native boundaries never invent a source fallback.
 * Invalid pointer/token/reference representation returns INVALID. All failures
 * leave output unchanged, even after a late unresolved access. Input/output
 * bytes may overlap; input is captured before publication.
 * Apply ordered writes, then call7A36C when requested, then write tail_state.
 * The callback's internal state effects are a separate owner, not executed
 * by this core. No callback is requested on a failed/unpublished transaction.
 * IMPORTANT original bugs are retained: a pre-existing claim leaves a stale
 * selected word; an empty candidate search uses incoming/last accepted s6,
 * potentially claiming another side's entity or overwriting an existing claim. */
int nba97_game_controller_selection(Nba97GameSelectionEffects* out,
                                    const Nba97GameSelectionInput* input);

#ifdef __cplusplus
}
#endif
#endif
