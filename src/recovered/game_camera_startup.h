#ifndef NBA97_GAME_CAMERA_STARTUP_H
#define NBA97_GAME_CAMERA_STARTUP_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameCameraStartupWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameCameraStartupRegisters;

enum Nba97GameCameraStartupCallKind {
    NBA97_GAME_CAMERA_STARTUP_CHILD_800799CC = 1
};

typedef struct Nba97GameCameraStartupEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based mapped-access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameCameraStartupEvent;

/* The callback observes all 32 live GPRs after JAL assigned ra and after the
 * A1=0 delay slot. It may synchronously mutate retained bytes and every GPR.
 * Return 1 only when the original 0x800799CC boundary returned. */
typedef int (*Nba97GameCameraStartupIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameCameraStartupEvent*, Nba97GameCameraStartupRegisters*);

enum Nba97GameCameraStartupAccessKind {
    NBA97_GAME_CAMERA_STARTUP_READ = 1,
    NBA97_GAME_CAMERA_STARTUP_STORE = 2
};

typedef struct Nba97GameCameraStartupAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask; /* One bit per little-endian source byte. */
    uint8_t kind;
} Nba97GameCameraStartupAccess;

typedef struct Nba97GameCameraStartupContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameCameraStartupRegisters registers;
    Nba97GameCameraStartupIo io;
    void* user;
    Nba97GameCameraStartupAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameCameraStartupContext;

typedef struct Nba97GameCameraStartupProgress {
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
    Nba97GameCameraStartupWord initial_camera_byte;
    Nba97GameCameraStartupWord initial_aux_byte;
    Nba97GameCameraStartupWord restored_return_address;
    Nba97GameCameraStartupRegisters registers;
    uint8_t completed;
} Nba97GameCameraStartupProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80079664
 * Range: 0x80079664..0x80079757 (inclusive)
 * Source size: 244 bytes / 61 instructions
 * Evidence: fresh Ghidra game_80079664.txt; routine SHA-256 dce17f8d8df1a0f17fbe9e2b728274a7cceb1d1760633a75827575a3adc0c2cb
 *
 * Purpose: Initialize retained camera globals, invoke the camera-mode child, and publish three camera source words.
 * Inputs: All 32 live MIPS GPRs, bytes at 0x80021ED7/0x80021ED9/0x80021EDA, source words at 0x800BC3D4..0x800BC3DC, retained stack memory, and the typed 0x800799CC child.
 * Returns: Every live GPR after the child and subsequent source instructions, with ra reloaded through live sp and sp advanced by 0x18.
 * Guest memory: Reads two bytes before framing, saves ra, publishes camera bytes and constants, optionally reads a mode byte, reads three source words, clears/publishes camera state, then reloads ra in exact source order.
 * Calls: 0x800799CC at 0x800796B8 for a0!=1, or at 0x800796E4 for a0==1; both set a1=0 in the JAL delay slot.
 * Original quirks: The entry branch always executes a0=12 in its delay slot; an unknown branch retains that write before stopping; child-mutated GPRs, globals and sp remain live; byte knownness and 32-bit address arithmetic are preserved.
 * Native mapping: All guest addresses use validated retained regions with per-byte knownness; 0x800799CC remains a full-GPR typed callback and guest integers are never host pointers.
 */
int nba97_game_camera_startup(Nba97GameCameraStartupContext*,
    Nba97GameCameraStartupProgress*);

#ifdef __cplusplus
}
#endif
#endif
