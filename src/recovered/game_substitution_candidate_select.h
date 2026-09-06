#ifndef NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_H
#define NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_H

#include "game_team_strategy_apply.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameTeamStrategyApplyWord Nba97GameSubstitutionCandidateSelectWord;
typedef Nba97GameTeamStrategyApplyMachine Nba97GameSubstitutionCandidateSelectMachine;
typedef Nba97GameTeamStrategyApplyAccess Nba97GameSubstitutionCandidateSelectAccess;

enum Nba97GameSubstitutionCandidateSelectCallKind {
  NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_800649D8 = 1,
  NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_CALL_KIND_COUNT
};

typedef struct Nba97GameSubstitutionCandidateSelectEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameSubstitutionCandidateSelectEvent;

typedef int (*Nba97GameSubstitutionCandidateSelectIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameSubstitutionCandidateSelectEvent *,
    Nba97GameSubstitutionCandidateSelectMachine *);

typedef struct Nba97GameSubstitutionCandidateSelectContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameSubstitutionCandidateSelectMachine machine;
  Nba97GameSubstitutionCandidateSelectIo io;
  void *user;
  Nba97GameSubstitutionCandidateSelectAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameSubstitutionCandidateSelectContext;

typedef struct Nba97GameSubstitutionCandidateSelectProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameSubstitutionCandidateSelectWord restored_return_address;
  Nba97GameSubstitutionCandidateSelectWord return_v0;
  Nba97GameSubstitutionCandidateSelectMachine machine;
  uint8_t completed;
} Nba97GameSubstitutionCandidateSelectProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80064DBC
 * Range: 0x80064DBC..0x8006506F (inclusive)
 * Source size: 692 bytes / 173 instructions
 * Evidence: fresh Ghidra game_80064dbc.txt; instruction SHA-256
 * 38ae4643f2ff42c752636fec94c44376539e8fb575b2d697ff81500a7c8ad277
 *
 * Purpose: Select an eligible substitution candidate through five ordered
 * source scans and invoke the existing substitution owner on a hit.
 * Inputs: Full GPR/HI-LO machine; a0 team header, full a1 player identifier,
 * a2 status pointer, a3 fifth child argument, mapped tables, live sp/ra.
 * Returns: v0 is zero on no hit and one after an accepted child; callback
 * mutations survive except ra reloads through callback-live sp before sp+0x48.
 * Guest memory: Saves/restores ra, reads side/count/inverse-lineup/player/status
 * and signed rank tables in exact pass order, and stores incoming a3 at sp+0x10
 * in the sole JAL delay slot.
 * Calls: 0x800649D8 at 0x80065038.
 * Original quirks: Pass counts are latched or reread exactly as sourced; signed
 * LB ranks never equal unsigned player bytes above 127; fifth scan rereads the
 * count after every rejected candidate; the child return is forced to one.
 * Native mapping: Guest addresses remain validated uint32_t mappings with
 * per-byte knownness; 0x800649D8 remains a typed full-machine callback.
 */
int nba97_game_substitution_candidate_select(
    Nba97GameSubstitutionCandidateSelectContext *,
    Nba97GameSubstitutionCandidateSelectProgress *);

#ifdef __cplusplus
}
#endif
#endif
