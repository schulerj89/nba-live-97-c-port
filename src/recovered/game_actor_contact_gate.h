#ifndef NBA97_GAME_ACTOR_CONTACT_GATE_H
#define NBA97_GAME_ACTOR_CONTACT_GATE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameActorContactGateWord;
typedef Nba97GameMatchClocksMachine Nba97GameActorContactGateMachine;
typedef Nba97GameMatchClocksAccess Nba97GameActorContactGateAccess;

enum Nba97GameActorContactGateCallKind {
  NBA97_GAME_ACTOR_CONTACT_GATE_CHILD_8005F948 = 1,
  NBA97_GAME_ACTOR_CONTACT_GATE_CALL_KIND_COUNT
};

typedef struct Nba97GameActorContactGateEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameActorContactGateEvent;

/* The callback observes JAL's ra and the arithmetic-shifted a2 after the
 * delay slot. It may mutate every GPR, HI/LO, and mapped retained memory. */
typedef int (*Nba97GameActorContactGateIo)(void *,
    const Nba97GameTextMemory *, const Nba97GameActorContactGateEvent *,
    Nba97GameActorContactGateMachine *);

typedef struct Nba97GameActorContactGateContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameActorContactGateMachine machine;
  Nba97GameActorContactGateIo io;
  void *user;
  Nba97GameActorContactGateAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameActorContactGateContext;

typedef struct Nba97GameActorContactGateProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_ACTOR_CONTACT_GATE_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameActorContactGateWord saved_return_address;
  Nba97GameActorContactGateWord second_coordinate;
  Nba97GameActorContactGateWord first_coordinate;
  Nba97GameActorContactGateWord coordinate_difference;
  Nba97GameActorContactGateWord coordinate_gate;
  Nba97GameActorContactGateWord shifted_difference;
  Nba97GameActorContactGateWord restored_return_address;
  Nba97GameActorContactGateWord returned_value;
  Nba97GameActorContactGateMachine machine;
  uint8_t completed;
} Nba97GameActorContactGateProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8005FAA8
 * Range: 0x8005FAA8..0x8005FAE7 (inclusive)
 * Source size: 64 bytes / 16 instructions
 * Evidence: fresh Ghidra game_8005faa8.txt; instruction SHA-256 45299ab26bd749d0d547a673aaffb5dddf5af971fdab2d446a7a7be20c13bf82
 *
 * Purpose: Gate an actor-pair contact test by the signed wrapped difference between the actors' coordinate words.
 * Inputs: All 32 live GPRs, HI/LO, a0 first actor, a1 second actor, coordinate words at actor offset 8, retained sp/ra stack state, and typed child 0x8005F948.
 * Returns: v0 is zero when the raw signed difference is at least 4097 and one after any completed child; child mutations remain live except v0/ra/sp epilogue effects.
 * Guest memory: Saves ra at entry sp-8, reads second+8 then first+8, and reloads ra through child-mutable live sp.
 * Calls: 0x8005F948 at 0x8005FACC with three arguments and SRA a2,a2,8 in the delay slot.
 * Original quirks: The signed gate has no lower bound so every negative raw difference passes; the branch always clears v0 in its delay slot and a completed child's v0 is overwritten with one.
 * Native mapping: Guest addresses use validated uint32_t retained-memory regions with per-byte knownness, exact access prefixes, and a full mutable-machine callback; no guest value is cast to a host pointer.
 */
int nba97_game_actor_contact_gate(Nba97GameActorContactGateContext *,
                                  Nba97GameActorContactGateProgress *);

#ifdef __cplusplus
}
#endif
#endif
