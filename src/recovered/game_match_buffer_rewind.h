#ifndef NBA97_GAME_MATCH_BUFFER_REWIND_H
#define NBA97_GAME_MATCH_BUFFER_REWIND_H

#include "game_match_state_reset.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchStateResetWord Nba97GameMatchBufferRewindWord;
typedef Nba97GameMatchStateResetRegisters Nba97GameMatchBufferRewindRegisters;
typedef Nba97GameMatchStateResetMachine Nba97GameMatchBufferRewindMachine;

enum Nba97GameMatchBufferRewindCallKind {
  NBA97_GAME_MATCH_BUFFER_REWIND_ZERO = 1,
  NBA97_GAME_MATCH_BUFFER_REWIND_CALL_KIND_COUNT
};

typedef struct Nba97GameMatchBufferRewindEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameMatchBufferRewindEvent;

typedef int (*Nba97GameMatchBufferRewindIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameMatchBufferRewindEvent *,
    Nba97GameMatchBufferRewindMachine *);

enum Nba97GameMatchBufferRewindAccessKind {
  NBA97_GAME_MATCH_BUFFER_REWIND_READ = 1,
  NBA97_GAME_MATCH_BUFFER_REWIND_STORE = 2
};

typedef struct Nba97GameMatchBufferRewindAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameMatchBufferRewindAccess;

typedef struct Nba97GameMatchBufferRewindContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameMatchBufferRewindMachine machine;
  Nba97GameMatchBufferRewindIo io;
  void *user;
  Nba97GameMatchBufferRewindAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameMatchBufferRewindContext;

typedef struct Nba97GameMatchBufferRewindProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_MATCH_BUFFER_REWIND_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_MATCH_BUFFER_REWIND_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameMatchBufferRewindWord restored_return_address;
  Nba97GameMatchBufferRewindMachine machine;
  uint8_t completed;
} Nba97GameMatchBufferRewindProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80076AD0
 * Range: 0x80076AD0..0x80076B27 (inclusive)
 * Source size: 88 bytes / 22 instructions
 * Evidence: fresh Ghidra game_80076ad0.txt; instruction SHA-256 520b041e803da61c6611b2b0a99b3da28da49315300930bd305b92ad7a4f54f6
 *
 * Purpose: Copy the retained match-buffer pointer pair, clear the four-byte rewind buffer, and reset its retained flags.
 * Inputs: Full live GPR/HI/LO state, retained stack and globals, and the typed 0x800A3A74 zero service.
 * Returns: The zero child's v0 and other live machine state, with ra reloaded through callback-live sp, sp advanced by 0x18, and HI/LO transported.
 * Guest memory: Reads 0x800FA004 and callback-live sp+0x10; saves ra, copies the pointer to 0x800FA00C/10, clears 0x800F1918..1B through the child, then stores zero to 0x800FE860, 0x8002148C, and 0x800FE864 in order.
 * Calls: 0x800A3A74 at 0x80076AF8 with a1=4 in the delay slot.
 * Original quirks: The child executes the optimized length-at-least-four path, issuing overlapping SWR and SWL stores to the same four bytes; callback mutations to sp and the saved return slot remain live.
 * Native mapping: Guest addresses remain validated uint32_t values over retained regions with per-byte knownness, typed full-machine callback state, and an observable access journal.
 */
int nba97_game_match_buffer_rewind(Nba97GameMatchBufferRewindContext *,
                                   Nba97GameMatchBufferRewindProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
