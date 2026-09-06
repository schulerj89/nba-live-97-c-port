#ifndef NBA97_GAME_CAMERA_STATE_LOOKUP_H
#define NBA97_GAME_CAMERA_STATE_LOOKUP_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameCameraStateLookupWord;
typedef Nba97GameMatchClocksMachine Nba97GameCameraStateLookupMachine;
typedef Nba97GameMatchClocksAccess Nba97GameCameraStateLookupAccess;

typedef struct Nba97GameCameraStateLookupContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCameraStateLookupMachine machine;
  Nba97GameCameraStateLookupAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameCameraStateLookupContext;

typedef struct Nba97GameCameraStateLookupProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t instruction_count;
  Nba97GameCameraStateLookupWord source_value;
  Nba97GameCameraStateLookupWord masked_value;
  Nba97GameCameraStateLookupWord signed_index;
  Nba97GameCameraStateLookupWord lookup_address;
  Nba97GameCameraStateLookupWord returned_value;
  Nba97GameCameraStateLookupMachine machine;
  uint8_t negative_table;
  uint8_t completed;
} Nba97GameCameraStateLookupProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8007A410
 * Range: 0x8007A410..0x8007A467 (inclusive)
 * Source size: 88 bytes / 22 instructions
 * Evidence: fresh Ghidra game_8007a410.txt; instruction SHA-256
 * 7b65160301ecc0720aa9d4f29a7c22f014a5eaea0623be65217514bccd262ee6
 *
 * Purpose: Derive a signed camera-state index and load its raw table word.
 * Inputs: Full live GPR/HI-LO state and the camera source word at 0x800FC9AC.
 * Returns: Raw selected table word and knownness in v0; all other live machine
 * state is source-faithful and ra remains the caller-provided return target.
 * Guest memory: Reads 0x800FC9AC, then one unchecked computed word from the
 * positive table based at 0x800BC204 or negative table based at 0x800BC224.
 * Calls: None observed.
 * Original quirks: Source bits 28..31 and 0..7 are discarded, the negative
 * path adds 0x8000 with 32-bit wrap, and table indices are unchecked.
 * Native mapping: Computed uint32_t guest addresses use validated retained
 * regions and per-byte knownness; no guest integer is a host pointer.
 */
int nba97_game_camera_state_lookup(Nba97GameCameraStateLookupContext *,
                                   Nba97GameCameraStateLookupProgress *);

#ifdef __cplusplus
}
#endif
#endif
