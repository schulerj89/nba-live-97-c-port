#ifndef NBA97_GAME_BALL_ACQUIRE_H
#define NBA97_GAME_BALL_ACQUIRE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameBallAcquireWord;
typedef Nba97GameMatchClocksMachine Nba97GameBallAcquireMachine;

enum Nba97GameBallAcquireCallKind {
    NBA97_GAME_BALL_ACQUIRE_CHILD_8002AB70 = 1,
    NBA97_GAME_BALL_ACQUIRE_CHILD_80072C40,
    NBA97_GAME_BALL_ACQUIRE_CHILD_800295C8,
    NBA97_GAME_BALL_ACQUIRE_CHILD_80029590,
    NBA97_GAME_BALL_ACQUIRE_CHILD_80035318,
    NBA97_GAME_BALL_ACQUIRE_CHILD_8005CE4C,
    NBA97_GAME_BALL_ACQUIRE_CALL_KIND_COUNT
};

typedef struct Nba97GameBallAcquireEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation;
    size_t invocation;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameBallAcquireEvent;

/* The callback observes JAL's link and completed delay slot. It may mutate
 * every GPR, HI/LO, retained memory, and the live saved frame. Return exactly
 * 1 only after the original child has returned synchronously. */
typedef int (*Nba97GameBallAcquireIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameBallAcquireEvent*, Nba97GameBallAcquireMachine*);

enum Nba97GameBallAcquireAccessKind {
    NBA97_GAME_BALL_ACQUIRE_READ = 1,
    NBA97_GAME_BALL_ACQUIRE_STORE = 2
};

typedef struct Nba97GameBallAcquireAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameBallAcquireAccess;

typedef struct Nba97GameBallAcquireContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameBallAcquireMachine machine;
    Nba97GameBallAcquireIo io;
    void* user;
    Nba97GameBallAcquireAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameBallAcquireContext;

typedef struct Nba97GameBallAcquireProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t call_count[NBA97_GAME_BALL_ACQUIRE_CALL_KIND_COUNT];
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameBallAcquireWord restored_return_address;
    Nba97GameBallAcquireWord restored_s3;
    Nba97GameBallAcquireWord restored_s2;
    Nba97GameBallAcquireWord restored_s1;
    Nba97GameBallAcquireWord restored_s0;
    Nba97GameBallAcquireMachine machine;
    uint8_t random_rule_set;
    uint8_t grounded_reset;
    uint8_t possession_changed;
    uint8_t same_team_claim;
    uint8_t completed;
} Nba97GameBallAcquireProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8005D140
 * Range: 0x8005D140..0x8005D9EF (inclusive)
 * Source size: 2224 bytes / 556 instructions
 * Evidence: fresh Ghidra game_8005d140.txt; instruction-byte SHA-256 18065d144bc5c545b3706ddd3178f77b57d4a72f28632ddd4ea978c2324f175e
 *
 * Purpose: Acquire ball possession for an actor, publish its team and coordinates, reset possession state, and update turnover, steal, assist, and controller statistics.
 * Inputs: All 32 live MIPS GPRs, HI/LO, actor address in a0, retained stack, actor/team/controller tables, phase/rule/possession globals, and live pointer graphs rooted at 0x80020BEC, 0x8001EDF4, 0x8001EEB8, and 0x800FDC40.
 * Returns: The signed cached 0x800FDB96 halfword in v0, with ra/s3/s2/s1/s0 reloaded through callback-mutable sp, sp advanced by 0x30, and restored ra consumed by JR.
 * Guest memory: Reads 0x800FA034 before allocating the frame; saves five live registers; performs every actor, team, global, history, velocity, and statistics read/write in source order; reloads the frame through live sp.
 * Calls: In source address order: 0x8002AB70@0x8005D1A4; 0x80072C40@0x8005D464; 0x800295C8@0x8005D498 and 0x8005D4A8; 0x80029590@0x8005D4B4; 0x80035318@0x8005D55C, 0x8005D5E8, 0x8005D68C, 0x8005D778 and 0x8005D7F0; 0x800295C8@0x8005D898 and 0x8005D8A8; 0x80029590@0x8005D8B4; 0x80035318@0x8005D924; 0x8005CE4C@0x8005D970 and 0x8005D9C0.
 * Original quirks: The descriptor byte is stored in the load branch delay slot; signed and unsigned team tests intentionally disagree for 0xFF; byte counters and controller halfwords wrap; individual halfwords cap at 999; odd s2-relative offsets resolve the controller pointer tables; the return value is loaded before cleanup stores and can survive aliases.
 * Native mapping: Guest addresses remain checked uint32_t retained-memory addresses; full-machine callbacks, exact delay slots, per-byte knownness, live aliases, refusal prefixes, and wrapped address arithmetic are preserved without host-pointer casts or fabricated child ABIs.
 */
int nba97_game_ball_acquire(
    Nba97GameBallAcquireContext*, Nba97GameBallAcquireProgress*);

#ifdef __cplusplus
}
#endif
#endif
