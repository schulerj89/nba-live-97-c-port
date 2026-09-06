#ifndef NBA97_GAME_PERIOD_EXPIRY_H
#define NBA97_GAME_PERIOD_EXPIRY_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GamePeriodExpiryWord;
typedef Nba97GameMatchClocksMachine Nba97GamePeriodExpiryMachine;
typedef Nba97GameMatchClocksAccess Nba97GamePeriodExpiryAccess;

enum Nba97GamePeriodExpiryCallKind {
    NBA97_GAME_PERIOD_EXPIRY_CHILD_800582DC = 1,
    NBA97_GAME_PERIOD_EXPIRY_CALL_KIND_COUNT
};

typedef struct Nba97GamePeriodExpiryEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GamePeriodExpiryEvent;

/* The callback observes JAL's ra and both completed argument/delay writes.
 * It may mutate every GPR, HI/LO, retained memory, and the saved frame. */
typedef int (*Nba97GamePeriodExpiryIo)(void*, const Nba97GameTextMemory*,
    const Nba97GamePeriodExpiryEvent*, Nba97GamePeriodExpiryMachine*);

typedef struct Nba97GamePeriodExpiryContext {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    Nba97GamePeriodExpiryMachine machine;
    Nba97GamePeriodExpiryIo io;
    void* user;
    Nba97GamePeriodExpiryAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GamePeriodExpiryContext;

typedef struct Nba97GamePeriodExpiryProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_PERIOD_EXPIRY_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GamePeriodExpiryWord restored_return_address;
    Nba97GamePeriodExpiryWord restored_s1;
    Nba97GamePeriodExpiryWord restored_s0;
    Nba97GamePeriodExpiryMachine machine;
    uint8_t completed;
} Nba97GamePeriodExpiryProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80067664
 * Range: 0x80067664..0x800677D7 (inclusive)
 * Source size: 372 bytes / 93 instructions
 * Evidence: fresh Ghidra game_80067664.txt; instruction SHA-256 8acbe5ca26dbf2ab85193413b716f84fbb6d6c4c2dfadded84f43aada11f5db4
 *
 * Purpose: Detect the end-of-period ball state, publish period-transition state, and return whether the wrapped period timer expired.
 * Inputs: All 32 live MIPS GPRs, HI/LO, retained stack, clock/owner/actor/ball globals, actor type byte, signed ball height and velocity, period gates, and unsigned timer/delta halfwords.
 * Returns: v0 receives live s1 on ordinary exits (so a child-mutated non-Boolean value survives), while a zero or negative wrapped timer half first forces s1/v0 to one; ra/s1/s0 reload through mutable sp, sp advances by 0x20, and all other child-mutated GPR/HI/LO state remains live.
 * Guest memory: Reads 0x800FDB58 before frame allocation; saves s1/ra/s0; conditionally reads and updates the actor, owner, phase, and ball pointer; reads the ball again for height/velocity gates; optionally stores 1 at 0x800FA038; subtracts 0x800FDB6C from 0x800FDB76 and stores the wrapped half before testing it; then reloads the frame in source order.
 * Calls: 0x800582DC at 0x800676CC with a0 assigned at 0x800676C8 and a1=1 in the JAL delay slot at 0x800676D0.
 * Original quirks: Actor types 14, 15, and 19 skip the child; the actor pointer used by the post-child store is live and callback-mutable; the ball pointer is reloaded; the timer store precedes its signed-low-half decision; zero and negative wrapped timer halves both return one.
 * Native mapping: Guest addresses stay uint32_t values validated against retained regions; full mutable machine callbacks and per-byte knownness preserve aliases, unknown branches, access prefixes, and HI/LO without host-pointer casts.
 */
int nba97_game_period_expiry(
    Nba97GamePeriodExpiryContext*, Nba97GamePeriodExpiryProgress*);

#ifdef __cplusplus
}
#endif
#endif
