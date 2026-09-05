#ifndef NBA97_GAME_SCENE_RANDOM_WARMUP_H
#define NBA97_GAME_SCENE_RANDOM_WARMUP_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameSceneRandomWarmupWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameSceneRandomWarmupRegisters;

enum Nba97GameSceneRandomWarmupCallKind {
    NBA97_GAME_SCENE_RANDOM_WARMUP_STARTUP_800800F8 = 1,
    NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70 = 2,
    NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694 = 3,
    NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4 = 4
};

typedef struct Nba97GameSceneRandomWarmupEvent {
    uint32_t pc;
    uint32_t delay_slot_pc;
    uint32_t entry;
    size_t operation; /* One-based parent access/call order. */
    size_t invocation; /* One-based invocation of this call kind. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameSceneRandomWarmupEvent;

/* The callback observes all 32 live GPRs after JAL assigned ra and after its
 * delay slot completed. It may mutate retained memory and every live GPR.
 * Return 1 only when the original child boundary returned. */
typedef int (*Nba97GameSceneRandomWarmupIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameSceneRandomWarmupEvent*,
    Nba97GameSceneRandomWarmupRegisters*);

enum Nba97GameSceneRandomWarmupAccessKind {
    NBA97_GAME_SCENE_RANDOM_WARMUP_READ = 1,
    NBA97_GAME_SCENE_RANDOM_WARMUP_STORE = 2
};

typedef struct Nba97GameSceneRandomWarmupAccess {
    uint32_t pc;
    uint32_t address;
    uint32_t value;
    size_t operation;
    uint8_t width;
    uint8_t known_mask;
    uint8_t kind;
} Nba97GameSceneRandomWarmupAccess;

typedef struct Nba97GameSceneRandomWarmupContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Attempted mapped accesses and child calls. */
    Nba97GameSceneRandomWarmupRegisters registers;
    Nba97GameSceneRandomWarmupIo io;
    void* user;
    Nba97GameSceneRandomWarmupAccess* access_journal;
    size_t access_journal_capacity;
} Nba97GameSceneRandomWarmupContext;

typedef struct Nba97GameSceneRandomWarmupProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t access_events;
    size_t startup_calls;
    size_t random_calls;
    size_t seed_calls;
    size_t step_calls;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    Nba97GameSceneRandomWarmupWord warmup_count;
    Nba97GameSceneRandomWarmupWord seed_argument;
    Nba97GameSceneRandomWarmupWord restored_return_address;
    Nba97GameSceneRandomWarmupWord restored_s0;
    Nba97GameSceneRandomWarmupRegisters registers;
    uint8_t completed;
} Nba97GameSceneRandomWarmupProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800802AC
 * Range: 0x800802AC..0x80080303 (inclusive)
 * Source size: 88 bytes / 22 instructions
 * Evidence: fresh Ghidra game_800802ac.txt; routine SHA-256 48d621e3c50d99bbcd8289dfb45f224bc03aa5cfdf642239bf198016741459cf
 *
 * Purpose: Initialize the scene random service, seed it from one random result, and run a randomized number of warm-up steps.
 * Inputs: All 32 live MIPS GPRs, retained stack memory, and four typed child services; a partial sp propagates through frame formation but must be concrete at the first store.
 * Returns: Final child v0 and all other live GPRs, except ra/s0 reloaded through live sp and sp advanced by 0x18.
 * Guest memory: Stores entry ra and s0 at frame+0x14/frame+0x10, then reloads both through the child-mutable live sp; all addresses use wrapping 32-bit arithmetic.
 * Calls: 0x800800F8, 0x8002AB70 twice, 0x80093694, then 0x800935C4 until live s0 is zero.
 * Original quirks: The first random result contributes only low 7 bits; the second only low 16 bits; the step-count decrement is a JAL delay slot and each child may replace live s0.
 * Native mapping: 32-bit guest addresses use validated retained regions; every unresolved original child is an explicit typed callback and no native RNG is substituted.
 */
int nba97_game_scene_random_warmup(
    Nba97GameSceneRandomWarmupContext*,
    Nba97GameSceneRandomWarmupProgress*);

#ifdef __cplusplus
}
#endif
#endif
