#ifndef NBA97_GAME_CAMERA_SELECT_H
#define NBA97_GAME_CAMERA_SELECT_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameCameraSelectWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameCameraSelectRegisters;

enum Nba97GameCameraSelectCallKind {
    NBA97_GAME_CAMERA_SELECT_CHILD_8007E26C = 1,
    NBA97_GAME_CAMERA_SELECT_CHILD_8007C964,
    NBA97_GAME_CAMERA_SELECT_CHILD_8007D3C8,
    NBA97_GAME_CAMERA_SELECT_CHILD_8007CC3C,
    NBA97_GAME_CAMERA_SELECT_CHILD_8007CAF4,
    NBA97_GAME_CAMERA_SELECT_CHILD_8007A19C,
    NBA97_GAME_CAMERA_SELECT_CHILD_8007A3A0,
    NBA97_GAME_CAMERA_SELECT_CHILD_80079F78,
    NBA97_GAME_CAMERA_SELECT_CHILD_80079EBC,
    NBA97_GAME_CAMERA_SELECT_CHILD_800798B4
};

enum Nba97GameCameraSelectExitKind {
    NBA97_GAME_CAMERA_SELECT_EXIT_NONE = 0,
    NBA97_GAME_CAMERA_SELECT_EXIT_NORMAL,
    NBA97_GAME_CAMERA_SELECT_EXIT_MODE_ZERO,
    NBA97_GAME_CAMERA_SELECT_EXIT_ALREADY_SELECTED
};

typedef struct Nba97GameCameraSelectEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based mapped-access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameCameraSelectEvent;

/* The callback observes every live GPR after JAL assigned ra and after the
 * source delay slot. It may synchronously mutate retained bytes and all GPRs.
 * Return 1 only when the unresolved original boundary returned. */
typedef int (*Nba97GameCameraSelectIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameCameraSelectEvent*, Nba97GameCameraSelectRegisters*);

enum Nba97GameCameraSelectAccessKind {
    NBA97_GAME_CAMERA_SELECT_READ = 1,
    NBA97_GAME_CAMERA_SELECT_STORE = 2
};

typedef struct Nba97GameCameraSelectAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask; /* One bit per little-endian source byte. */
    uint8_t kind;
} Nba97GameCameraSelectAccess;

typedef struct Nba97GameCameraSelectContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameCameraSelectRegisters registers;
    Nba97GameCameraSelectIo io;
    void* user;
    Nba97GameCameraSelectAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameCameraSelectContext;

typedef struct Nba97GameCameraSelectProgress {
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
    Nba97GameCameraSelectWord restored_return_address;
    Nba97GameCameraSelectRegisters registers;
    uint8_t exit_kind;
    uint8_t completed;
} Nba97GameCameraSelectProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800799CC
 * Range: 0x800799CC..0x80079D37 (inclusive)
 * Source size: 876 bytes / 219 instructions
 * Evidence: fresh Ghidra game_800799cc.txt; routine SHA-256 7ebbad49fdd6daaf0fb679d5c20123ac73e9d5b2725485e216aa6b0681ce2035
 *
 * Purpose: Select a camera mode, dispatch its setup providers, and preserve or reset the retained camera-state block.
 * Inputs: All 32 live MIPS GPRs; a0 mode, a1 preservation selector, retained globals/table/stack bytes, and ten typed unresolved child boundaries.
 * Returns: Every live GPR after callbacks and source instructions, with ra/s1/s0 reloaded through callback-live sp and sp advanced by 0x58.
 * Guest memory: Saves a 0x58-byte frame; reads and publishes camera globals/table entries; conditionally copies 14 words through guest stack or resets twelve camera words; writes busy and completion globals in exact source order.
 * Calls: 0x8007E26C, 0x8007C964, 0x8007D3C8, 0x8007CC3C, 0x8007CAF4, 0x8007A19C, 0x8007A3A0, 0x80079F78, 0x80079EBC, and 0x800798B4 at their source call PCs.
 * Original quirks: Mode zero clears a prior nonzero selection before its child; modes 200..203 can return with busy byte 0x801029F8 still one; signed dispatch and unchecked wrapped table indices are retained; callback-mutated sp/GPRs and partial failure prefixes remain live.
 * Native mapping: Guest addresses use validated retained regions with per-byte knownness; unresolved callees receive full-GPR typed events and guest integers are never host pointers.
 */
int nba97_game_camera_select(Nba97GameCameraSelectContext*,
    Nba97GameCameraSelectProgress*);

#ifdef __cplusplus
}
#endif
#endif
