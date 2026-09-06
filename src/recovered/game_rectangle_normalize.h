#ifndef NBA97_GAME_RECTANGLE_NORMALIZE_H
#define NBA97_GAME_RECTANGLE_NORMALIZE_H

#include "game_rectangle_upload_submit.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameRectangleUploadSubmitWord Nba97GameRectangleNormalizeWord;
typedef Nba97GameRectangleUploadSubmitMachine Nba97GameRectangleNormalizeMachine;
typedef Nba97GameRectangleUploadSubmitAccess Nba97GameRectangleNormalizeAccess;

enum Nba97GameRectangleNormalizeAccessKind {
  NBA97_GAME_RECTANGLE_NORMALIZE_READ = 1,
  NBA97_GAME_RECTANGLE_NORMALIZE_STORE = 2
};

typedef struct Nba97GameRectangleNormalizeContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameRectangleNormalizeMachine machine;
  Nba97GameRectangleNormalizeAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameRectangleNormalizeContext;

typedef struct Nba97GameRectangleNormalizeProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t instruction_count;
  Nba97GameRectangleNormalizeMachine machine;
  uint8_t completed;
} Nba97GameRectangleNormalizeProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80094440
 * Range: 0x80094440..0x8009446B (inclusive)
 * Source size: 44 bytes / 11 instructions
 * Evidence: fresh Ghidra game_80094440.txt; instruction SHA-256 758caadbf9bfc2e94e142374466f354974715d9525934b12817928022cb73d7f
 *
 * Purpose: Force an odd rectangle height when its unsigned width is odd.
 * Inputs: Full live GPR/HI/LO state, a0 as a mapped rectangle address, unsigned width at a0+4, unsigned height at a0+6, and live ra.
 * Returns: V0 is zero for even width or the unsigned odd-normalized height for odd width; all other registers and HI/LO remain live, and ra is consumed after the JR NOP delay.
 * Guest memory: Reads the width halfword first; only odd width reads the height halfword and stores its low-bit-forced value back to the same address.
 * Calls: None observed.
 * Original quirks: Even width never reads height; width 0xFFFF takes the odd path; height 0xFFFF remains 0xFFFF; unknown upper width byte is discarded before the branch.
 * Native mapping: The rectangle remains a validated uint32_t guest address with little-endian mapped accesses, per-byte knownness, exact failure prefixes, and an explicit operation budget.
 */
int nba97_game_rectangle_normalize(Nba97GameRectangleNormalizeContext *,
                                   Nba97GameRectangleNormalizeProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
