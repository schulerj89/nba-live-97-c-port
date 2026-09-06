#ifndef NBA97_GAME_TEAM_STRATEGY_APPLY_H
#define NBA97_GAME_TEAM_STRATEGY_APPLY_H

#include "game_match_state_reset.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchStateResetWord Nba97GameTeamStrategyApplyWord;
typedef Nba97GameMatchStateResetMachine Nba97GameTeamStrategyApplyMachine;
typedef Nba97GameMatchStateResetAccess Nba97GameTeamStrategyApplyAccess;

enum Nba97GameTeamStrategyApplyCallKind {
  NBA97_GAME_TEAM_STRATEGY_APPLY_800646A8 = 1,
  NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC,
  NBA97_GAME_TEAM_STRATEGY_APPLY_CALL_KIND_COUNT
};

typedef struct Nba97GameTeamStrategyApplyEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameTeamStrategyApplyEvent;

typedef int (*Nba97GameTeamStrategyApplyIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameTeamStrategyApplyEvent *,
    Nba97GameTeamStrategyApplyMachine *);

typedef struct Nba97GameTeamStrategyApplyContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameTeamStrategyApplyMachine machine;
  Nba97GameTeamStrategyApplyIo io;
  void *user;
  Nba97GameTeamStrategyApplyAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameTeamStrategyApplyContext;

typedef struct Nba97GameTeamStrategyApplyProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_TEAM_STRATEGY_APPLY_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_TEAM_STRATEGY_APPLY_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameTeamStrategyApplyWord restored_return_address;
  Nba97GameTeamStrategyApplyWord restored_s0;
  Nba97GameTeamStrategyApplyWord return_v0;
  Nba97GameTeamStrategyApplyMachine machine;
  uint8_t completed;
} Nba97GameTeamStrategyApplyProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80065820
 * Range: 0x80065820..0x800659EF (inclusive)
 * Source size: 464 bytes / 116 instructions
 * Evidence: fresh Ghidra game_80065820.txt; instruction SHA-256
 * a8eb544429cee266dd170710e0a2684889dee2404446c9ba6ff6462c727739ad
 *
 * Purpose: Apply side-specific strategy fields and the selected injury or
 * lineup substitution to one live team structure.
 * Inputs: Full live GPR/HI-LO machine with the team pointer in a0, mapped
 * strategy/team/lineup memory, live sp/ra, and two typed child services.
 * Returns: Live callback state with ra and s0 reloaded through live sp, sp
 * advanced by 0x18, v0 retaining the final source value, and HI/LO preserved
 * except for child mutations.
 * Guest memory: Saves/restores s0 and ra, reads mode/side/control/strategy and
 * injury data, writes seven strategy bytes or CPU defaults, optionally swaps
 * lineup halfwords, decrements the live team count, and preserves exact order.
 * Calls: 0x800646A8 at 0x80065998; 0x80064DBC at 0x800659C4.
 * Original quirks: Mode writes only byte 0x76; injury values at least 12 skip
 * both children and the count decrement; lineup scan keeps source a0/a1;
 * count zero wraps to 0xFFFF; all J/branch delay effects remain observable.
 * Native mapping: Guest pointers remain validated uint32_t mapped addresses
 * with per-byte knownness; both unresolved callees are typed full-machine
 * callbacks and the older no-injury projection remains separate.
 */
int nba97_game_team_strategy_apply(
    Nba97GameTeamStrategyApplyContext *,
    Nba97GameTeamStrategyApplyProgress *);

#ifdef __cplusplus
}
#endif
#endif
