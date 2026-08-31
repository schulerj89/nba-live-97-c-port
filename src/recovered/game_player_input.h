#ifndef NBA97_GAME_PLAYER_INPUT_H
#define NBA97_GAME_PLAYER_INPUT_H
#include "game_period.h"
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameInputEntityField {
    NBA97_INPUT_00=0,NBA97_INPUT_04,NBA97_INPUT_10,NBA97_INPUT_14,NBA97_INPUT_16,
    NBA97_INPUT_18,NBA97_INPUT_1A,NBA97_INPUT_60,NBA97_INPUT_64,NBA97_INPUT_A4,
    NBA97_INPUT_BA,NBA97_INPUT_BC,NBA97_INPUT_BE,NBA97_INPUT_C0,NBA97_INPUT_C4,
    NBA97_INPUT_D8,NBA97_INPUT_D9,NBA97_INPUT_E4,NBA97_INPUT_ENTITY_COUNT
};
enum Nba97GameInputGlobal {
    NBA97_INPUT_FDB9C=0,NBA97_INPUT_FE918,NBA97_INPUT_FDBCC,NBA97_INPUT_FDB94,
    NBA97_INPUT_FE8E2,NBA97_INPUT_FE8CC,NBA97_INPUT_FDB90,NBA97_INPUT_FDB7C,
    NBA97_INPUT_FE880,NBA97_INPUT_FDBD4,NBA97_INPUT_FDBD2,NBA97_INPUT_D8EEC,
    NBA97_INPUT_FC99C,NBA97_INPUT_FA378,NBA97_INPUT_GLOBAL_COUNT
};
enum Nba97GameInputControllerField {
    NBA97_INPUT_CONTROL_26=0,NBA97_INPUT_CONTROL_2A,NBA97_INPUT_CONTROL_2E,
    NBA97_INPUT_CONTROL_30,NBA97_INPUT_CONTROL_32,NBA97_INPUT_CONTROL_34,
    NBA97_INPUT_CONTROL_38,NBA97_INPUT_CONTROL_3C,NBA97_INPUT_CONTROL_COUNT
};
typedef struct Nba97GamePlayerInputState {
    Nba97GamePeriodValue entity[11][NBA97_INPUT_ENTITY_COUNT];
    Nba97GamePeriodValue global[NBA97_INPUT_GLOBAL_COUNT];
    Nba97GamePeriodValue controller[8][NBA97_INPUT_CONTROL_COUNT];
    Nba97GamePeriodValue player_reference[11],player1d[24];
    Nba97GamePeriodReference entity_table[11],reference_fdc34,team_fdc40;
    uint8_t player_count;
} Nba97GamePlayerInputState;

enum Nba97GameInputOwner {
    NBA97_INPUT_CALL_6CD50=0,NBA97_INPUT_CALL_612E4,NBA97_INPUT_CALL_5BDD8,
    NBA97_INPUT_CALL_610FC,NBA97_INPUT_CALL_5B258,NBA97_INPUT_CALL_5C008,
    NBA97_INPUT_CALL_5699C,NBA97_INPUT_CALL_5ADB8,NBA97_INPUT_CALL_6A144,
    NBA97_INPUT_CALL_6A2E4,NBA97_INPUT_CALL_56B78,NBA97_INPUT_CALL_56CE0,
    NBA97_INPUT_CALL_7A498
};
typedef struct Nba97GameInputCall {
    uint32_t callsite,argument[2];
    /* entity is actual captured a0 entity. 5B258 argument0 is another owned
     * entity index (the original a1 pointer), never fabricated pointer bits.
     * 7A498 instead has both raw scalar a0/a1 in argument[], controller set.
     * All other argument[] entries are the declared scalar args after a0. */
    uint8_t owner,entity,controller,argument_count,argument_known;
    /* argument_known bit i: argument[i] is known. Internal7A498 neutral8 may
     * carry unknown a1 without reading it; its zero payload is only metadata. */
} Nba97GameInputCall;
typedef struct Nba97GameInputEvent {
    Nba97GamePeriodValue value,return_v0;
    Nba97GameInputCall call;
    uint8_t kind,record,field,completed; /* kind0entity,1global,2controller,3call. */
} Nba97GameInputEvent;
typedef struct Nba97GameInputReceipt {
    Nba97GameInputEvent event[24];
    Nba97GamePeriodReference captured_team,controller_argument;
    Nba97GamePeriodValue edge_mask;
    uint32_t stopped_pc,logical_mask,mapped_mask;
    uint8_t count,completed,play_call_pending;
} Nba97GameInputReceipt;
typedef int (*Nba97GameInputCallback)(void*,Nba97GamePlayerInputState*,
                                    const Nba97GameInputCall*,Nba97GamePeriodValue* return_v0);
enum Nba97GameInputResult {
    NBA97_INPUT_OK=1,NBA97_INPUT_ARGUMENT=0,NBA97_INPUT_UNRESOLVED=-1,
    NBA97_INPUT_REFERENCE=-2,NBA97_INPUT_PENDING=-3,NBA97_INPUT_CALLBACK_FAILED=-4,
    NBA97_INPUT_PLAY_CALL_PENDING=-5
};
unsigned nba97_game_input_entity_offset(unsigned);
unsigned nba97_game_input_entity_width(unsigned);
unsigned nba97_game_input_global_address(unsigned);
unsigned nba97_game_input_global_width(unsigned);
unsigned nba97_game_input_controller_offset(unsigned);
unsigned nba97_game_input_controller_width(unsigned);

/* GAME61760 initial FDB9C writes and COMPLETE ordinary branch. Raw a2/a3 are
 * logical_mask/mapped_mask from actual700E4/2D2DC, never raw button constants.
 * The play-call continuation stops at617D0 before its first dereference, with
 * capturedS1 team/current controller/masks retained; it cannot be acknowledged
 * as complete by a callback. Ordinary callees run synchronously and update the
 * view before return1. Null/0 means pending, negative failed. Consumed original
 * v0 must be known; accepted0 from6A2E4 continues FALLBACK and keeps its RNG and
 * other effects in the caller context. Callbacks must retain effects beyond
 * this narrow view. Complete does not mean that a motion/queue request changed
 * animation: the original locks and full queues still apply.
 * Mutable candidate/prefix receipt, NOT resumable. Clone complete owned state
 * and context for atomic publication. No resource data or success stubs here.
 * Needed unowned signed table indices refuse, never clamp to a player slot.
 * Canonical unknown values may be copied; arithmetic/branches need known data.
 * State, receipt, and context-owned immutable resources must not overlap. */
int nba97_game_player_input(Nba97GamePlayerInputState*,unsigned entity,
                           Nba97GamePeriodReference controller_argument,
                           uint32_t logical_mask,uint32_t mapped_mask,
                           Nba97GameInputCallback,void*,Nba97GameInputReceipt*);
/* Complete700E4 direct owner. Uses captured signed previous controller30,
 * writes original mask/direction prefix, invokes actual7A498, writes2A twice,
 * then re-reads selected26/table for held400 entityE4=10. Returned edge_mask is
 * FULL32 bits, unlike the low16 controller34 store. No physical mapping.
 * Actual55-instruction7A498 is internal and never a success-only callback. */
int nba97_game_input_edge(Nba97GamePlayerInputState*,unsigned controller,uint32_t mapped_mask,
                         Nba97GameInputReceipt*);
/* Actual7A498, including rawlow16==8 early return and low8a1 test. The three
 * direction/camera globals must retain original provenance. Output unchanged
 * on failure; caller may use this helper independently with full32 inputs. */
int nba97_game_input_direction(Nba97GamePeriodValue* out,const Nba97GamePlayerInputState*,
                              uint32_t direction,uint32_t mode);
#ifdef __cplusplus
}
#endif
#endif
