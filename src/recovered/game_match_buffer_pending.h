#ifndef NBA97_GAME_MATCH_BUFFER_PENDING_H
#define NBA97_GAME_MATCH_BUFFER_PENDING_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameMatchBufferPendingWord;
typedef Nba97GameMatchClocksMachine Nba97GameMatchBufferPendingMachine;

enum Nba97GameMatchBufferPendingAccessKind {
  NBA97_GAME_MATCH_BUFFER_PENDING_STORE = 1
};

typedef struct Nba97GameMatchBufferPendingAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameMatchBufferPendingAccess;

typedef struct Nba97GameMatchBufferPendingContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameMatchBufferPendingMachine machine;
  Nba97GameMatchBufferPendingAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameMatchBufferPendingContext;

typedef struct Nba97GameMatchBufferPendingProgress {
  size_t operations;
  size_t accesses;
  size_t stores;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameMatchBufferPendingWord return_address;
  Nba97GameMatchBufferPendingWord returned_value;
  Nba97GameMatchBufferPendingMachine machine;
  uint8_t completed;
} Nba97GameMatchBufferPendingProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80076B28
 * Range: 0x80076B28..0x80076B3B (inclusive)
 * Source size: 20 bytes / 5 instructions
 * Evidence: fresh Ghidra game_80076b28.txt; instruction SHA-256 7a8289f8a38324a1bcda832e1d355ab0d9e95facf5f5cdc4872c81cd6628c7e3
 *
 * Purpose: Mark the retained match buffer as pending and return success.
 * Inputs: All 32 live GPRs, HI/LO, live ra, and mapped pending byte 0x800FE864; argument registers are ignored.
 * Returns: v0=1 and at=0x80100000 with every other GPR and HI/LO unchanged; live ra supplies the JR target.
 * Guest memory: Stores byte 1 at 0x800FE864 before consuming ra.
 * Calls: None observed.
 * Original quirks: The fixed register assignments precede every store failure; the store precedes unknown or unaligned JR failure.
 * Native mapping: The guest byte uses validated uint32_t retained memory with per-byte knownness and no host-pointer cast; full-machine state remains explicit.
 */
int nba97_game_match_buffer_pending(
    Nba97GameMatchBufferPendingContext *,
    Nba97GameMatchBufferPendingProgress *);

#ifdef __cplusplus
}
#endif
#endif
