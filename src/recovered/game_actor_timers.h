#ifndef NBA97_GAME_ACTOR_TIMERS_H
#define NBA97_GAME_ACTOR_TIMERS_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameActorTimersWord;
typedef Nba97GameMatchClocksMachine Nba97GameActorTimersMachine;

enum Nba97GameActorTimersAccessKind {
  NBA97_GAME_ACTOR_TIMERS_READ = 1,
  NBA97_GAME_ACTOR_TIMERS_STORE = 2
};

typedef struct Nba97GameActorTimersAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameActorTimersAccess;

typedef struct Nba97GameActorTimersMultiplyTrace {
  uint32_t pc;
  Nba97GameActorTimersWord multiplicand;
  Nba97GameActorTimersWord multiplier;
  Nba97GameActorTimersWord hi;
  Nba97GameActorTimersWord lo;
  Nba97GameActorTimersWord quotient;
} Nba97GameActorTimersMultiplyTrace;

typedef struct Nba97GameActorTimersContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameActorTimersMachine machine;
  Nba97GameActorTimersAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameActorTimersContext;

typedef struct Nba97GameActorTimersProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t multiply_count;
  size_t entity_iterations;
  size_t participation_iterations;
  size_t team_counter_updates;
  size_t participation_updates;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t frame_stack_pointer;
  Nba97GameActorTimersWord clock_quotient_60;
  Nba97GameActorTimersWord last_clock_quotient_3600;
  Nba97GameActorTimersWord return_address;
  Nba97GameActorTimersMultiplyTrace multiply[11];
  Nba97GameActorTimersMachine machine;
  uint8_t completed;
} Nba97GameActorTimersProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8006830C
 * Range: 0x8006830C..0x80068503 (inclusive)
 * Source size: 504 bytes / 126 instructions
 * Evidence: fresh Ghidra game_8006830c.txt; instruction SHA-256
 * 1b2b4229ce1bbb9b927722a1dac657b3cd98cad3a7fe53a0cab1042021344db2
 *
 * Purpose: Decrement eleven actors' live timers and update participation
 * counters from signed match-clock quotients by 60 and 3600.
 * Inputs: All 32 live MIPS GPRs, HI/LO, eleven actor pointers at
 * 0x80020BEC, ten team pointers at 0x800FDC70, controller pointers based at
 * 0x800FDC50, signed actor controller indices, delta 0x800FDB6C, clock
 * 0x800FDB58, and cached counters at 0x800FDB74 and controller fields.
 * Returns: The complete live machine after both signed MULT pipelines, with
 * sp restored by 0x18 and the original live ra consumed by JR.
 * Guest memory: Reads eleven actor pointers and their +0xE6/+0xE4/+0xB4
 * timers; clears the first ten +0xD8/+0xF2 fields; stores wrapped timer halves
 * before negative clamps and sets +0xDD after a nonzero +0xE4 timer; updates
 * 0x800FDB74 and ten live team +0x1A/+0x1C fields; then reads ten actor +4
 * indices and updates selected controller +0x22/+0x1E fields in source order.
 * Calls: None observed.
 * Original quirks: The eleventh actor receives only the +0xB4 timer update;
 * pointer and index loop increments execute in branch delay slots even on the
 * last iteration; team pointers are reread after +0x1A stores; clocks are
 * reread for every nonnegative actor; negative quotients never equal an
 * unsigned cached halfword and duplicate controller pointers can suppress
 * later increments after the first cache update.
 * Native mapping: Signed magic-number MULT/MFHI pipelines update explicit
 * full-machine HI/LO state; guest addresses remain validated uint32_t values
 * with per-byte knownness and no host-pointer casts.
 */
int nba97_game_actor_timers(Nba97GameActorTimersContext *,
                            Nba97GameActorTimersProgress *);

#ifdef __cplusplus
}
#endif
#endif
