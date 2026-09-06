#ifndef NBA97_GAME_BALL_ACTOR_CONTACT_H
#define NBA97_GAME_BALL_ACTOR_CONTACT_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameBallActorContactWord;
typedef Nba97GameMatchClocksMachine Nba97GameBallActorContactMachine;
typedef Nba97GameMatchClocksAccess Nba97GameBallActorContactAccess;

enum Nba97GameBallActorContactCallKind {
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8007066C = 1,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_800601B8,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_80060240,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_80060008,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8002AB70,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_800581C0,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_80058120,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_80029258,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_800295C8,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_80029590,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8007059C,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8005D140,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_80058260,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8005BC34,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_800582DC,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8006E7AC,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8006229C,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_80062660,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_80035318,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8005699C,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_800A5638,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_800AA788,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_800A5634,
  NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8005828C,
  NBA97_GAME_BALL_ACTOR_CONTACT_CALL_KIND_COUNT
};

typedef struct Nba97GameBallActorContactEvent {
  uint32_t pc, delay_slot_pc, entry;
  size_t operation, invocation;
  uint8_t kind, argument_count;
} Nba97GameBallActorContactEvent;

/* Called after JAL has published ra and its delay instruction has executed.
 * A child may change every GPR, HI/LO, and any mapped retained byte. */
typedef int (*Nba97GameBallActorContactIo)(
    void *, const Nba97GameTextMemory *, const Nba97GameBallActorContactEvent *,
    Nba97GameBallActorContactMachine *);

typedef struct Nba97GameBallActorContactContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameBallActorContactMachine machine;
  Nba97GameBallActorContactIo io;
  void *user;
  Nba97GameBallActorContactAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameBallActorContactContext;

typedef struct Nba97GameBallActorContactProgress {
  size_t operations, accesses, reads, stores, callbacks_completed,
      access_events;
  size_t call_count[NBA97_GAME_BALL_ACTOR_CONTACT_CALL_KIND_COUNT];
  uint32_t stopped_pc, stopped_address, stopped_entry, frame_stack_pointer;
  Nba97GameBallActorContactWord restored_return_address;
  Nba97GameBallActorContactMachine machine;
  uint32_t instruction_count;
  uint8_t completed;
} Nba97GameBallActorContactProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800602CC
 * Range: 0x800602CC..0x80060E8B (inclusive)
 * Source size: 3008 bytes / 752 instructions
 * Evidence: fresh Ghidra game_800602cc.txt; instruction SHA-256
 * df9074d4240d6e16e099c0d3c5d2a45941355872521ad92d25123f29b30b7ac7
 *
 * Purpose: Complete ball/actor eligibility, contact, acquisition, phase-81
 * transition, and negative-deflection behavior.
 * Inputs: All 32 GPRs and HI/LO; a0 ball, a1 actor, a2 distance; mapped stack,
 * match globals, and actor/team/controller records.
 * Returns: Source-live values with ra and s5..s0 restored through live sp, sp
 * advanced by 0x40, and JR using live ra.
 * Guest memory: The 0x40 frame and all contact, possession, statistics,
 * velocity, phase, ownership, actor, team, and controller fields.
 * Calls: 0x8007066C, 0x800601B8, 0x80060240, 0x80060008, 0x8002AB70,
 * 0x800581C0, 0x80058120, 0x80029258, 0x800295C8, 0x80029590,
 * 0x8007059C, 0x8005D140, 0x80058260, 0x8005BC34, 0x800582DC,
 * 0x8006E7AC, 0x8006229C, 0x80062660, 0x80035318, 0x8005699C,
 * 0x800A5638, 0x800AA788, 0x800A5634, and 0x8005828C in source order
 * across 51 call sites; every callee remains a typed boundary.
 * Original quirks: Wrapped coordinate arithmetic, signed-low-half contact
 * results, repeated B4 stores, mixed capped and wrapping statistics, and stores
 * in JAL delay slots.
 * Native mapping: Guest addresses remain uint32_t mapped values; full mutable
 * machine callbacks and per-byte knownness preserve aliases and exact prefixes.
 */
int nba97_game_ball_actor_contact(Nba97GameBallActorContactContext *,
                                  Nba97GameBallActorContactProgress *);

#ifdef __cplusplus
}
#endif
#endif
