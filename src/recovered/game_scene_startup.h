#ifndef NBA97_GAME_SCENE_STARTUP_H
#define NBA97_GAME_SCENE_STARTUP_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameSceneStartupWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameSceneStartupRegisters;

enum Nba97GameSceneStartupCallKind {
    NBA97_GAME_SCENE_STARTUP_CONTROLLER_8008F224 = 1,
    NBA97_GAME_SCENE_STARTUP_CHILD_8004D38C,
    NBA97_GAME_SCENE_STARTUP_CHILD_80052C20,
    NBA97_GAME_SCENE_STARTUP_CHILD_800A7738,
    NBA97_GAME_SCENE_STARTUP_CHILD_80056074,
    NBA97_GAME_SCENE_STARTUP_CHILD_8005605C,
    NBA97_GAME_SCENE_STARTUP_DISPLAY_80099CA4,
    NBA97_GAME_SCENE_STARTUP_DRAW_80099ACC,
    NBA97_GAME_SCENE_STARTUP_ATTRIBUTES_80063EDC,
    NBA97_GAME_SCENE_STARTUP_CHILD_80056944
};

typedef struct Nba97GameSceneStartupEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based parent access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameSceneStartupEvent;

/* The callback observes all 32 live GPRs after JAL assigned ra and after its
 * delay slot executed. It may synchronously mutate retained bytes and every
 * GPR. Return 1 only when the original child boundary returned. */
typedef int (*Nba97GameSceneStartupIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameSceneStartupEvent*, Nba97GameSceneStartupRegisters*);

enum Nba97GameSceneStartupAccessKind {
    NBA97_GAME_SCENE_STARTUP_READ = 1,
    NBA97_GAME_SCENE_STARTUP_STORE = 2
};

typedef struct Nba97GameSceneStartupAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameSceneStartupAccess;

typedef struct Nba97GameSceneStartupContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameSceneStartupRegisters registers;
    Nba97GameSceneStartupIo io;
    void* user;
    Nba97GameSceneStartupAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameSceneStartupContext;

typedef struct Nba97GameSceneStartupProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t controller_iterations;
    size_t controller_matches;
    size_t roster_iterations;
    size_t entity_iterations;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameSceneStartupWord restored_return_address;
    Nba97GameSceneStartupRegisters registers;
    uint8_t completed;
} Nba97GameSceneStartupProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80048D5C
 * Range: 0x80048D5C..0x80048FE3 (inclusive)
 * Source size: 648 bytes / 162 instructions
 * Evidence: fresh Ghidra game_80048d5c.txt; routine SHA-256 e4d0c3f2d16ed75081a530cb5140aa37f4a82abd36ec7cdd0a59fc0bed7b5744; evidence-file SHA-256 7d538dcb4b19de3bdea4986878d78be9a623d512bb541cab90234f1486ddb06f
 *
 * Purpose: Reset retained scene state, publish roster identities, start rendering resources, and select both display buffers.
 * Inputs: All 32 live MIPS GPRs, retained stack/global/roster/entity mappings, and typed services for all nineteen ordered child calls.
 * Returns: Final child v0 and every other live GPR, except ra/s0..s3 reloaded through live sp and sp advanced by 0x28.
 * Guest memory: Saves five GPRs in the live stack frame; resets scene/controller globals; follows live roster/entity pointers; writes signed roster IDs, camera and double-buffer state; then reloads five saved GPRs in source order.
 * Calls: Eight calls to 0x8008F224, then 0x8004D38C, 0x80052C20, 0x800A7738, 0x80056074, 0x8005605C, 0x80099CA4, 0x80099ACC, 0x80099CA4, 0x80099ACC, 0x80063EDC, and 0x80056944.
 * Original quirks: Controller and roster loops use child-mutated live GPRs; signed halfwords become 32-bit IDs; pointer tables and selectors are reloaded; nonboolean selectors invert to zero; callback-mutated bases and live sp affect later accesses; all 32-bit address arithmetic wraps.
 * Native mapping: 32-bit guest addresses use validated retained regions with per-byte knownness; every unresolved original callee remains an explicit typed callback.
 */
int nba97_game_scene_startup(Nba97GameSceneStartupContext*,
    Nba97GameSceneStartupProgress*);

#ifdef __cplusplus
}
#endif
#endif
