#ifndef NBA97_GAME_SCENE_LOAD_H
#define NBA97_GAME_SCENE_LOAD_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameSceneLoadWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameSceneLoadRegisters;

enum Nba97GameSceneLoadCallKind {
    NBA97_GAME_SCENE_LOAD_CHILD_800802AC = 1,
    NBA97_GAME_SCENE_LOAD_CHILD_80048D5C = 2
};

typedef struct Nba97GameSceneLoadEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based parent access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameSceneLoadEvent;

/* The callback observes all 32 live GPRs after JAL assigned ra and the NOP
 * delay slot completed. It may mutate retained memory and every live GPR.
 * Return 1 only when the original child boundary returned. */
typedef int (*Nba97GameSceneLoadIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameSceneLoadEvent*, Nba97GameSceneLoadRegisters*);

enum Nba97GameSceneLoadAccessKind {
    NBA97_GAME_SCENE_LOAD_READ = 1,
    NBA97_GAME_SCENE_LOAD_STORE = 2
};

typedef struct Nba97GameSceneLoadAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameSceneLoadAccess;

typedef struct Nba97GameSceneLoadContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameSceneLoadRegisters registers;
    Nba97GameSceneLoadIo io;
    void* user;
    Nba97GameSceneLoadAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameSceneLoadContext;

typedef struct Nba97GameSceneLoadProgress {
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
    Nba97GameSceneLoadWord restored_return_address;
    Nba97GameSceneLoadRegisters registers;
    uint8_t completed;
} Nba97GameSceneLoadProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8002DB68
 * Range: 0x8002DB68..0x8002DB8F (inclusive)
 * Source size: 40 bytes / 10 instructions
 * Evidence: fresh Ghidra game_8002db68.txt; routine SHA-256 5e2cf98a5fc45be394c4897b6d4701580cf2ac3bd483b3d16dac2b6d2dab1d5b
 *
 * Purpose: Preserve the caller return address while invoking the two ordered scene-loading services.
 * Inputs: All 32 live MIPS GPRs, including known sp for frame formation, plus retained stack memory and two typed child services.
 * Returns: Final child v0 and every other live GPR, except ra reloaded through live sp and sp advanced by 0x18.
 * Guest memory: Stores entry ra at (entry sp-0x18)+0x10, then after both calls reads ra at live sp+0x10; both addresses use wrapping 32-bit arithmetic.
 * Calls: 0x800802AC at 0x8002DB70, then 0x80048D5C at 0x8002DB78; both delay slots are NOP.
 * Original quirks: Child GPR and stack mutations remain live, so the epilogue may reload ra from a relocated frame; the final child v0 is incidental but retained.
 * Native mapping: 32-bit guest addresses use validated retained regions; both unresolved original children remain explicit typed callbacks with no host-pointer casts.
 */
int nba97_game_scene_load(Nba97GameSceneLoadContext*,
    Nba97GameSceneLoadProgress*);

#ifdef __cplusplus
}
#endif
#endif
