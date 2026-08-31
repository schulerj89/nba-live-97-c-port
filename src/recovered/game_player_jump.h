#ifndef NBA97_GAME_PLAYER_JUMP_H
#define NBA97_GAME_PLAYER_JUMP_H
#include "game_period.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97GameJumpField {
    NBA97_JUMP_00=0,NBA97_JUMP_08,NBA97_JUMP_0C,NBA97_JUMP_10,
    NBA97_JUMP_14,NBA97_JUMP_16,NBA97_JUMP_18,NBA97_JUMP_1A,
    NBA97_JUMP_48,NBA97_JUMP_4C,NBA97_JUMP_50,NBA97_JUMP_60,NBA97_JUMP_64,
    NBA97_JUMP_9E,NBA97_JUMP_A0,NBA97_JUMP_A4,NBA97_JUMP_A6,NBA97_JUMP_A8,
    NBA97_JUMP_BA,NBA97_JUMP_BC,NBA97_JUMP_BE,NBA97_JUMP_C0,NBA97_JUMP_C4,
    NBA97_JUMP_D9,NBA97_JUMP_FIELD_COUNT
};
enum Nba97GameJumpGlobal {
    NBA97_JUMP_FE8CC=0,NBA97_JUMP_FE8CA,NBA97_JUMP_FDB90,NBA97_JUMP_21D93,
    NBA97_JUMP_FDB94,NBA97_JUMP_FDC1E,NBA97_JUMP_FDC04,NBA97_JUMP_FDC08,
    NBA97_JUMP_FDC0A,NBA97_JUMP_FDC0C,NBA97_JUMP_FDC14,NBA97_JUMP_GLOBAL_COUNT
};
typedef struct Nba97GameJumpPlayer {
    Nba97GamePeriodValue byte09,byte17; /* Actual owned player record, not ratings defaults. */
} Nba97GameJumpPlayer;
typedef struct Nba97GamePlayerJumpState {
    Nba97GamePeriodValue entity[11][NBA97_JUMP_FIELD_COUNT];
    Nba97GamePeriodValue player_reference[11],status_reference[11];
    Nba97GameJumpPlayer player[24];
    Nba97GamePeriodValue status20[24];
    Nba97GamePeriodValue global[NBA97_JUMP_GLOBAL_COUNT],rng1edee;
    Nba97GamePeriodReference ball_fdc48;
    uint8_t player_count,status_count;
} Nba97GamePlayerJumpState;
typedef struct Nba97GamePlayerJumpResources {
    const uint16_t* threshold[2]; /* Actual signed halfwords: B89C4, B89CA. */
    size_t threshold_count[2]; /* Source unchecked index; missing storage refuses. */
    const uint8_t* motion_b86f4; /* Four bytes per unsignedFDC04 row. */
    size_t motion_row_count;
} Nba97GamePlayerJumpResources;
enum Nba97GameJumpOwner {
    NBA97_JUMP_CALL_2AB70=0,NBA97_JUMP_CALL_5A570,NBA97_JUMP_CALL_5699C,
    NBA97_JUMP_CALL_56AA4,NBA97_JUMP_CALL_56B78,NBA97_JUMP_CALL_56CE0
};
typedef struct Nba97GameJumpCall {
    uint32_t callsite,argument[2];
    uint8_t owner,entity,argument_count;
} Nba97GameJumpCall;
typedef struct Nba97GameJumpEvent {
    Nba97GamePeriodValue value;
    Nba97GameJumpCall call;
    uint8_t kind,entity,field,completed; /* 0entity,1global,2RNG store,3call. */
} Nba97GameJumpEvent;
typedef struct Nba97GameJumpReceipt {
    Nba97GameJumpEvent event[24];
    uint8_t count,completed,accepted; /* accepted is originalv0, separate from native success. */
} Nba97GameJumpReceipt;
typedef int (*Nba97GameJumpCallback)(void*,Nba97GamePlayerJumpState*,const Nba97GameJumpCall*);
typedef struct Nba97GameJumpRngEffects {
    Nba97GamePeriodValue state;
    uint16_t value,write[2];
    uint8_t count;
} Nba97GameJumpRngEffects;
enum Nba97GameJumpResult {
    NBA97_JUMP_OK=1,NBA97_JUMP_ARGUMENT=0,NBA97_JUMP_UNRESOLVED=-1,
    NBA97_JUMP_REFERENCE=-2,NBA97_JUMP_PENDING=-3,NBA97_JUMP_CALLBACK_FAILED=-4,
    NBA97_JUMP_SOURCE_DIVZERO=-5
};
unsigned nba97_game_jump_entity_offset(unsigned field);
unsigned nba97_game_jump_entity_width(unsigned field);
unsigned nba97_game_jump_global_address(unsigned field);
unsigned nba97_game_jump_global_width(unsigned field);
/* Complete GAME2AB70, including zero-seed's first store. Atomic standalone
 * effects. This is the SAME shared original1EDEE halfword, never a fresh RNG. */
int nba97_game_jump_rng(Nba97GameJumpRngEffects*,const Nba97GamePeriodValue*);
/* Complete GAME6A2E4 direct owner plus actual2AB70. Explicit caller-selected
 * entity and exact raw a1; this API does not authorize or emulate input gating.
 * Mutable candidate and ordered prefix receipt, including rejectedv0=0 paths.
 * Callbacks execute actual5A570/animation owners synchronously and update this
 * view before returning1. Null/0 means pending, negative means failed. 2AB70
 * is internal and never delegated. Ball/player/status references are owned
 * indices with explicit knownness; aliases are retained. The ball reference
 * is captured before the rating gates, as in the originalS2.
 * No table data, game input, resource loading, fake callback success or pointer
 * bits are embedded. State/resources/receipt storage must not overlap. */
int nba97_game_player_jump(Nba97GamePlayerJumpState*,unsigned entity,uint32_t argument,
                          const Nba97GamePlayerJumpResources*,Nba97GameJumpCallback,
                          void*,Nba97GameJumpReceipt*);
#ifdef __cplusplus
}
#endif
#endif
