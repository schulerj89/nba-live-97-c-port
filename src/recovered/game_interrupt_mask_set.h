#ifndef NBA97_GAME_INTERRUPT_MASK_SET_H
#define NBA97_GAME_INTERRUPT_MASK_SET_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameInterruptMaskSetContext {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    uint32_t interrupt_mask;
} Nba97GameInterruptMaskSetContext;

typedef struct Nba97GameInterruptMaskSetProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t requested_mask;
    uint32_t previous_mask;
    uint32_t published_mask;
    uint32_t return_v0;
    uint8_t completed;
} Nba97GameInterruptMaskSetProgress;

/* Original GAMEONLY subroutine 0x800985B4..0x800985CB (6 instructions),
 * called at 0x80029A08 with a0=0 immediately before ResetCallback at
 * 0x800985DC. The linked PsyQ INTR module identifies this entry as
 * SetIntrMask: 0x800985B8 loads the previous mask from 0x800C54AC,
 * 0x800985C0 stores a0 there, and v0 returns the previous value.
 *
 * This compatibility owner changes mapped PS1 interrupt/callback state only.
 * It does not mask native OS interrupts, pause native input, or touch the
 * renderer. Returns NBA97_TEXT_*; return_v0 records the prior PS1 mask. */
int nba97_game_interrupt_mask_set(Nba97GameInterruptMaskSetContext*,
    Nba97GameInterruptMaskSetProgress*);

#ifdef __cplusplus
}
#endif
#endif
