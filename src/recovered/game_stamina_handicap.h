#ifndef NBA97_GAME_STAMINA_HANDICAP_H
#define NBA97_GAME_STAMINA_HANDICAP_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameStaminaHandicapWord;
typedef Nba97GameMatchClocksMachine Nba97GameStaminaHandicapMachine;

enum Nba97GameStaminaHandicapAccessKind {
  NBA97_GAME_STAMINA_HANDICAP_READ = 1,
  NBA97_GAME_STAMINA_HANDICAP_STORE = 2
};

typedef struct Nba97GameStaminaHandicapAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameStaminaHandicapAccess;

typedef struct Nba97GameStaminaHandicapContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameStaminaHandicapMachine machine;
  Nba97GameStaminaHandicapAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameStaminaHandicapContext;

typedef struct Nba97GameStaminaHandicapProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t score_iterations;
  size_t actor_iterations;
  size_t score_updates;
  size_t stamina_updates;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameStaminaHandicapWord return_address;
  Nba97GameStaminaHandicapMachine machine;
  uint8_t completed;
} Nba97GameStaminaHandicapProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80068504
 * Range: 0x80068504..0x800686B7 (inclusive)
 * Source size: 436 bytes / 109 instructions
 * Evidence: fresh Ghidra game_80068504.txt; instruction SHA-256
 * f0736b214b6a4d3bd08c1ff01790894a88b2cb710f5a81ff15d7cef5ac94fea2
 *
 * Purpose: Set the score handicap, adjust 24 signed score entries, and reduce
 * ten actors' linked stamina records from the live handicap delta.
 * Inputs: All 32 live MIPS GPRs and HI/LO; feature bytes 0x80021D81 and
 * 0x80021D93; clock 0x800FDB58; score halfwords 0x8001EE22/0x8001EEE6 and
 * 0x8001F80C at stride 0x22; phase 0x800FE8CC; signed delta 0x800FDB7E;
 * ten actor pointers at 0x80020BEC and their linked record fields.
 * Returns: The complete live machine with final v0 and all source register
 * effects; sp, ra, HI, and LO remain untouched before JR consumes live ra.
 * Guest memory: Stores 0xFFFF to 0x800FDB98 before the first branch; may
 * replace it with 0 or 5; conditionally updates 24 score halfwords; then
 * stores wrapped stamina before any zero clamp and always clears actor +0xDD.
 * Calls: None observed.
 * Original quirks: The first handicap store is a branch delay effect; phase
 * testing assigns a2=0x800FDB7E in its delay slot; signed negative score
 * increments can underflow; stamina stores precede signed-low-half clamps;
 * actor +0xDD is cleared even when that actor is otherwise inactive.
 * Native mapping: All guest addresses are validated uint32_t values with
 * per-byte knownness and observable access prefixes; no host-pointer casts or
 * inferred machine state are used.
 */
int nba97_game_stamina_handicap(Nba97GameStaminaHandicapContext *,
                                Nba97GameStaminaHandicapProgress *);

#ifdef __cplusplus
}
#endif
#endif
