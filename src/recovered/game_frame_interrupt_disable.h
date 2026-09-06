#ifndef NBA97_GAME_FRAME_INTERRUPT_DISABLE_H
#define NBA97_GAME_FRAME_INTERRUPT_DISABLE_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameFrameInterruptDisableWord;
typedef Nba97GameMatchInitializeRegisters
    Nba97GameFrameInterruptDisableRegisters;

typedef struct Nba97GameFrameInterruptDisableMachine {
    Nba97GameFrameInterruptDisableRegisters registers;
    Nba97GameFrameInterruptDisableWord hi;
    Nba97GameFrameInterruptDisableWord lo;
    Nba97GameFrameInterruptDisableWord cp0_status;
} Nba97GameFrameInterruptDisableMachine;

enum Nba97GameFrameInterruptDisableJournalKind {
    NBA97_GAME_FRAME_INTERRUPT_DISABLE_CP0_READ = 1,
    NBA97_GAME_FRAME_INTERRUPT_DISABLE_CP0_WRITE = 2
};

typedef struct Nba97GameFrameInterruptDisableJournal {
    uint32_t pc;
    uint32_t value;
    size_t operation;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameFrameInterruptDisableJournal;

typedef struct Nba97GameFrameInterruptDisableContext {
    size_t operation_budget; /* Attempted MFC0/MTC0 operations. */
    Nba97GameFrameInterruptDisableMachine machine;
    Nba97GameFrameInterruptDisableJournal* journal;
    size_t journal_capacity;
} Nba97GameFrameInterruptDisableContext;

typedef struct Nba97GameFrameInterruptDisableProgress {
    size_t operations;
    size_t cp0_reads;
    size_t cp0_writes;
    size_t journal_events;
    uint32_t stopped_pc;
    Nba97GameFrameInterruptDisableWord old_status;
    Nba97GameFrameInterruptDisableWord new_status;
    Nba97GameFrameInterruptDisableMachine machine;
    uint8_t completed;
} Nba97GameFrameInterruptDisableProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80048FF4
 * Range: 0x80048FF4..0x8004900B (inclusive)
 * Source size: 24 bytes / 6 instructions
 * Evidence: fresh Ghidra game_80048ff4.txt; instruction-byte SHA-256 e2b5189bb64b723db34b4a799b587de972cad235d22f3004a0b1d50abea35fb8
 *
 * Purpose: Capture CP0 Status and disable its interrupt-enable bit for a frame critical section.
 * Inputs: All 32 live MIPS GPRs, HI/LO, explicit CP0 Status, and ra consumed by JR.
 * Returns: v0 is the old CP0 Status, v1 is old Status with bit 0 cleared, CP0 Status receives v1, and every other GPR plus HI/LO/SP is unchanged.
 * Guest memory: None observed.
 * Calls: None observed.
 * Original quirks: Only Status bit 0 is cleared; an unknown ra stops at JR after the CP0 write and its NOP delay remains part of the completed prefix.
 * Native mapping: CP0 Status is explicit per-byte-known machine state; this owner does not control host OS interrupts, access host pointers, or fabricate hardware state.
 */
int nba97_game_frame_interrupt_disable(
    Nba97GameFrameInterruptDisableContext*,
    Nba97GameFrameInterruptDisableProgress*);

#ifdef __cplusplus
}
#endif
#endif
