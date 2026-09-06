#ifndef NBA97_GAME_TEAM_TACTICS_UPDATE_H
#define NBA97_GAME_TEAM_TACTICS_UPDATE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameTeamTacticsWord;
typedef Nba97GameMatchClocksMachine Nba97GameTeamTacticsMachine;

enum Nba97GameTeamTacticsCallKind {
  NBA97_GAME_TEAM_TACTICS_CHILD_8002AB70 = 1,
  NBA97_GAME_TEAM_TACTICS_CHILD_800706E4,
  NBA97_GAME_TEAM_TACTICS_CHILD_8007066C,
  NBA97_GAME_TEAM_TACTICS_CHILD_800295C0,
  NBA97_GAME_TEAM_TACTICS_CHILD_80073134,
  NBA97_GAME_TEAM_TACTICS_CHILD_80072C40,
  NBA97_GAME_TEAM_TACTICS_CHILD_8007308C,
  NBA97_GAME_TEAM_TACTICS_CHILD_80072AB0,
  NBA97_GAME_TEAM_TACTICS_CHILD_80072B70,
  NBA97_GAME_TEAM_TACTICS_CHILD_80073054,
  NBA97_GAME_TEAM_TACTICS_CHILD_800742C0,
  NBA97_GAME_TEAM_TACTICS_CHILD_8007458C,
  NBA97_GAME_TEAM_TACTICS_CHILD_80074374,
  NBA97_GAME_TEAM_TACTICS_CHILD_800743C8,
  NBA97_GAME_TEAM_TACTICS_CHILD_80074488,
  NBA97_GAME_TEAM_TACTICS_CHILD_80074688,
  NBA97_GAME_TEAM_TACTICS_CHILD_80074714,
  NBA97_GAME_TEAM_TACTICS_CALL_KIND_COUNT
};

enum Nba97GameTeamTacticsAccessKind {
  NBA97_GAME_TEAM_TACTICS_READ = 1,
  NBA97_GAME_TEAM_TACTICS_STORE = 2
};

typedef struct Nba97GameTeamTacticsEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameTeamTacticsEvent;

typedef int (*Nba97GameTeamTacticsIo)(void *, const Nba97GameTextMemory *,
                                      const Nba97GameTeamTacticsEvent *,
                                      Nba97GameTeamTacticsMachine *);

typedef struct Nba97GameTeamTacticsAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameTeamTacticsAccess;

typedef struct Nba97GameTeamTacticsContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameTeamTacticsMachine machine;
  Nba97GameTeamTacticsIo io;
  void *user;
  Nba97GameTeamTacticsAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameTeamTacticsContext;

typedef struct Nba97GameTeamTacticsProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t call_count[NBA97_GAME_TEAM_TACTICS_CALL_KIND_COUNT];
  size_t actor_iterations;
  size_t opposing_actor_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameTeamTacticsWord restored_return_address;
  Nba97GameTeamTacticsWord restored_fp;
  Nba97GameTeamTacticsWord restored_saved[8];
  Nba97GameTeamTacticsMachine machine;
  uint8_t completed;
} Nba97GameTeamTacticsProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800747B0
 * Range: 0x800747B0..0x80075D3F (inclusive)
 * Source size: 5520 bytes / 1380 instructions
 * Evidence: fresh Ghidra game_800747b0.txt; instruction SHA-256
 * 1c26b7364cec973a77600152182b1a5ada6c547fad563c80b29fb4484194a1ac
 *
 * Purpose: Update the active teams' tactical play state, actor geometry,
 * possession choices, defensive marking, and retained tactical timers.
 * Inputs: All 32 live GPRs, HI/LO, mutable stack frame, match/team/actor
 * globals, runtime play tables, and typed child results described by events.
 * Returns: The complete callback-live machine after fresh sp-relative reloads
 * of ra/fp/s7..s0, sp+0x98, and JR of the restored ra.
 * Guest memory: Reads and writes live match globals, two team records, ten
 * actor records, linked state, runtime play tables, and the 0x98-byte frame in
 * exact source order through validated mapped uint32_t addresses.
 * Calls: 0x8002AB70@0x800748EC; 0x800706E4@0x800749CC,
 * 0x800749F0; 0x8007066C@0x80074AE8; 0x800706E4@0x80074BEC,
 * 0x80074C1C,0x80074C44; 0x800295C0@0x80074D30;
 * 0x8002AB70@0x80074EE8,0x80074F04,0x80074F18,0x80074F40;
 * 0x80073134@0x80074FB0; 0x8002AB70@0x80074FD8;
 * 0x80072C40@0x80075000; 0x8007308C@0x800750A8;
 * 0x80072AB0@0x800750C8; 0x8002AB70@0x8007517C,
 * 0x800751A4,0x800751B8,0x800751D0,0x800751E4,0x80075208,
 * 0x80075250,0x80075264,0x800752DC,0x80075354,0x80075378,
 * 0x80075394; 0x80073134@0x800753CC; 0x8007308C@0x800753EC;
 * 0x80072B70@0x8007541C,0x80075458; 0x80073054@0x800755A8;
 * 0x800742C0@0x800757F0,0x80075820; 0x8007458C@0x800759B8;
 * 0x80074374@0x800759F8; 0x800743C8@0x80075A0C;
 * 0x8007458C@0x80075A68; 0x80074488@0x80075A80;
 * 0x8002AB70@0x80075BD0,0x80075C08; 0x80074688@0x80075C28;
 * 0x80074714@0x80075C44; 0x8007458C@0x80075CE0;
 * 0x800743C8@0x80075CF0.
 * Original quirks: Candidate minima replace on ties; empty candidate scans
 * consume retained uninitialized frame words; random rejection and marker
 * loops remain bounded only by the host operation budget; timer arithmetic
 * wraps,
 * signed byte indices, delay-slot stores, and callback mutations are retained.
 * Native mapping: Original calls are full-machine typed boundaries. Existing
 * narrow scalar owners are not promoted to an unproved CPU ABI; guest
 * addresses remain mapped uint32_t values with per-byte knownness.
 */
int nba97_game_team_tactics_update(Nba97GameTeamTacticsContext *,
                                   Nba97GameTeamTacticsProgress *);

#ifdef __cplusplus
}
#endif
#endif
