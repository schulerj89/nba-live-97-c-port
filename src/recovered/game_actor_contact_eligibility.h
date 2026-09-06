#ifndef NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_H
#define NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameActorContactEligibilityWord;
typedef Nba97GameMatchClocksMachine Nba97GameActorContactEligibilityMachine;
typedef Nba97GameMatchClocksAccess Nba97GameActorContactEligibilityAccess;

enum Nba97GameActorContactEligibilityCallKind {
  NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_GEOMETRY_8007066C = 1,
  NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_OTHER_TEAM_8005F888,
  NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_SAME_TEAM_8005F328,
  NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_CALL_KIND_COUNT
};

typedef struct Nba97GameActorContactEligibilityEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameActorContactEligibilityEvent;

typedef int (*Nba97GameActorContactEligibilityIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameActorContactEligibilityEvent *,
    Nba97GameActorContactEligibilityMachine *);

typedef struct Nba97GameActorContactEligibilityContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameActorContactEligibilityMachine machine;
  Nba97GameActorContactEligibilityIo io;
  void *user;
  Nba97GameActorContactEligibilityAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameActorContactEligibilityContext;

typedef struct Nba97GameActorContactEligibilityProgress {
  size_t operations, accesses, reads, stores, callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_CALL_KIND_COUNT];
  uint32_t stopped_pc, stopped_address, stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameActorContactEligibilityWord restored_return_address;
  Nba97GameActorContactEligibilityWord restored_s1;
  Nba97GameActorContactEligibilityWord restored_s0;
  Nba97GameActorContactEligibilityMachine machine;
  uint8_t completed;
} Nba97GameActorContactEligibilityProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8005F948
 * Range: 0x8005F948..0x8005FAA7 (inclusive)
 * Source size: 352 bytes / 88 instructions
 * Evidence: fresh Ghidra game_8005f948.txt; instruction SHA-256
 * ffc105ab1358cf77d142007dc72e5428807fcee9de4e6f592dfde243bc378402
 *
 * Purpose: Apply mode, identity, team, coordinate, and geometry gates before
 * dispatching an actor-pair contact action.
 * Inputs: Full 32-GPR/HI-LO machine;
 * a0/a1 actor guest addresses; a2 normalized X delta; retained stack;
 * mode/phase/owner globals; actor ID, coordinate, state, and team fields.
 * Returns: Rejection v0=0 or the admitted action child's low return byte; live
 * callback mutations persist except ra/s1/s0 restored through mutable sp and sp
 * advanced by 0x20.
 * Guest memory: Saves s1/s0/ra; reads 0x800FE8CC then
 * 0x800FE8CA, optional phase/owner gates, actor fields at +0/+0x0C/+0x1A/+0xD9,
 * then reloads the live frame in source order.
 * Calls: 0x8007066C at 0x8005FA18;
 * 0x8005F888 at 0x8005FA2C; 0x8007066C at 0x8005FA70; 0x8005F328 at 0x8005FA84.
 * Original quirks: Signed-half owner/exclusion values compare against full
 * actor IDs; the phase-82 negative-owner state gate is asymmetric; same-team X
 * accepts every negative value because it checks only signed a0<9; action
 * returns are truncated to one byte.
 * Native mapping: All guest addresses remain
 * validated uint32_t mappings with per-byte knownness, exact access/call
 * prefixes, mutable full-machine callbacks, and no host-pointer casts.
 */
int nba97_game_actor_contact_eligibility(
    Nba97GameActorContactEligibilityContext *,
    Nba97GameActorContactEligibilityProgress *);

#ifdef __cplusplus
}
#endif
#endif
