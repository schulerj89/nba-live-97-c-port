#ifndef NBA97_GAME_ROSTER_BINDINGS_H
#define NBA97_GAME_ROSTER_BINDINGS_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameRosterBindingsAccessKind {
    NBA97_GAME_ROSTER_BINDINGS_READ = 1,
    NBA97_GAME_ROSTER_BINDINGS_STORE = 2
};

typedef struct Nba97GameRosterBindingsAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation; /* One-based source access order. */
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameRosterBindingsAccess;

/* Host-only instrumentation invoked after each completed guest access. It may
 * synchronously mutate retained memory and every live GPR. Return 1 to let the
 * source routine continue; any other value refuses at the completed prefix. */
typedef int (*Nba97GameRosterBindingsObserver)(void*,
    const Nba97GameTextMemory*, const Nba97GameRosterBindingsAccess*,
    Nba97GameMatchInitializeRegisters*);

typedef struct Nba97GameRosterBindingsContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped guest accesses. */
    Nba97GameMatchInitializeRegisters registers;
    Nba97GameRosterBindingsObserver observer;
    void* user;
    Nba97GameRosterBindingsAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameRosterBindingsContext;

typedef struct Nba97GameRosterBindingsProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t access_events;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    Nba97GameMatchInitializeRegisters registers;
    uint8_t completed;
} Nba97GameRosterBindingsProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80063D58
 * Range: 0x80063D58..0x80063EDB (inclusive)
 * Source size: 388 bytes / 97 instructions
 * Evidence: fresh Ghidra listing game_80063d58.txt; routine SHA-256 ae04f6f8be23ad8b85b73281f9f7df1bc375693040a95f677f46fd20479bc3c5
 *
 * Purpose: Publish selected-team roster roots, reverse record pointers, mirrored twelve-player bindings, and lineup indices.
 * Inputs: All live MIPS GPRs; selected-team words at 0x80021D74/0x80021D78; live unsigned counts at 0x80023AEC+team*0x68; retained destination mappings.
 * Returns: All final live GPRs after the JR delay slot; no source-defined v0 value beyond the final loop comparison result.
 * Guest memory: Reads selected teams and per-iteration counts; writes 0x8001EDF4/0x8001EDF8/0x8001EEB8/0x8001EEBC, 0x80020B0C..0x80020B88, 0x80015030, mirrored pointer tables, and lineup halfwords in exact source order.
 * Calls: None observed.
 * Original quirks: Both team indices and both count bytes reload each iteration; indices and 32-bit address arithmetic are unchecked/wrapping; out-of-count slots alias record zero; branch/jump delay-slot stores and pointer increments remain effective.
 * Native mapping: 32-bit guest addresses use validated retained regions with little-endian accesses and byte knownness; optional observation exposes access-order mutation without host-pointer casts.
 */
int nba97_game_roster_bindings(Nba97GameRosterBindingsContext*,
    Nba97GameRosterBindingsProgress*);

#ifdef __cplusplus
}
#endif
#endif
