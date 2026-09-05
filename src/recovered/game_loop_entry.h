#ifndef NBA97_GAME_LOOP_ENTRY_H
#define NBA97_GAME_LOOP_ENTRY_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameLoopEntryCallKind {
    NBA97_GAME_LOOP_ENTRY_MATCH_TICK = 1
};

typedef struct Nba97GameLoopEntryEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameLoopEntryEvent;

/* The callback observes JAL's known ra and the NOP delay slot. It may mutate
 * shared retained memory and every live GPR. Return 1 only after the complete
 * child returns to 0x8002DC48; a pending/non-resumable child returns 0. */
typedef int (*Nba97GameLoopEntryIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameLoopEntryEvent*, Nba97GameMatchInitializeRegisters*);

enum Nba97GameLoopEntryAccessKind {
    NBA97_GAME_LOOP_ENTRY_READ = 1,
    NBA97_GAME_LOOP_ENTRY_STORE = 2
};

typedef struct Nba97GameLoopEntryAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameLoopEntryAccess;

typedef struct Nba97GameLoopEntryContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameMatchInitializeRegisters registers;
    Nba97GameLoopEntryIo io;
    void* user;
    Nba97GameLoopEntryAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameLoopEntryContext;

typedef struct Nba97GameLoopEntryProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameMatchInitializeWord restored_return_address;
    Nba97GameMatchInitializeRegisters registers;
    uint8_t completed;
} Nba97GameLoopEntryProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8002DC38
 * Range: 0x8002DC38..0x8002DC57 (inclusive)
 * Source size: 32 bytes / 8 instructions
 * Evidence: fresh Ghidra listing game_8002dc38.txt; routine SHA-256 9fd509dbd7e045a5eb066df7b6d95c7a4e7aa60c20f3a6281310a0747e3ca7af
 *
 * Purpose: Enter the recovered match tick through a standard o32 stack frame and return its live state.
 * Inputs: All live MIPS GPRs, retained stack memory, and the mandatory 0x80068BF8 child boundary.
 * Returns: Final child GPRs except ra, which is reloaded through the child-mutable sp; sp then advances by 0x18.
 * Guest memory: Stores incoming ra at entry sp-8, then reloads ra from live sp+0x10 after the child; both are ordered 32-bit little-endian accesses.
 * Calls: 0x80068BF8 at 0x8002DC40 with a NOP delay slot at 0x8002DC44.
 * Original quirks: Child mutation of sp relocates the epilogue load; 32-bit address and sp arithmetic wrap; unknown restored ra cannot drive JR.
 * Native mapping: GPRs retain per-byte knownness and guest addresses use validated retained regions; the child is a typed callback, never a host-pointer cast or successful no-op.
 */
int nba97_game_loop_entry(Nba97GameLoopEntryContext*,
    Nba97GameLoopEntryProgress*);

#ifdef __cplusplus
}
#endif
#endif
