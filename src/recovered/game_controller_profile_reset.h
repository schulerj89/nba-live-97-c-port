#ifndef NBA97_GAME_CONTROLLER_PROFILE_RESET_H
#define NBA97_GAME_CONTROLLER_PROFILE_RESET_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameControllerProfileResetWord;
typedef Nba97GameMatchClocksMachine Nba97GameControllerProfileResetMachine;
typedef Nba97GameMatchClocksAccess Nba97GameControllerProfileResetAccess;

enum Nba97GameControllerProfileResetCallKind {
  NBA97_GAME_CONTROLLER_PROFILE_RESET_ZERO_800A3A74 = 1,
  NBA97_GAME_CONTROLLER_PROFILE_RESET_CALL_KIND_COUNT
};

typedef struct Nba97GameControllerProfileResetEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameControllerProfileResetEvent;

/* The callback sees JAL ra and its ORI delay result. It may mutate all GPRs,
 * HI/LO, retained memory, live SP, or saved frame words before returning. */
typedef int (*Nba97GameControllerProfileResetIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameControllerProfileResetEvent *,
    Nba97GameControllerProfileResetMachine *);

typedef struct Nba97GameControllerProfileResetContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameControllerProfileResetMachine machine;
  Nba97GameControllerProfileResetIo io;
  void *user;
  Nba97GameControllerProfileResetAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameControllerProfileResetContext;

typedef struct Nba97GameControllerProfileResetProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts;
  size_t call_count;
  size_t records_started;
  size_t records_copied;
  size_t bytes_copied;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameControllerProfileResetWord restored_return_address;
  Nba97GameControllerProfileResetWord restored_s4;
  Nba97GameControllerProfileResetWord restored_s3;
  Nba97GameControllerProfileResetWord restored_s2;
  Nba97GameControllerProfileResetWord restored_s1;
  Nba97GameControllerProfileResetWord restored_s0;
  Nba97GameControllerProfileResetMachine machine;
  uint8_t completed;
} Nba97GameControllerProfileResetProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80083490
 * Range: 0x80083490..0x800835C3 (inclusive)
 * Source size: 308 bytes / 77 instructions
 * Evidence: fresh Ghidra game_80083490.txt; instruction SHA-256
 * 8ef146f2e6f42e6a8790aadee0043a6dd9c736133f98a635b8dba5d9c9c76c8d
 *
 * Purpose: Clear eight controller records and copy the selected 59-byte input
 * profile tail into each eligible record.
 * Inputs: Full live GPR/HI/LO state; a0 selects the forced-default profile;
 * live s0-s4, sp and saved frame words; controller selection/profile memory.
 * Returns: v0 holds the final signed-loop predicate; ra/s4..s0 are restored
 * through live sp, sp advances by 0x28, and callback mutations remain live.
 * Guest memory: Saves/restores six frame words; reads 0x80021DDE+index,
 * profile flag/data under 0x80020C1C or default 0x800BC94C; clears record
 * headers through the typed child and copies exactly 59 bytes to record+0x3C.
 * Calls: 0x800A3A74 at 0x800834D8, with 0x800834DC setting a1=0x24.
 * Original quirks: Selection is read even in override mode; a negative signed
 * selection skips only the tail copy; the byte loop is forward and alias-live;
 * callback-mutated s-registers and sp can change later addresses and loop count.
 * Native mapping: Guest addresses remain uint32_t values validated
 * against mapped regions with per-byte knownness; the zero child is a
 * full-machine callback and its existing owner is composed by the native
 * adapter.
 */
int nba97_game_controller_profile_reset(
    Nba97GameControllerProfileResetContext *,
    Nba97GameControllerProfileResetProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
