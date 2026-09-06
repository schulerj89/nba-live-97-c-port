#ifndef NBA97_GAME_OPPONENT_CONTACT_H
#define NBA97_GAME_OPPONENT_CONTACT_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameOpponentContactWord;
typedef Nba97GameMatchClocksMachine Nba97GameOpponentContactMachine;
typedef Nba97GameMatchClocksAccess Nba97GameOpponentContactAccess;

typedef struct Nba97GameOpponentContactEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t argument_count;
} Nba97GameOpponentContactEvent;

/* The child observes JAL ra and its NOP delay. It may mutate every GPR,
 * HI/LO, retained memory, a2, sp, ra, and the saved stack word. */
typedef int (*Nba97GameOpponentContactIo)(void *, const Nba97GameTextMemory *,
                                          const Nba97GameOpponentContactEvent *,
                                          Nba97GameOpponentContactMachine *);

typedef struct Nba97GameOpponentContactContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameOpponentContactMachine machine;
  Nba97GameOpponentContactIo io;
  void *user;
  Nba97GameOpponentContactAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameOpponentContactContext;

typedef struct Nba97GameOpponentContactProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t child_calls;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameOpponentContactWord first_c2;
  Nba97GameOpponentContactWord second_c2;
  Nba97GameOpponentContactWord option;
  Nba97GameOpponentContactWord phase;
  Nba97GameOpponentContactWord last_predicate;
  Nba97GameOpponentContactWord second_da;
  Nba97GameOpponentContactWord first_da;
  Nba97GameOpponentContactWord owner;
  Nba97GameOpponentContactWord first_id;
  Nba97GameOpponentContactWord restored_return_address;
  Nba97GameOpponentContactWord returned_value;
  Nba97GameOpponentContactMachine machine;
  uint8_t completed;
} Nba97GameOpponentContactProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8005F888
 * Range: 0x8005F888..0x8005F947 (inclusive)
 * Source size: 192 bytes / 48 instructions
 * Evidence: fresh Ghidra game_8005f888.txt; instruction SHA-256
 * b5842ef025359b52db720caa919e28d7aa305df4eb97167bb8592f55239c20d5
 *
 * Purpose: Order an opposing actor pair for contact dispatch after source
 * option, phase, flag, and ownership gates.
 * Inputs: All 32 live GPRs and HI/LO; a0 first actor, a1 second actor, live
 * sp/ra, actor C2/DA/ID fields, option 0x80021D8A, phase 0x800FDB90, owner
 * 0x800FDBCC, and typed child 0x8005F3BC.
 * Returns: Zero on a rejected gate; otherwise the completed child's v0 low
 * byte, with child-live machine state except v0 and the live-sp ra epilogue.
 * Guest memory: Saves ra at live sp-8; conditionally reads first/second C2,
 * option, phase, DA flags, owner before first ID, then reloads ra through the
 * child-mutable live sp.
 * Calls: 0x8005F3BC at 0x8005F92C with two arguments and a NOP delay.
 * Original quirks: Both zero C2 values bypass option and phase; signed-negative
 * phase passes the `<129` gate; both DA branches publish their a0 delay move;
 * owner is sign-extended before full-word ID comparison.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory
 * values with byte knownness; the unresolved child is a full-machine typed
 * callback and no guest integer is cast to a host pointer.
 */
int nba97_game_opponent_contact(Nba97GameOpponentContactContext *,
                                Nba97GameOpponentContactProgress *);

#ifdef __cplusplus
}
#endif
#endif
