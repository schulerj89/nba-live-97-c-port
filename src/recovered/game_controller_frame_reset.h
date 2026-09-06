#ifndef NBA97_GAME_CONTROLLER_FRAME_RESET_H
#define NBA97_GAME_CONTROLLER_FRAME_RESET_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameControllerFrameResetWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameControllerFrameResetRegisters;

enum Nba97GameControllerFrameResetCallKind {
    NBA97_GAME_CONTROLLER_FRAME_RESET_83EEC = 1
};

typedef struct Nba97GameControllerFrameResetEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based mapped-access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameControllerFrameResetEvent;

/* The callback receives the complete GPR file after JAL writes ra and after
 * its NOP delay slot. It may mutate retained memory and every live GPR. */
typedef int (*Nba97GameControllerFrameResetIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameControllerFrameResetEvent*,
    Nba97GameControllerFrameResetRegisters*);

enum Nba97GameControllerFrameResetAccessKind {
    NBA97_GAME_CONTROLLER_FRAME_RESET_READ = 1,
    NBA97_GAME_CONTROLLER_FRAME_RESET_STORE = 2
};

typedef struct Nba97GameControllerFrameResetAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask; /* One bit per little-endian source byte. */
    uint8_t kind;
} Nba97GameControllerFrameResetAccess;

typedef struct Nba97GameControllerFrameResetContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameControllerFrameResetRegisters registers;
    Nba97GameControllerFrameResetIo io;
    void* user;
    Nba97GameControllerFrameResetAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameControllerFrameResetContext;

typedef struct Nba97GameControllerFrameResetProgress {
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
    Nba97GameControllerFrameResetWord initial_timer;
    Nba97GameControllerFrameResetWord delta;
    Nba97GameControllerFrameResetWord adjusted_timer;
    Nba97GameControllerFrameResetWord restored_return_address;
    Nba97GameControllerFrameResetRegisters registers;
    uint8_t controller_slots_cleared;
    uint8_t timer_updated;
    uint8_t timer_clamped;
    uint8_t completed;
} Nba97GameControllerFrameResetProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800675E4
 * Range: 0x800675E4..0x80067663 (inclusive)
 * Source size: 128 bytes / 32 instructions
 * Evidence: fresh Ghidra listing game_800675e4.txt; routine SHA-256 d4efbc7854c91bafdea88b2df235ea00f5140bdb039c9369b464fec2e363262e
 *
 * Purpose: Decrement and clamp the live frame timer, clear eight controller-object halfwords, and dispatch the controller service.
 * Inputs: All 32 live MIPS GPRs; retained stack; signed timer halfword 0x800FE90E; unsigned delta halfword 0x800FDB6C; eight live pointers at 0x800FDC50..0x800FDC6F; and typed child 0x80083EEC.
 * Returns: Final child/live GPRs with ra reloaded through child-mutable sp, sp advanced by 0x20, and the restored ra consumed by JR.
 * Guest memory: Saves ra at entry sp-8; conditionally reads the delta and updates/clamps 0x800FE90E; loads each live pointer then stores zero at pointer+0x28 in eight ordered iterations; reloads ra from live sp+0x18.
 * Calls: 0x80083EEC at 0x8006764C with a NOP delay slot.
 * Original quirks: A zero timer does not read the delta; subtraction wraps before the stored low halfword is sign-tested; every loop increments a0 before its target store and advances v1 in the BNE delay, including the final fall-through.
 * Native mapping: Guest addresses remain validated uint32_t values with per-byte knownness and little-endian accesses; the unresolved child is a full-GPR callback and no guest integer is cast to a host pointer.
 */
int nba97_game_controller_frame_reset(
    Nba97GameControllerFrameResetContext*,
    Nba97GameControllerFrameResetProgress*);

#ifdef __cplusplus
}
#endif
#endif
