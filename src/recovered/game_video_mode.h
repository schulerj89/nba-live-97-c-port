#ifndef NBA97_GAME_VIDEO_MODE_H
#define NBA97_GAME_VIDEO_MODE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameVideoModeWord;
typedef Nba97GameMatchClocksMachine Nba97GameVideoModeMachine;
typedef Nba97GameMatchClocksAccess Nba97GameVideoModeAccess;

typedef struct Nba97GameVideoModeContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameVideoModeMachine machine;
  Nba97GameVideoModeAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameVideoModeContext;

typedef struct Nba97GameVideoModeProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameVideoModeWord return_v0;
  Nba97GameVideoModeMachine machine;
  uint8_t completed;
} Nba97GameVideoModeProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800985CC
 * Range: 0x800985CC..0x800985DB (inclusive)
 * Source size: 16 bytes / 4 instructions
 * Evidence: fresh Ghidra game_800985cc.txt; instruction SHA-256
 * 6573858a77338ffd6166fbc44753517ce124fb50c76eeae7fc9e1d7540013d32
 *
 * Purpose: Return the raw retained video-mode word at 0x800C54AC.
 * Inputs: Full 32-GPR/HI-LO machine, mapped guest memory at 0x800C54AC, and
 * live ra for the final JR.
 * Returns: Replaces v0 with the raw word and its byte knownness; preserves all
 * other GPRs and HI/LO and consumes live ra only after the read.
 * Guest memory: Reads four little-endian bytes at 0x800C54AC exactly once and
 * performs no writes.
 * Calls: None observed.
 * Original quirks: The value is not normalized to a Boolean or enum; partial
 * data may return, and the JR NOP executes before unknown-ra refusal.
 * Native mapping: The fixed PS1 address uses validated uint32_t retained
 * regions and byte knownness without a guest-to-host pointer cast.
 */
int nba97_game_video_mode(Nba97GameVideoModeContext *,
                          Nba97GameVideoModeProgress *);

#ifdef __cplusplus
}
#endif
#endif
