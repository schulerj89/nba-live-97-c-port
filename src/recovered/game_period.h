#ifndef NBA97_GAME_PERIOD_H
#define NBA97_GAME_PERIOD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Address suffixes identify original fields, not native pointers or RAM. */
enum Nba97GamePeriodScalar {
    NBA97_PERIOD_FDB68=0, NBA97_PERIOD_21D73, NBA97_PERIOD_1EDEC, NBA97_PERIOD_1EDF2,
    NBA97_PERIOD_FE90C, NBA97_PERIOD_FA038, NBA97_PERIOD_FDB74, NBA97_PERIOD_FDBDE,
    NBA97_PERIOD_FE8BC, NBA97_PERIOD_FE8C4, NBA97_PERIOD_FE8CC, NBA97_PERIOD_FE884,
    NBA97_PERIOD_FDBAE, NBA97_PERIOD_FDBDA, NBA97_PERIOD_FDBD8, NBA97_PERIOD_FE8FE,
    NBA97_PERIOD_FE894, NBA97_PERIOD_FE8FA, NBA97_PERIOD_FDB84, NBA97_PERIOD_FDBBE,
    NBA97_PERIOD_FE90E, NBA97_PERIOD_FE8F2, NBA97_PERIOD_FE89C, NBA97_PERIOD_FE8E2,
    NBA97_PERIOD_FE8E0, NBA97_PERIOD_FE8AC, NBA97_PERIOD_FE8A6, NBA97_PERIOD_FE8A4,
    NBA97_PERIOD_FE86A, NBA97_PERIOD_FE868, NBA97_PERIOD_FE866, NBA97_PERIOD_FDBCA,
    NBA97_PERIOD_FDB8C, NBA97_PERIOD_FDB8A, NBA97_PERIOD_FDB80, NBA97_PERIOD_FDBBC,
    NBA97_PERIOD_FDBBA, NBA97_PERIOD_FDBB4, NBA97_PERIOD_FDBB2, NBA97_PERIOD_FDBB0,
    NBA97_PERIOD_FDB52, NBA97_PERIOD_FDB50, NBA97_PERIOD_FDB7C, NBA97_PERIOD_FA034,
    NBA97_PERIOD_FE898, NBA97_PERIOD_FE87E, NBA97_PERIOD_FE87C, NBA97_PERIOD_FE87A,
    NBA97_PERIOD_FDB98, NBA97_PERIOD_FDBCC, NBA97_PERIOD_FDB9A, NBA97_PERIOD_FE8E4,
    NBA97_PERIOD_FDBA4, NBA97_PERIOD_FE86E, NBA97_PERIOD_FDB60, NBA97_PERIOD_FDB58,
    NBA97_PERIOD_FDB76, NBA97_PERIOD_FDB5C, NBA97_PERIOD_FDB64, NBA97_PERIOD_FDB7E,
    NBA97_PERIOD_FE8AA, NBA97_PERIOD_FDB96, NBA97_PERIOD_FDB94, NBA97_PERIOD_FDBAA,
    NBA97_PERIOD_FDB90, NBA97_PERIOD_FDB72, NBA97_PERIOD_FE880, NBA97_PERIOD_FE882,
    NBA97_PERIOD_SCALAR_COUNT
};
enum Nba97GamePeriodTeamField { NBA97_PERIOD_TEAM_34=0, NBA97_PERIOD_TEAM_35,
                              NBA97_PERIOD_TEAM_10, NBA97_PERIOD_TEAM_FIELD_COUNT };
enum Nba97GamePeriodEntityField {
    NBA97_PERIOD_ENTITY_00=0, NBA97_PERIOD_ENTITY_06, NBA97_PERIOD_ENTITY_08,
    NBA97_PERIOD_ENTITY_0C, NBA97_PERIOD_ENTITY_10, NBA97_PERIOD_ENTITY_14,
    NBA97_PERIOD_ENTITY_16, NBA97_PERIOD_ENTITY_18, NBA97_PERIOD_ENTITY_AC,
    NBA97_PERIOD_ENTITY_B4, NBA97_PERIOD_ENTITY_BA, NBA97_PERIOD_ENTITY_D9,
    NBA97_PERIOD_ENTITY_FIELD_COUNT
};
typedef struct Nba97GamePeriodValue {
    uint32_t word; /* Canonical raw bits at the field's declared width. */
    uint8_t known; /* 0 unknown (word0 metadata),1 known. */
} Nba97GamePeriodValue;
typedef struct Nba97GamePeriodReference {
    uint8_t record,known; /* Unknown record0 is metadata, not a null pointer. */
} Nba97GamePeriodReference;
typedef struct Nba97GamePeriodState {
    Nba97GamePeriodValue scalar[NBA97_PERIOD_SCALAR_COUNT];
    Nba97GamePeriodValue team[2][NBA97_PERIOD_TEAM_FIELD_COUNT];
    Nba97GamePeriodValue entity[11][NBA97_PERIOD_ENTITY_FIELD_COUNT];
    Nba97GamePeriodValue controller22[8];
    Nba97GamePeriodReference controller_table[8]; /* FDC50. */
    Nba97GamePeriodReference entity_table[11]; /*20BEC; entry10 is20C14. */
    Nba97GamePeriodReference render_table[11]; /*FDCC0; entry10 isFDCE8. */
    Nba97GamePeriodReference ball_fdc48,reference_fdc34;
    Nba97GamePeriodValue incoming_s6; /* Explicit caller register provenance for653E8. */
} Nba97GamePeriodState;
typedef struct Nba97GamePeriodDurations {
    uint32_t value[2][256]; /* Actual words atB895C/B8970 + unsigned21D73*4. */
} Nba97GamePeriodDurations;

enum Nba97GamePeriodOwner {
    NBA97_PERIOD_CALL_646A8=0, NBA97_PERIOD_CALL_65140, NBA97_PERIOD_CALL_65B18,
    NBA97_PERIOD_CALL_653E8, NBA97_PERIOD_CALL_60EF8, NBA97_PERIOD_CALL_5828C,
    NBA97_PERIOD_CALL_56B78
};
enum Nba97GamePeriodFormation { NBA97_PERIOD_FORMATION_B891C=0, NBA97_PERIOD_FORMATION_B893C=1 };
typedef struct Nba97GamePeriodCall {
    uint32_t callsite;
    int32_t argument; /*65140 amount /65B18 special-center /56B78 motion. */
    uint8_t owner,side,formation,entity;
    /* Only653E8 depends on incoming s6. Scratch/returned s6 is not persisted. */
    Nba97GamePeriodValue incoming_s6;
} Nba97GamePeriodCall;

enum Nba97GamePeriodTarget {
    NBA97_PERIOD_TARGET_SCALAR=0, NBA97_PERIOD_TARGET_TEAM, NBA97_PERIOD_TARGET_ENTITY,
    NBA97_PERIOD_TARGET_CONTROLLER22, NBA97_PERIOD_TARGET_ENTITY_TABLE,
    NBA97_PERIOD_TARGET_RENDER_TABLE, NBA97_PERIOD_TARGET_BALL_FDC48,
    NBA97_PERIOD_TARGET_REFERENCE_FDC34
};
enum { NBA97_PERIOD_EVENT_WRITE=0, NBA97_PERIOD_EVENT_CALL=1, NBA97_PERIOD_EVENT_CAPACITY=160 };
typedef struct Nba97GamePeriodEvent {
    uint32_t value;
    Nba97GamePeriodCall call;
    uint8_t kind,target,field,record,width,call_completed;
} Nba97GamePeriodEvent;
typedef struct Nba97GamePeriodReceipt {
    Nba97GamePeriodEvent event[160];
    uint16_t count;
    uint8_t completed_calls,complete;
    int16_t captured_quarter;
    uint8_t captured_quarter_known;
} Nba97GamePeriodReceipt;
enum Nba97GamePeriodResult {
    NBA97_PERIOD_COMPLETE=1, NBA97_PERIOD_ARGUMENT=0,
    NBA97_PERIOD_UNRESOLVED=-1, NBA97_PERIOD_REFERENCE=-2,
    NBA97_PERIOD_CALLBACK_PENDING=-3, NBA97_PERIOD_CALLBACK_FAILED=-4
};
/* Return1 only after the requested owner actually completed on this candidate
 * state/context. Return0 if unavailable (no callback effects), negative on an
 * explicit failure. Native coordinator never implements a successful no-op. */
typedef int (*Nba97GamePeriodCallback)(void* context,Nba97GamePeriodState* state,
                                      const Nba97GamePeriodCall* call);

unsigned nba97_game_period_scalar_width(unsigned field);
unsigned nba97_game_period_entity_width(unsigned field);

/* Complete65DB0 coordinator (381 original instructions), with synchronous
 * callback boundaries whose internal effects belong to their own modules.
 * Runs on MUTABLE CANDIDATE state. Scalar writes precede each callback; its
 * completed effects are visible to every subsequent source read. Every direct
 * write and call is recorded in order. Call_completed is set only after callback
 * returns1. Missing callback returnsPENDING at the first actual call, not success.
 * A stopped run preserves its exact prefix in candidate+receipt for diagnosis;
 * it is not a resumable continuation. Caller must clone state AND callback
 * context before invoking, and publish only a complete supported transaction.
 * No rollback of external callback effects is attempted or promised.
 * Null/bad initial representations returnARGUMENT before mutation. Needed
 * unknown source reads returnUNRESOLVED; unowned references returnREFERENCE.
 * All durations256 lookups are real resource words (including adjacent raw-data
 * reads); no five-option table bound, substituted duration, position or register.
 * State, receipt and duration objects must be separate/nonoverlapping. Callback
 * context may own/reference candidate, but must not overlap receipt or immutable
 * durations. Incoming s6 remains caller provenance, not a guess.
 */
int nba97_game_period_initialize(Nba97GamePeriodState* candidate,
                                 const Nba97GamePeriodDurations* durations,
                                 Nba97GamePeriodCallback callback,void* context,
                                 Nba97GamePeriodReceipt* receipt);

#ifdef __cplusplus
}
#endif
#endif
