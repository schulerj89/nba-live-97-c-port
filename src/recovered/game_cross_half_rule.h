#ifndef NBA97_GAME_CROSS_HALF_RULE_H
#define NBA97_GAME_CROSS_HALF_RULE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameCrossHalfRuleWord;
typedef Nba97GameMatchClocksMachine Nba97GameCrossHalfRuleMachine;

enum Nba97GameCrossHalfRuleCallKind {
  NBA97_GAME_CROSS_HALF_RULE_CHILD_80062D84 = 1,
  NBA97_GAME_CROSS_HALF_RULE_CHILD_80029590,
  NBA97_GAME_CROSS_HALF_RULE_CHILD_800295C8,
  NBA97_GAME_CROSS_HALF_RULE_CHILD_80062300,
  NBA97_GAME_CROSS_HALF_RULE_CHILD_80062660,
  NBA97_GAME_CROSS_HALF_RULE_CALL_KIND_COUNT
};

typedef struct Nba97GameCrossHalfRuleEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameCrossHalfRuleEvent;

/* The callback observes JAL's ra and the completed delay slot. It may mutate
 * every GPR, HI/LO, retained memory, and the saved frame. Return exactly 1
 * only after the original child boundary returns synchronously. */
typedef int (*Nba97GameCrossHalfRuleIo)(void *, const Nba97GameTextMemory *,
                                        const Nba97GameCrossHalfRuleEvent *,
                                        Nba97GameCrossHalfRuleMachine *);

enum Nba97GameCrossHalfRuleAccessKind {
  NBA97_GAME_CROSS_HALF_RULE_READ = 1,
  NBA97_GAME_CROSS_HALF_RULE_STORE = 2
};

typedef struct Nba97GameCrossHalfRuleAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameCrossHalfRuleAccess;

typedef struct Nba97GameCrossHalfRuleContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCrossHalfRuleMachine machine;
  Nba97GameCrossHalfRuleIo io;
  void *user;
  Nba97GameCrossHalfRuleAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameCrossHalfRuleContext;

typedef struct Nba97GameCrossHalfRuleProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_CROSS_HALF_RULE_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameCrossHalfRuleWord restored_return_address;
  Nba97GameCrossHalfRuleMachine machine;
  uint8_t armed;
  uint8_t timer_accumulated;
  uint8_t rule_dispatched;
  uint8_t blocker_cleared;
  uint8_t completed;
} Nba97GameCrossHalfRuleProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8006817C
 * Range: 0x8006817C..0x8006830B (inclusive)
 * Source size: 400 bytes / 100 instructions
 * Evidence: fresh Ghidra game_8006817c.txt; instruction SHA-256
 * 608e555ead6150b53d94c8dcafe77e30cde1ca3d94242c7f39270875e745b19a
 *
 * Purpose: Track a sign-crossing relation between live actor fields, arm and
 * accumulate its timer, then dispatch the enabled rule-effect sequence.
 * Inputs: All 32 live MIPS GPRs, HI/LO, retained stack, phase/owner/team/delta
 * globals, two live actor pointers and fields, enable/blocker state, and five
 * typed child boundaries.
 * Returns: The final live GPR and HI/LO state, with ra reloaded through
 * callback-live sp, sp advanced by 0x18, and restored ra consumed by JR.
 * Guest memory: Reads 0x800FDB90, 0x800FE8CC, 0x800FDBCC,
 * 0x800FDC38/0x800FDC34 and their live fields, 0x80021D8B,
 * 0x800FE8E0, 0x800FDBAC/0x800FDB6C, and 0x800FDB94; writes the
 * saved ra, ordered arm values 1/0/0x7FFF, the wrapped timer, rule code 8 at
 * 0x800FE882, optional blocker clears, then reloads ra through live sp.
 * Calls: In source order: 0x80062D84@0x80068290;
 * 0x80029590@0x800682B4; 0x80029590@0x800682C4;
 * 0x800295C8@0x800682D0; 0x80062300@0x800682D8; and
 * 0x80062660@0x800682E0.
 * Original quirks: A negative owner exits without clearing the blocker;
 * nonnegative crossing arms through three ordered stores; the wrapped low
 * half timer is stored before signed comparison; child-mutated a0/sp/HI/LO
 * and saved-ra aliases remain live; announcement paths leave a0 as 5000 or
 * 20000 before the source no-op delay call.
 * Native mapping: Guest addresses remain uint32_t values resolved through
 * validated retained regions; full mutable-machine callbacks and per-byte
 * knownness preserve aliases and failure prefixes without host-pointer casts.
 */
int nba97_game_cross_half_rule(Nba97GameCrossHalfRuleContext *,
                               Nba97GameCrossHalfRuleProgress *);

#ifdef __cplusplus
}
#endif
#endif
