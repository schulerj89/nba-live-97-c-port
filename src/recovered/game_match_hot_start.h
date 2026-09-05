#ifndef NBA97_GAME_MATCH_HOT_START_H
#define NBA97_GAME_MATCH_HOT_START_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameMatchHotStartCallKind {
    NBA97_MATCH_HOT_START_CHILD_80051ED8 = 1,
    NBA97_MATCH_HOT_START_CHILD_800A72BC
};

typedef struct Nba97GameMatchHotStartEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based source access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameMatchHotStartEvent;

/* The callback observes all registers after JAL and its delay slot. It may
 * synchronously mutate retained memory and every GPR. Return 1 only after the
 * original child boundary has returned; v0 is supplied through registers. */
typedef int (*Nba97GameMatchHotStartIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameMatchHotStartEvent*,
    Nba97GameMatchInitializeRegisters*);

enum Nba97GameMatchHotStartAccessKind {
    NBA97_MATCH_HOT_START_READ = 1,
    NBA97_MATCH_HOT_START_STORE = 2
};

typedef struct Nba97GameMatchHotStartAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask; /* Low width bits, one per little-endian byte. */
    uint8_t kind;
} Nba97GameMatchHotStartAccess;

/* Optional host instrumentation after each completed guest access. It may
 * mutate retained bytes and live GPRs, making source reload behavior testable.
 * Any return other than 1 refuses after the completed access prefix. */
typedef int (*Nba97GameMatchHotStartObserver)(void*,
    const Nba97GameTextMemory*, const Nba97GameMatchHotStartAccess*,
    Nba97GameMatchInitializeRegisters*);

typedef struct Nba97GameMatchHotStartContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameMatchInitializeRegisters registers;
    Nba97GameMatchHotStartIo io;
    Nba97GameMatchHotStartObserver observer;
    void* user;
    Nba97GameMatchHotStartAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameMatchHotStartContext;

typedef struct Nba97GameMatchHotStartProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t access_events;
    size_t callbacks_completed;
    size_t prefixes_written;
    size_t retry_attempts;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameMatchInitializeWord restored_return_address;
    Nba97GameMatchInitializeRegisters registers;
    uint8_t completed;
} Nba97GameMatchHotStartProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80066F88
 * Range: 0x80066F88..0x800670A7 (inclusive)
 * Source size: 288 bytes / 72 instructions
 * Evidence: fresh Ghidra listing game_80066f88.txt; routine SHA-256 cb3d0a2d3babd3aa2b21eecd88e29bf5252b08189303ea11c287b23c81ff8092
 *
 * Purpose: Build 84 hot-data prefix offsets, load ZHOTS with source retries, and initialize the selected live hot-data entry.
 * Inputs: All live MIPS GPRs; retained stack; pointer tables at 0x8001EC98/0x800170C8; live root at 0x80020BEC; child services.
 * Returns: Final child v0 and all live GPRs, with ra/s1/s0 reloaded through the mutable live sp and sp advanced by 0x20.
 * Guest memory: Writes 84 low-halfword prefixes at 0x800FE920, retries pointer publication at 0x800FE91C and flag writes at 0x800D7AF8, clears 0x8002148C, and performs all table, payload, root, and stack accesses in source order.
 * Calls: 0x80051ED8 at 0x80067034; 0x800A72BC at 0x80067054 one or more times; 0x80051ED8 at 0x80067088.
 * Original quirks: Prefix sums wrap at 32 bits and stores truncate to 16 bits; signed SLT selects the larger zero-extended byte; a null loader result retries forever; callback-mutated s0/s1/sp and saved stack words remain live.
 * Native mapping: 32-bit guest addresses use validated retained regions with little-endian per-byte knownness; unresolved callees are full-GPR typed callbacks and operation_budget exposes exact bounded prefixes.
 */
int nba97_game_match_hot_start(Nba97GameMatchHotStartContext*,
    Nba97GameMatchHotStartProgress*);

#ifdef __cplusplus
}
#endif
#endif
