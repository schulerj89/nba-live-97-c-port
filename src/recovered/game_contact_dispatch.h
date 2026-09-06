#ifndef NBA97_GAME_CONTACT_DISPATCH_H
#define NBA97_GAME_CONTACT_DISPATCH_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameContactDispatchWord;
typedef Nba97GameMatchClocksMachine Nba97GameContactDispatchMachine;
typedef Nba97GameMatchClocksAccess Nba97GameContactDispatchAccess;

enum Nba97GameContactDispatchCallKind {
    NBA97_GAME_CONTACT_DISPATCH_CHILD_8005FAA8 = 1,
    NBA97_GAME_CONTACT_DISPATCH_CHILD_80060E8C,
    NBA97_GAME_CONTACT_DISPATCH_CALL_KIND_COUNT
};

typedef struct Nba97GameContactDispatchEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameContactDispatchEvent;

/* The callback observes JAL's ra and completed delay-slot argument move. It
 * may mutate all GPRs, HI/LO, retained memory, and the saved stack frame. */
typedef int (*Nba97GameContactDispatchIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameContactDispatchEvent*, Nba97GameContactDispatchMachine*);

typedef struct Nba97GameContactDispatchContext {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    Nba97GameContactDispatchMachine machine;
    Nba97GameContactDispatchIo io;
    void* user;
    Nba97GameContactDispatchAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameContactDispatchContext;

typedef struct Nba97GameContactDispatchProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_CONTACT_DISPATCH_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameContactDispatchWord restored_return_address;
    Nba97GameContactDispatchWord restored_s2;
    Nba97GameContactDispatchWord restored_s1;
    Nba97GameContactDispatchWord restored_s0;
    Nba97GameContactDispatchMachine machine;
    uint8_t completed;
} Nba97GameContactDispatchProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80060FBC
 * Range: 0x80060FBC..0x800610FB (inclusive)
 * Source size: 320 bytes / 80 instructions
 * Evidence: fresh Ghidra game_80060fbc.txt; instruction SHA-256 c689adf57bd7c0054146f3a97c0f8c9e3ebafd77fafe55c2fa3ee77956b0d254
 *
 * Purpose: Scan the eleven sorted contact references and dispatch actor/ball pair tests until each sorted-distance child reports an early exit.
 * Inputs: All 32 live MIPS GPRs, HI/LO, retained stack, reference words at 0x800FDCBC through 0x800FDCE8, live ball pointer 0x800FDC48, and ball state halfword at offset 0xB4.
 * Returns: Source-live v0/v1 and child-mutated caller-saved GPR/HI/LO state, with ra/s2/s1/s0 reloaded through mutable sp and sp advanced by 0x20.
 * Guest memory: Saves s2/ra/s1/s0, repeatedly reads signed-low-half-indexed reference words and the live ball pointer, reads ball+0xB4 on the two ball branches, and restores the live stack frame in source order; the owner itself performs no guest stores beyond its frame.
 * Calls: 0x8005FAA8 at 0x8006104C with a0=s0 in the delay slot; 0x80060E8C at 0x80061070 with a1=s0 in the delay slot and at 0x800610C4 with a0=s0 in the delay slot.
 * Original quirks: Loop counters increment as full wrapping words but are sign-extended from their low halves for comparisons and table indexing; negative indices can pass the <12 gate; only child v0's low byte controls continuation; every branch/jump delay increment remains live across callback mutation.
 * Native mapping: Guest addresses remain mapped uint32_t values with per-byte knownness; typed callbacks carry the complete mutable machine and preserve aliases, unknown decisions, exact failure prefixes, and HI/LO without host-pointer casts.
 */
int nba97_game_contact_dispatch(
    Nba97GameContactDispatchContext*, Nba97GameContactDispatchProgress*);

#ifdef __cplusplus
}
#endif
#endif
