#ifndef NBA97_GAME_RANDOM_SEED_H
#define NBA97_GAME_RANDOM_SEED_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameRandomSeedWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameRandomSeedRegisters;

enum Nba97GameRandomSeedAccessKind {
    NBA97_GAME_RANDOM_SEED_STORE = 1
};

typedef struct Nba97GameRandomSeedAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation; /* One-based source store-attempt order. */
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameRandomSeedAccess;

typedef struct Nba97GameRandomSeedContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped guest stores. */
    Nba97GameRandomSeedRegisters registers;
    Nba97GameRandomSeedAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameRandomSeedContext;

typedef struct Nba97GameRandomSeedProgress {
    size_t operations;
    size_t accesses;
    size_t stores;
    size_t access_events;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    Nba97GameRandomSeedRegisters registers;
    uint8_t completed;
} Nba97GameRandomSeedProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80093694
 * Range: 0x80093694..0x80093733 (inclusive)
 * Source size: 160 bytes / 40 instructions
 * Evidence: fresh Ghidra game_80093694.txt; routine SHA-256 cd1d036806a765a225bd599d954d5dda486165a0e9cc194a758658eb3920e725
 *
 * Purpose: Expand the live a0 seed into the six retained words consumed by the GAMEONLY random-step routine.
 * Inputs: All 32 live MIPS GPRs, with a0 as the cumulative seed and ra as the leaf return target; retained mappings for 0x800C4AE8..0x800C4AFF.
 * Returns: a0 after six cumulative additions, a1=0x800C4AE8, final v0/at source operands, and every untouched GPR unchanged.
 * Guest memory: Stores six words in order at 0x800C4AE8,+4,+8,+0xC,+0x10,+0x14; each word retains per-byte knownness from live a0.
 * Calls: None observed.
 * Original quirks: The first fixed add is signed ADD but cannot overflow for its fixed operands; all six additions into a0 wrap, and an unknown ra is consumed only after every store and the JR delay NOP.
 * Native mapping: Guest addresses remain validated uint32_t values with little-endian stores; no native RNG or adjacent 0x800935C4 algorithm is substituted.
 */
int nba97_game_random_seed(Nba97GameRandomSeedContext*,
    Nba97GameRandomSeedProgress*);

#ifdef __cplusplus
}
#endif
#endif
