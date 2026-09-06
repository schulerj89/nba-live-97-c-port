#ifndef NBA97_GAME_DRAW_MODE_COMMAND_H
#define NBA97_GAME_DRAW_MODE_COMMAND_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameDrawModeCommandWord;
typedef Nba97GameMatchClocksMachine Nba97GameDrawModeCommandMachine;
typedef Nba97GameMatchClocksAccess Nba97GameDrawModeCommandAccess;

typedef struct Nba97GameDrawModeCommandContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameDrawModeCommandMachine machine;
  Nba97GameDrawModeCommandAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameDrawModeCommandContext;

typedef struct Nba97GameDrawModeCommandProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameDrawModeCommandWord return_v0;
  Nba97GameDrawModeCommandMachine machine;
  uint8_t completed;
} Nba97GameDrawModeCommandProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009A5E8
 * Range: 0x8009A5E8..0x8009A643 (inclusive)
 * Source size: 92 bytes / 23 instructions
 * Evidence: fresh Ghidra game_8009a5e8.txt; instruction SHA-256
 * 0b3c69b40bf5a16d3c424e16bb05db4eef1927a92e78d6d7764f6686a384cf20
 *
 * Purpose: Pack the environment type and raw draw-mode arguments into a PS1
 * GPU E1 command word.
 * Inputs: Full 32-GPR/HI-LO machine, Boolean-style full-word a0/a1, raw mode
 * bits in a2, display type byte at 0x800C55C0, and live ra.
 * Returns: v0 is the packed E1 command, v1 is its E1000000 base plus the
 * selected a1 bit, a0/a1/a2 remain unchanged, and all other state is preserved.
 * Guest memory: Reads one byte from 0x800C55C0 and performs no stores.
 * Calls: None observed.
 * Original quirks: Only types 1 and 2 use mask 0x27FF and flags 0x800/0x1000;
 * every other byte uses mask 0x09FF and flags 0x200/0x400; a0/a1 test their
 * full words; branch-delay ANDI/LUI and final JR-delay OR remain observable.
 * Native mapping: The fixed guest address uses validated uint32_t mapped
 * memory with per-byte knownness and no host-pointer cast.
 */
int nba97_game_draw_mode_command(Nba97GameDrawModeCommandContext *,
                                  Nba97GameDrawModeCommandProgress *);

#ifdef __cplusplus
}
#endif
#endif
