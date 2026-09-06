#ifndef NBA97_GAME_DRAW_AREA_END_H
#define NBA97_GAME_DRAW_AREA_END_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameDrawAreaEndWord;
typedef Nba97GameMatchClocksMachine Nba97GameDrawAreaEndMachine;
typedef Nba97GameMatchClocksAccess Nba97GameDrawAreaEndAccess;

typedef struct Nba97GameDrawAreaEndContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameDrawAreaEndMachine machine;
  Nba97GameDrawAreaEndAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameDrawAreaEndContext;

typedef struct Nba97GameDrawAreaEndProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameDrawAreaEndWord return_v0;
  Nba97GameDrawAreaEndMachine machine;
  uint8_t completed;
} Nba97GameDrawAreaEndProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009A710
 * Range: 0x8009A710..0x8009A7DB (inclusive)
 * Source size: 204 bytes / 51 instructions
 * Evidence: fresh Ghidra game_8009a710.txt; instruction SHA-256
 * 0f5135aa43b161924d022ff28c214458f897f9df0228b62b4aaba8f7971fff5b
 *
 * Purpose: Clamp signed draw-area end coordinates to retained display limits
 * and return the PS1 GPU E4 draw-area-end command.
 *
 * Inputs: Full 32-GPR/HI-LO machine; a0/a1 low halves are signed x/y; display
 * type byte 0x800C55C0 and signed limit halves
 * 0x800C55C4/0x800C55C6.
 *
 * Returns: v0 is the packed E4 command; a0 is 0xE4000000, a1 is clamped y,
 * a2 is x-limit minus one only if read, and v1 is the shifted packed y.
 *
 * Guest memory: Conditionally reads the x and y limit halfwords, then reads
 * the display-type byte; performs no guest stores.
 *
 * Calls: None observed.
 *
 * Original quirks: Negative coordinates skip their corresponding limit read;
 * signed limit-minus-one wraps; types one and two select 12-bit packing while
 * every other byte selects 10-bit packing; final OR is the JR delay slot.
 *
 * Native mapping: Fixed PS1 addresses use validated retained regions, exact
 * little-endian reads, full-machine state, and per-byte knownness.
 */
int nba97_game_draw_area_end(Nba97GameDrawAreaEndContext *,
                             Nba97GameDrawAreaEndProgress *);

#ifdef __cplusplus
}
#endif
#endif
