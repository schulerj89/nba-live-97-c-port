#ifndef NBA97_GAME_CLOCK_VIOLATIONS_H
#define NBA97_GAME_CLOCK_VIOLATIONS_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameClockViolationsWord;
typedef Nba97GameMatchClocksMachine Nba97GameClockViolationsMachine;

enum Nba97GameClockViolationsCallKind {
    NBA97_GAME_CLOCK_VIOLATIONS_CHILD_80029590 = 1,
    NBA97_GAME_CLOCK_VIOLATIONS_CHILD_800295C8,
    NBA97_GAME_CLOCK_VIOLATIONS_CHILD_80062300,
    NBA97_GAME_CLOCK_VIOLATIONS_CHILD_80062660,
    NBA97_GAME_CLOCK_VIOLATIONS_CALL_KIND_COUNT
};

typedef struct Nba97GameClockViolationsEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameClockViolationsEvent;

/* The callback observes JAL's ra and the completed delay slot. It may mutate
 * every GPR, HI/LO, retained memory, and the saved frame. Return exactly 1
 * only after the original child boundary returns synchronously. */
typedef int (*Nba97GameClockViolationsIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameClockViolationsEvent*,
    Nba97GameClockViolationsMachine*);

enum Nba97GameClockViolationsAccessKind {
    NBA97_GAME_CLOCK_VIOLATIONS_READ = 1,
    NBA97_GAME_CLOCK_VIOLATIONS_STORE = 2
};

typedef struct Nba97GameClockViolationsAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameClockViolationsAccess;

typedef struct Nba97GameClockViolationsContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameClockViolationsMachine machine;
    Nba97GameClockViolationsIo io;
    void* user;
    Nba97GameClockViolationsAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameClockViolationsContext;

typedef struct Nba97GameClockViolationsProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_CLOCK_VIOLATIONS_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameClockViolationsWord restored_return_address;
    Nba97GameClockViolationsWord restored_s0;
    Nba97GameClockViolationsMachine machine;
    uint8_t first_violation_triggered;
    uint8_t phase_82_timer_decremented;
    uint8_t phase_82_violation_triggered;
    uint8_t final_timer_decremented;
    uint8_t final_violation_triggered;
    uint8_t completed;
} Nba97GameClockViolationsProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80067D38
 * Range: 0x80067D38..0x8006801B (inclusive)
 * Source size: 740 bytes / 185 instructions
 * Evidence: fresh Ghidra game_80067d38.txt; instruction-byte SHA-256 6f91de08516306fa3d3a827c48ed8460185386a6909d3f627b89f0378fde50ae
 *
 * Purpose: Decrement two violation timers and dispatch shot, phase-82, and final clock-violation effect sequences.
 * Inputs: All 32 live MIPS GPRs, HI/LO, signed delta in a0, retained stack, clocks and phase at 0x800FDB58/0x800FDB90/0x800FDBA4/0x800FDBA8/0x800FDBAA, ownership/team flags, and live actor/ball pointers.
 * Returns: Live GPR and HI/LO state with ra/s0 reloaded through mutable sp, sp advanced by 0x18, and restored ra consumed by JR.
 * Guest memory: Reads all gates and unchecked pointer targets in source order; stores the saved frame, violation state 0x800FE882, phase 0x800FDB90, and wrapped/cleared timer halfwords with exact delay-slot ordering.
 * Calls: 0x80029590 at 0x80067DD8/0x80067DE8, 0x80067ED4/0x80067EE4, and 0x80067FC0/0x80067FD0; 0x800295C8 at 0x80067DF4/0x80067EF0/0x80067FDC; 0x80062300 at 0x80067DFC/0x80067EF8/0x80067FE4; 0x80062660 at 0x80067E04/0x80067F00/0x80067FEC.
 * Original quirks: Main-clock read precedes frame allocation; the zero-main-clock branch still spills ra in its delay slot; phase is cleared through live a0 before team selection; halfword subtraction wraps and tests the signed low half; timer underflow clears even when its enable flag is zero.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory addresses; per-byte knownness and full mutable machine callbacks preserve unknown and failure prefixes without host-pointer casts or fabricated child results.
 */
int nba97_game_clock_violations(
    Nba97GameClockViolationsContext*, Nba97GameClockViolationsProgress*);

#ifdef __cplusplus
}
#endif
#endif
