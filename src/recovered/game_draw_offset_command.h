#ifndef NBA97_GAME_DRAW_OFFSET_COMMAND_H
#define NBA97_GAME_DRAW_OFFSET_COMMAND_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameDrawOffsetCommandWord;
typedef Nba97GameMatchClocksMachine Nba97GameDrawOffsetCommandMachine;
typedef Nba97GameMatchClocksAccess Nba97GameDrawOffsetCommandAccess;

typedef struct Nba97GameDrawOffsetCommandContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameDrawOffsetCommandMachine machine;
  Nba97GameDrawOffsetCommandAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameDrawOffsetCommandContext;

typedef struct Nba97GameDrawOffsetCommandProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameDrawOffsetCommandWord return_v0;
  Nba97GameDrawOffsetCommandMachine machine;
  uint8_t completed;
} Nba97GameDrawOffsetCommandProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009A7DC
 * Range: 0x8009A7DC..0x8009A823 (inclusive)
 * Source size: 72 bytes / 18 instructions
 * Evidence: fresh Ghidra game_8009a7dc.txt; instruction SHA-256
 * f1cfbd67f0585f8b4ff68a86906883752ae4cf73940cfe19b4df8b7116ce2ce7
 *
 * Purpose: Pack raw draw offsets into the PS1 GPU E5 command word.
 *
 * Inputs: Full 32-GPR/HI-LO machine, raw x/y in a0/a1, display type byte at
 * 0x800C55C0, and live ra.
 *
 * Returns: v0 is the E5 command, v1 is shifted packed y, a0 is 0xE5000000,
 * a1 is unchanged, and all other GPRs plus HI/LO are preserved.
 *
 * Guest memory: Reads one byte from 0x800C55C0 and performs no writes.
 *
 * Calls: None observed.
 *
 * Original quirks: Types one and two use 12-bit fields; every other byte uses
 * 11-bit fields; the unknown type branch retains its ANDI delay prefix; the
 * final OR executes in JR's delay slot.
 *
 * Native mapping: The fixed PS1 address uses validated retained regions,
 * exact byte knownness, and no guest-to-host pointer cast.
 */
int nba97_game_draw_offset_command(Nba97GameDrawOffsetCommandContext *,
                                   Nba97GameDrawOffsetCommandProgress *);

#ifdef __cplusplus
}
#endif
#endif
