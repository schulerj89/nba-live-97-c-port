#ifndef NBA97_GAME_BALL_CONTACT_GATE_H
#define NBA97_GAME_BALL_CONTACT_GATE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameBallContactGateWord;
typedef Nba97GameMatchClocksMachine Nba97GameBallContactGateMachine;
typedef Nba97GameMatchClocksAccess Nba97GameBallContactGateAccess;

enum Nba97GameBallContactGateCallKind {
    NBA97_GAME_BALL_CONTACT_GATE_CHILD_800602CC = 1,
    NBA97_GAME_BALL_CONTACT_GATE_CALL_KIND_COUNT
};

typedef struct Nba97GameBallContactGateEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameBallContactGateEvent;

/* The callback observes JAL's ra after its NOP delay and the full live
 * machine, including pre-swap a2. It may mutate every GPR, HI/LO, retained
 * memory, sp, and saved stack words. Return 1 only after the child returns. */
typedef int (*Nba97GameBallContactGateIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameBallContactGateEvent*,
    Nba97GameBallContactGateMachine*);

typedef struct Nba97GameBallContactGateContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameBallContactGateMachine machine;
    Nba97GameBallContactGateIo io;
    void* user;
    Nba97GameBallContactGateAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameBallContactGateContext;

typedef struct Nba97GameBallContactGateProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_BALL_CONTACT_GATE_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameBallContactGateWord saved_return_address;
    Nba97GameBallContactGateWord second_coordinate;
    Nba97GameBallContactGateWord first_coordinate;
    Nba97GameBallContactGateWord coordinate_difference;
    Nba97GameBallContactGateWord shifted_difference;
    Nba97GameBallContactGateWord coordinate_gate;
    Nba97GameBallContactGateWord second_identifier;
    Nba97GameBallContactGateWord restored_return_address;
    Nba97GameBallContactGateWord returned_value;
    Nba97GameBallContactGateMachine machine;
    uint8_t completed;
} Nba97GameBallContactGateProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80060E8C
 * Range: 0x80060E8C..0x80060EF7 (inclusive)
 * Source size: 108 bytes / 27 instructions
 * Evidence: fresh Ghidra game_80060e8c.txt; instruction SHA-256 ae854630bdf6eb03fe7e71319dc8a55c0bf174f1ce958b2fd20cd0480cb884e9
 *
 * Purpose: Gate ball/actor contact dispatch by signed coordinate distance and normalize argument order when the second object has identifier 10.
 * Inputs: All 32 live MIPS GPRs, HI/LO, a0 first-object address, a1 second-object address, their words at offsets 0 and 8, retained stack, and the typed 0x800602CC child.
 * Returns: v0 is zero outside shifted distance [-32,32] and one after any completed child dispatch; a2 retains the pre-swap shifted difference, child machine changes remain live except v0/ra/sp epilogue effects.
 * Guest memory: Saves ra at entry sp-8; reads second+8 then first+8, conditionally reads second+0, and reloads ra through child-mutable live sp.
 * Calls: 0x800602CC at 0x80060ED4 with a0/a1 normalized by identifier and a2 computed before swapping; NOP delay slot.
 * Original quirks: The wrapper overwrites every completed child return with one; v0=10 and a0=original first execute in branch delay slots even when their predicates are unknown.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory addresses; per-byte knownness, wrapping arithmetic, and full mutable machine callbacks preserve exact prefixes without host-pointer casts.
 */
int nba97_game_ball_contact_gate(Nba97GameBallContactGateContext*,
    Nba97GameBallContactGateProgress*);

#ifdef __cplusplus
}
#endif
#endif
