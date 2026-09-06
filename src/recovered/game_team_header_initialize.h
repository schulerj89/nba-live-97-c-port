#ifndef NBA97_GAME_TEAM_HEADER_INITIALIZE_H
#define NBA97_GAME_TEAM_HEADER_INITIALIZE_H

#include "game_match_state_reset.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchStateResetWord Nba97GameTeamHeaderInitializeWord;
typedef Nba97GameMatchStateResetRegisters
    Nba97GameTeamHeaderInitializeRegisters;

typedef struct Nba97GameTeamHeaderInitializeMachine {
  Nba97GameTeamHeaderInitializeRegisters registers;
  Nba97GameTeamHeaderInitializeWord hi;
  Nba97GameTeamHeaderInitializeWord lo;
} Nba97GameTeamHeaderInitializeMachine;

enum Nba97GameTeamHeaderInitializeAccessKind {
  NBA97_GAME_TEAM_HEADER_INITIALIZE_READ = 1,
  NBA97_GAME_TEAM_HEADER_INITIALIZE_STORE = 2
};

typedef struct Nba97GameTeamHeaderInitializeAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameTeamHeaderInitializeAccess;

typedef struct Nba97GameTeamHeaderInitializeContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameTeamHeaderInitializeMachine machine;
  Nba97GameTeamHeaderInitializeAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameTeamHeaderInitializeContext;

typedef struct Nba97GameTeamHeaderInitializeProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t status_iterations;
  size_t unused_iterations;
  size_t actor_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameTeamHeaderInitializeMachine machine;
  uint8_t completed;
} Nba97GameTeamHeaderInitializeProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800655B0
 * Range: 0x800655B0..0x8006581F (inclusive)
 * Source size: 624 bytes / 156 instructions
 * Evidence: fresh Ghidra game_800655b0.txt; instruction SHA-256 c9bee46f4653d4e1c816cf14503651e4feb900aae914bcb804c9723f34d6f046
 *
 * Purpose: Initialize one retained team header, its twelve status slots, and five actor links from the live side pair and metadata tables.
 * Inputs: Full live GPR/HI/LO state; a0 is the destination team header, a1 is the opposing header, and retained globals and actor tables are mapped guest memory.
 * Returns: All source-mutated GPRs, sp restored by 0x10, ra used as the indirect return target, and HI/LO unchanged.
 * Guest memory: Reads and writes the team headers, side status table, metadata pointer/count/rank globals, actor-pointer table, and five actor records in exact source order.
 * Calls: None observed.
 * Original quirks: Count clamps to 12, the status loop rereads the live stored count after every write, actor registration descends from local index 4 to 0, low-halfword arithmetic wraps, and three source clamp instructions are unreachable.
 * Native mapping: Guest addresses remain validated uint32_t values over retained regions with per-byte knownness and an observable access journal; no host pointer casts are used.
 */
int nba97_game_team_header_initialize(
    Nba97GameTeamHeaderInitializeContext *,
    Nba97GameTeamHeaderInitializeProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
