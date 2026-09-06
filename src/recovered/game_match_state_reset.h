#ifndef NBA97_GAME_MATCH_STATE_RESET_H
#define NBA97_GAME_MATCH_STATE_RESET_H

#include "game_match_initialize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchInitializeWord Nba97GameMatchStateResetWord;
typedef Nba97GameMatchInitializeRegisters Nba97GameMatchStateResetRegisters;

typedef struct Nba97GameMatchStateResetMachine {
  Nba97GameMatchStateResetRegisters registers;
  Nba97GameMatchStateResetWord hi;
  Nba97GameMatchStateResetWord lo;
} Nba97GameMatchStateResetMachine;

enum Nba97GameMatchStateResetCallKind {
  NBA97_GAME_MATCH_STATE_RESET_ZERO = 1,
  NBA97_GAME_MATCH_STATE_RESET_80083490,
  NBA97_GAME_MATCH_STATE_RESET_80063D58,
  NBA97_GAME_MATCH_STATE_RESET_800655B0,
  NBA97_GAME_MATCH_STATE_RESET_80065328,
  NBA97_GAME_MATCH_STATE_RESET_80065DB0,
  NBA97_GAME_MATCH_STATE_RESET_80065820,
  NBA97_GAME_MATCH_STATE_RESET_800646A8,
  NBA97_GAME_MATCH_STATE_RESET_80076AD0,
  NBA97_GAME_MATCH_STATE_RESET_8006432C,
  NBA97_GAME_MATCH_STATE_RESET_CALL_KIND_COUNT
};

typedef struct Nba97GameMatchStateResetEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameMatchStateResetEvent;

typedef int (*Nba97GameMatchStateResetIo)(void *, const Nba97GameTextMemory *,
                                          const Nba97GameMatchStateResetEvent *,
                                          Nba97GameMatchStateResetMachine *);

enum Nba97GameMatchStateResetAccessKind {
  NBA97_GAME_MATCH_STATE_RESET_READ = 1,
  NBA97_GAME_MATCH_STATE_RESET_STORE = 2
};

typedef struct Nba97GameMatchStateResetAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameMatchStateResetAccess;

typedef struct Nba97GameMatchStateResetContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameMatchStateResetMachine machine;
  Nba97GameMatchStateResetIo io;
  void *user;
  Nba97GameMatchStateResetAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameMatchStateResetContext;

typedef struct Nba97GameMatchStateResetProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_MATCH_STATE_RESET_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_MATCH_STATE_RESET_CALL_KIND_COUNT];
  size_t spin_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameMatchStateResetWord restored_return_address;
  Nba97GameMatchStateResetWord restored_s1;
  Nba97GameMatchStateResetWord restored_s0;
  Nba97GameMatchStateResetMachine machine;
  uint8_t mode_98;
  uint8_t completed;
} Nba97GameMatchStateResetProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800659F0
 * Range: 0x800659F0..0x80065B17 (inclusive)
 * Source size: 296 bytes / 74 instructions
 * Evidence: fresh Ghidra game_800659f0.txt; instruction SHA-256 bae52046d74f276be2f00c026cb35b54ac140f1d8b3a07a44bcc987086107522
 *
 * Purpose: Clear and rebuild retained match state, initialize paired team structures, and dispatch the mode-specific reset child.
 * Inputs: Full live GPR/HI/LO state, retained stack and match-state memory, and typed child services.
 * Returns: Final child register state with ra/s1/s0 restored through live sp, sp advanced by 0x20, and live HI/LO transported unchanged except for child mutations.
 * Guest memory: Saves/restores three stack words; clears four ranges through typed calls; stores 0x8001EDF2, 0x800FDB9C, 0x8001EECC, and 0x800FDB54; reads 0x8001EDEC in exact source order.
 * Calls: 0x800A3A74 at 0x80065A0C/18/24/30; 0x80083490 at 0x80065A38; 0x80063D58 at 0x80065A54; 0x800655B0 at 0x80065A88/94; 0x80065328 at 0x80065A9C; 0x80065DB0 at 0x80065AA4; 0x80065820 at 0x80065ABC/C4; 0x800646A8 at 0x80065ACC; mode 98 calls 0x80076AD0 at 0x80065AE8, otherwise 0x8006432C at 0x80065AF8.
 * Original quirks: The fixed decrement loop finishes with v0=-2; every call consumes callback-live registers; several stores execute in JAL delay slots; mode is tested only after v0=98 is assigned in the LHU load-delay instruction.
 * Native mapping: Guest addresses are validated uint32_t mapped values with per-byte knownness; every source callee remains a typed full-machine boundary.
 */
int nba97_game_match_state_reset(Nba97GameMatchStateResetContext *,
                                 Nba97GameMatchStateResetProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
