#ifndef NBA97_GAME_DRAW_AREA_START_H
#define NBA97_GAME_DRAW_AREA_START_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameDrawAreaStartWord;
typedef Nba97GameMatchClocksMachine Nba97GameDrawAreaStartMachine;
typedef Nba97GameMatchClocksAccess Nba97GameDrawAreaStartAccess;

typedef struct Nba97GameDrawAreaStartContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameDrawAreaStartMachine machine;
  Nba97GameDrawAreaStartAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameDrawAreaStartContext;

typedef struct Nba97GameDrawAreaStartProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameDrawAreaStartWord return_v0;
  Nba97GameDrawAreaStartMachine machine;
  uint8_t completed;
} Nba97GameDrawAreaStartProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009A644
 * Range: 0x8009A644..0x8009A70F (inclusive)
 * Source size: 204 bytes / 51 instructions
 * Evidence: fresh Ghidra game_8009a644.txt; instruction SHA-256
 * 3c7734bec5a5abe7be9a069a0ed5a4cc70bcf291f6122580303e0cea36164f34
 *
 * Purpose: Clamp signed draw-area origins to retained display limits and
 * return the PS1 GPU E3 draw-area-start command.
 * Inputs: Full 32-GPR/HI-LO machine; a0/a1 low halves are signed x/y; display
 * type byte 0x800C55C0 and signed limit halves 0x800C55C4/0x800C55C6.
 * Returns: v0 is the packed E3 command; a0 is 0xE3000000, a1 is clamped y,
 * a2 is x-limit minus one only if read, and v1 is the shifted packed y.
 * Guest memory: Conditionally reads the x and y limit halfwords, then reads
 * the display-type byte; performs no guest stores.
 * Calls: None observed.
 * Original quirks: Negative coordinates skip their corresponding limit read;
 * signed limit-minus-one wraps; types one and two select 12-bit packing while
 * every other byte selects 10-bit packing; final OR is the JR delay slot.
 * Native mapping: Fixed PS1 addresses use validated retained regions, exact
 * little-endian reads, full-machine state, and per-byte knownness.
 */
int nba97_game_draw_area_start(Nba97GameDrawAreaStartContext *,
                               Nba97GameDrawAreaStartProgress *);

#ifdef __cplusplus
}
#endif
#endif
