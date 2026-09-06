#ifndef NBA97_GAME_CLOCK_READ_H
#define NBA97_GAME_CLOCK_READ_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameClockReadWord;
typedef Nba97GameMatchClocksRegisters Nba97GameClockReadRegisters;
typedef Nba97GameMatchClocksMachine Nba97GameClockReadMachine;

enum Nba97GameClockReadAccessKind {
    NBA97_GAME_CLOCK_READ_READ = 1
};

typedef struct Nba97GameClockReadAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameClockReadAccess;

typedef struct Nba97GameClockReadContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped LW operations. */
    Nba97GameClockReadMachine machine;
    Nba97GameClockReadAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameClockReadContext;

typedef struct Nba97GameClockReadProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t access_events;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    Nba97GameClockReadWord return_v0;
    Nba97GameClockReadMachine machine;
    uint8_t completed;
} Nba97GameClockReadProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800A5810
 * Range: 0x800A5810..0x800A581F (inclusive)
 * Source size: 16 bytes / 4 instructions
 * Evidence: fresh Ghidra game_800a5810.txt; instruction SHA-256 e8dc11d1abddf2e952768e4de6abb193e2cbae4d9423b179825acb709b7d9c0d
 *
 * Purpose: Return the current retained GAMEONLY clock counter.
 * Inputs: All 32 live MIPS GPRs, HI/LO, live ra for JR, and the little-endian word at 0x800D7A70.
 * Returns: Raw counter bytes and their knownness in v0; every other GPR and HI/LO is unchanged.
 * Guest memory: Reads exactly four bytes at 0x800D7A70; no writes.
 * Calls: None observed.
 * Original quirks: The leaf does not allocate a frame, inspect sp, increment time, or require v0 to be known before returning; unknown ra stops after the read.
 * Native mapping: The fixed guest uint32_t address is resolved through validated retained-memory regions with per-byte knownness and is never cast to a host pointer.
 */
int nba97_game_clock_read(
    Nba97GameClockReadContext*, Nba97GameClockReadProgress*);

#ifdef __cplusplus
}
#endif
#endif
