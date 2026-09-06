#ifndef NBA97_GAME_MATCH_BUFFER_COMPRESS_H
#define NBA97_GAME_MATCH_BUFFER_COMPRESS_H

#include "game_match_buffer_rewind.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchBufferRewindWord Nba97GameMatchBufferCompressWord;
typedef Nba97GameMatchBufferRewindMachine Nba97GameMatchBufferCompressMachine;
typedef Nba97GameMatchBufferRewindAccess Nba97GameMatchBufferCompressAccess;

enum Nba97GameMatchBufferCompressAccessKind {
  NBA97_GAME_MATCH_BUFFER_COMPRESS_READ = 1,
  NBA97_GAME_MATCH_BUFFER_COMPRESS_STORE = 2
};

typedef struct Nba97GameMatchBufferCompressContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameMatchBufferCompressMachine machine;
  Nba97GameMatchBufferCompressAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameMatchBufferCompressContext;

typedef struct Nba97GameMatchBufferCompressProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t element_iterations;
  size_t completed_flag_groups;
  uint32_t instruction_count;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameMatchBufferCompressMachine machine;
  uint8_t completed;
} Nba97GameMatchBufferCompressProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800767FC
 * Range: 0x800767FC..0x800768EF (inclusive)
 * Source size: 244 bytes / 61 instructions
 * Evidence: fresh Ghidra game_800767fc.txt; instruction SHA-256 903fcb4272bf882ee14901f4938131758c86949ebe45770b5d04959e84b98638
 *
 * Purpose: Delta-compress two retained halfword snapshots into grouped two-bit flags and variable-width difference bytes.
 * Inputs: Full live GPR/HI/LO state; a0/a1 address the halfword snapshots, a2 addresses the output record, and a3 is the raw element count.
 * Returns: v0 is one byte past the terminal length sentinel; all other source-mutated GPRs are retained and HI/LO are unchanged.
 * Guest memory: Reads paired halfwords in source order, writes grouped flags and delta bytes through a2/t3/t4, then toggles halfword 0x800F9FFC with XOR 1.
 * Calls: None observed.
 * Original quirks: Count termination uses only the decremented low 16 bits, so count zero processes 65536 elements; the full count affects the initial header skip, differences use modular low-16 signed-byte classification, and output/source/global aliases remain live.
 * Native mapping: Guest addresses remain validated uint32_t values over retained regions with per-byte knownness, observable access order, and an explicit operation budget.
 */
int nba97_game_match_buffer_compress(Nba97GameMatchBufferCompressContext *,
                                     Nba97GameMatchBufferCompressProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
