#ifndef NBA97_GAME_GTE_INITIALIZE_H
#define NBA97_GAME_GTE_INITIALIZE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameGteControlRegister {
    NBA97_GAME_GTE_OFX = 24,
    NBA97_GAME_GTE_OFY = 25,
    NBA97_GAME_GTE_H = 26,
    NBA97_GAME_GTE_DQA = 27,
    NBA97_GAME_GTE_DQB = 28,
    NBA97_GAME_GTE_ZSF3 = 29,
    NBA97_GAME_GTE_ZSF4 = 30
};

enum Nba97GameGteInitializeTarget {
    NBA97_GAME_GTE_TARGET_NONE = 0,
    NBA97_GAME_GTE_TARGET_COP0_STATUS = 1,
    NBA97_GAME_GTE_TARGET_CONTROL = 2
};

typedef struct Nba97GameGteInitializeValue {
    uint32_t word;
    uint8_t known;
} Nba97GameGteInitializeValue;

typedef struct Nba97GameGteInitializeState {
    Nba97GameGteInitializeValue cop0_status;
    Nba97GameGteInitializeValue control[32];
} Nba97GameGteInitializeState;

typedef struct Nba97GameGteInitializeContext {
    Nba97GameGteInitializeState* state;
    size_t operation_budget; /* One CP0/GTE register read or write. */
} Nba97GameGteInitializeContext;

typedef struct Nba97GameGteInitializeProgress {
    size_t operations;
    size_t reads;
    size_t stores;
    uint32_t stopped_pc;
    uint32_t status_before;
    uint32_t status_after;
    uint32_t return_v0;
    uint32_t control_written_mask;
    uint8_t stopped_target;
    uint8_t stopped_register;
    uint8_t controls_written;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameGteInitializeProgress;

/* Original GAMEONLY GTE initializer 0x80056678..0x800566DF
 * (26 instructions), called at main PC 0x80029A54. It preserves every live
 * CP0 Status bit except setting CU2 (0x40000000), then writes the source's
 * projection controls: ZSF3=0x155, ZSF4=0x100, H=1000, DQA=-4194,
 * DQB=0x01400000, OFX=0 and OFY=0. Its incidental v0 return is the updated
 * Status word.
 *
 * Compatibility deliberately does not clear GTE matrices, FIFOs, FLAG, or
 * any of the other 25 control registers. Repeated calls rewrite only these
 * seven controls. This owner initializes retained native GTE state; it does
 * not execute an opcode interpreter or render a pixel. Returns NBA97_TEXT_*.
 */
int nba97_game_gte_initialize(Nba97GameGteInitializeContext*,
    Nba97GameGteInitializeProgress*);

#ifdef __cplusplus
}
#endif
#endif
