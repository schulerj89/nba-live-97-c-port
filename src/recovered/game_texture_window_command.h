#ifndef NBA97_GAME_TEXTURE_WINDOW_COMMAND_H
#define NBA97_GAME_TEXTURE_WINDOW_COMMAND_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameTextureWindowCommandWord;
typedef Nba97GameMatchClocksMachine Nba97GameTextureWindowCommandMachine;
typedef Nba97GameMatchClocksAccess Nba97GameTextureWindowCommandAccess;

typedef struct Nba97GameTextureWindowCommandContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameTextureWindowCommandMachine machine;
  Nba97GameTextureWindowCommandAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameTextureWindowCommandContext;

typedef struct Nba97GameTextureWindowCommandProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t frame_stack_pointer;
  Nba97GameTextureWindowCommandWord return_v0;
  Nba97GameTextureWindowCommandMachine machine;
  uint8_t completed;
} Nba97GameTextureWindowCommandProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009A824
 * Range: 0x8009A824..0x8009A8A7 (inclusive)
 * Source size: 132 bytes / 33 instructions
 * Evidence: fresh Ghidra game_8009a824.txt; instruction SHA-256
 * f2d40cd7880eaef38f2f91670a1d1221ee18c294f310c40d2a06ae425bcf7526
 *
 * Purpose: Pack a guest texture-window rectangle into the PS1 GPU E2 command
 * while retaining the source stack scratch writes.
 * Inputs: Full 32-GPR/HI-LO machine, nullable guest rectangle pointer in a0,
 * live sp and ra, and mapped rectangle/stack bytes.
 * Returns: Null a0 returns v0=0; otherwise v0 is the E2 command, v1 is the
 * height mask, a0 is height mask shifted five, a1 is E2 plus x bits, a2 is
 * the width mask, sp is restored, and all other state is preserved.
 * Guest memory: On the nonnull path reads a0+0, +4, +2, +6 and stores the four
 * intermediate words at frame offsets 0, 8, 4, 12 in that exact order.
 * Calls: None observed.
 * Original quirks: The entry branch decrements sp in its delay slot even on
 * null or unknown a0; x/y use only their low bytes; negated signed widths and
 * heights wrap before mask/shift; stack/input aliases remain observable.
 * Native mapping: Guest pointers remain validated uint32_t mapped addresses
 * with per-byte knownness; no host pointer cast or GPU action is performed.
 */
int nba97_game_texture_window_command(
    Nba97GameTextureWindowCommandContext *,
    Nba97GameTextureWindowCommandProgress *);

#ifdef __cplusplus
}
#endif
#endif
