#ifndef NBA97_GAME_FRAME_INTERRUPT_RESTORE_H
#define NBA97_GAME_FRAME_INTERRUPT_RESTORE_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameFrameInterruptRestoreWord;
typedef Nba97GameMatchInitializeRegisters
    Nba97GameFrameInterruptRestoreRegisters;

typedef struct Nba97GameFrameInterruptRestoreMachine {
    Nba97GameFrameInterruptRestoreRegisters registers;
    Nba97GameFrameInterruptRestoreWord hi;
    Nba97GameFrameInterruptRestoreWord lo;
    Nba97GameFrameInterruptRestoreWord cp0_status;
} Nba97GameFrameInterruptRestoreMachine;

enum Nba97GameFrameInterruptRestoreJournalKind {
    NBA97_GAME_FRAME_INTERRUPT_RESTORE_CP0_WRITE = 1
};

typedef struct Nba97GameFrameInterruptRestoreJournal {
    uint32_t pc;
    uint32_t value;
    size_t operation;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameFrameInterruptRestoreJournal;

typedef struct Nba97GameFrameInterruptRestoreContext {
    size_t operation_budget; /* Attempted MTC0 operations. */
    Nba97GameFrameInterruptRestoreMachine machine;
    Nba97GameFrameInterruptRestoreJournal* journal;
    size_t journal_capacity;
} Nba97GameFrameInterruptRestoreContext;

typedef struct Nba97GameFrameInterruptRestoreProgress {
    size_t operations;
    size_t cp0_writes;
    size_t journal_events;
    uint32_t stopped_pc;
    Nba97GameFrameInterruptRestoreWord published_status;
    Nba97GameFrameInterruptRestoreMachine machine;
    uint8_t completed;
} Nba97GameFrameInterruptRestoreProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8004900C
 * Range: 0x8004900C..0x80049017 (inclusive)
 * Source size: 12 bytes / 3 instructions
 * Evidence: fresh Ghidra game_8004900c.txt; instruction-byte SHA-256 2a0aba4dfdd11aabbe78d6d357c5248773d17b360739d0f3c8567bded0096ee3
 *
 * Purpose: Restore a captured frame-critical-section value into CP0 Status.
 * Inputs: All 32 live MIPS GPRs, HI/LO, explicit prior CP0 Status, full a0 value and knownness, and ra consumed by JR.
 * Returns: CP0 Status receives a0 exactly; all 32 GPRs including a0 and v0, plus HI/LO/SP, remain unchanged.
 * Guest memory: None observed.
 * Calls: None observed.
 * Original quirks: Prior CP0 Status is never read or returned; partial a0 knownness is published unchanged, and an unknown ra stops only after MTC0 and the JR NOP delay.
 * Native mapping: CP0 Status is explicit per-byte-known machine state; this owner does not control host OS interrupts, access host pointers, or sanitize the source bit pattern.
 */
int nba97_game_frame_interrupt_restore(
    Nba97GameFrameInterruptRestoreContext*,
    Nba97GameFrameInterruptRestoreProgress*);

#ifdef __cplusplus
}
#endif
#endif
