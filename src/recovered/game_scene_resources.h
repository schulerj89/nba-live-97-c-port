#ifndef NBA97_GAME_SCENE_RESOURCES_H
#define NBA97_GAME_SCENE_RESOURCES_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameSceneResourcesWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameSceneResourcesRegisters;

enum Nba97GameSceneResourcesCallKind {
    NBA97_GAME_SCENE_RESOURCES_CHILD_800536A0 = 1,
    NBA97_GAME_SCENE_RESOURCES_CHILD_8004D490,
    NBA97_GAME_SCENE_RESOURCES_CHILD_80029BFC,
    NBA97_GAME_SCENE_RESOURCES_CHILD_80029BCC,
    NBA97_GAME_SCENE_RESOURCES_CHILD_800516E4,
    NBA97_GAME_SCENE_RESOURCES_CHILD_80029BD4,
    NBA97_GAME_SCENE_RESOURCES_CHILD_800A3FEC,
    NBA97_GAME_SCENE_RESOURCES_CHILD_80051294,
    NBA97_GAME_SCENE_RESOURCES_CHILD_80090160,
    NBA97_GAME_SCENE_RESOURCES_CHILD_8004DC08,
    NBA97_GAME_SCENE_RESOURCES_CHILD_8004FD38,
    NBA97_GAME_SCENE_RESOURCES_CHILD_800994F4,
    NBA97_GAME_SCENE_RESOURCES_CHILD_80090698,
    NBA97_GAME_SCENE_RESOURCES_CHILD_8004FD48,
    NBA97_GAME_SCENE_RESOURCES_CHILD_800504A8,
    NBA97_GAME_SCENE_RESOURCES_CHILD_80050DD0,
    NBA97_GAME_SCENE_RESOURCES_CHILD_80050DC8,
    NBA97_GAME_SCENE_RESOURCES_CHILD_800479B8
};

typedef struct Nba97GameSceneResourcesEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based parent access/call order. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameSceneResourcesEvent;

/* The callback observes all 32 live GPRs after JAL assigned ra and its delay
 * slot completed. It may synchronously mutate retained memory and every GPR.
 * Return 1 only when the original child boundary returned. */
typedef int (*Nba97GameSceneResourcesIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameSceneResourcesEvent*, Nba97GameSceneResourcesRegisters*);

enum Nba97GameSceneResourcesAccessKind {
    NBA97_GAME_SCENE_RESOURCES_READ = 1,
    NBA97_GAME_SCENE_RESOURCES_STORE = 2
};

typedef struct Nba97GameSceneResourcesAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameSceneResourcesAccess;

typedef struct Nba97GameSceneResourcesContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameSceneResourcesRegisters registers;
    Nba97GameSceneResourcesIo io;
    void* user;
    Nba97GameSceneResourcesAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameSceneResourcesContext;

typedef struct Nba97GameSceneResourcesProgress {
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
    Nba97GameSceneResourcesWord restored_return_address;
    Nba97GameSceneResourcesWord restored_saved_register[3]; /* s2,s1,s0. */
    Nba97GameSceneResourcesRegisters registers;
    uint8_t completed;
} Nba97GameSceneResourcesProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80052C20
 * Range: 0x80052C20..0x800530FB (inclusive)
 * Source size: 1244 bytes / 311 instructions
 * Evidence: fresh Ghidra listing game_80052c20.txt; routine SHA-256 d724ab2a6e1540262e72275fab86b346afcb52bd8c74186e16d1831922f00720
 *
 * Purpose: Orchestrate scene roster, player, head, palette, presentation, and net resource lifetimes for normal and alternate startup modes.
 * Inputs: All live MIPS GPRs; live mode flag 0x800EB678, team indices 0x80021D74/0x80021D78, team resource pointer tables, retained resource globals, and mutable stack memory.
 * Returns: All final live GPRs after ra/s2/s1/s0 reload through the live sp and the JR NOP delay slot.
 * Guest memory: Reads every evidenced live flag, team index/table slot, resource pointer, and saved stack word; writes scene flags, resource publications, lookup tables, and saved frame words in exact source order.
 * Calls: 0x800536A0, 0x8004D490, 0x80029BFC, 0x80029BCC, 0x800516E4, 0x80029BD4, 0x800A3FEC, 0x80051294, 0x80090160, 0x8004DC08, 0x8004FD38, 0x800994F4, 0x80090698, 0x8004FD48, 0x800504A8, 0x80050DD0, 0x80050DC8, and 0x800479B8 in source-selected order.
 * Original quirks: Mode and resource globals reload after mutable children; team/table indices and all address arithmetic are unchecked and wrapping; child-mutated loop/cursor registers remain live; JAL, branch, and jump delay slots execute exactly.
 * Native mapping: 32-bit guest addresses use validated retained regions with per-byte knownness; unresolved children are full-GPR typed callbacks and no guest integer is cast to a host pointer.
 */
int nba97_game_scene_resources(Nba97GameSceneResourcesContext*,
    Nba97GameSceneResourcesProgress*);

#ifdef __cplusplus
}
#endif
#endif
