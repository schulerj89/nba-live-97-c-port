#ifndef NBA97_GAME_CAMERA_OVERRIDE_END_H
#define NBA97_GAME_CAMERA_OVERRIDE_END_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameCameraOverrideEndWord;
typedef Nba97GameMatchClocksMachine Nba97GameCameraOverrideEndMachine;
typedef Nba97GameMatchClocksAccess Nba97GameCameraOverrideEndAccess;

enum Nba97GameCameraOverrideEndCallKind {
  NBA97_GAME_CAMERA_OVERRIDE_END_CHILD_8007A114 = 1,
  NBA97_GAME_CAMERA_OVERRIDE_END_CALL_KIND_COUNT
};

typedef struct Nba97GameCameraOverrideEndEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameCameraOverrideEndEvent;

/* The callback observes JAL's ra and delay-slot a0=0. It may mutate every
 * GPR, HI/LO, mapped retained memory, sp, and the saved return-address word. */
typedef int (*Nba97GameCameraOverrideEndIo)(void *,
    const Nba97GameTextMemory *, const Nba97GameCameraOverrideEndEvent *,
    Nba97GameCameraOverrideEndMachine *);

typedef struct Nba97GameCameraOverrideEndContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCameraOverrideEndMachine machine;
  Nba97GameCameraOverrideEndIo io;
  void *user;
  Nba97GameCameraOverrideEndAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameCameraOverrideEndContext;

typedef struct Nba97GameCameraOverrideEndProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_CAMERA_OVERRIDE_END_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameCameraOverrideEndWord flag;
  Nba97GameCameraOverrideEndWord saved_return_address;
  Nba97GameCameraOverrideEndWord restored_return_address;
  Nba97GameCameraOverrideEndWord returned_value;
  Nba97GameCameraOverrideEndMachine machine;
  uint8_t completed;
} Nba97GameCameraOverrideEndProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8007A36C
 * Range: 0x8007A36C..0x8007A39F (inclusive)
 * Source size: 52 bytes / 13 instructions
 * Evidence: fresh Ghidra game_8007a36c.txt; instruction SHA-256 c0c28c611a2e440826289a731844389556bef691a7048f82d5409a147cf90a91
 *
 * Purpose: End an active camera override by dispatching the restore service and clearing its retained flag.
 * Inputs: All 32 live GPRs, HI/LO, byte flag 0x800BC1F0, retained sp/ra stack state, and typed child 0x8007A114.
 * Returns: Raw loaded or child-mutated v0 and source-live register state, with ra restored through live sp and sp advanced by 0x18.
 * Guest memory: Reads flag 0x800BC1F0 before frame allocation, saves ra at frame+0x10 in the branch delay, conditionally clears the flag, then reloads ra from live sp+0x10.
 * Calls: 0x8007A114 at 0x8007A380 with one argument; JAL publishes ra=0x8007A388 before delay-slot a0=0.
 * Original quirks: The branch saves ra even for a zero or unknown flag; child refusal leaves the flag uncleared; the active path overwrites at with 0x800C0000 but never normalizes v0.
 * Native mapping: Guest addresses use validated uint32_t retained-memory regions with little-endian access, per-byte knownness, exact failure prefixes, and a full mutable-machine callback; no host-pointer cast is used.
 */
int nba97_game_camera_override_end(Nba97GameCameraOverrideEndContext *,
                                   Nba97GameCameraOverrideEndProgress *);

#ifdef __cplusplus
}
#endif
#endif
