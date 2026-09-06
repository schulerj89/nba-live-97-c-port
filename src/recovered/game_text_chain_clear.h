#ifndef NBA97_GAME_TEXT_CHAIN_CLEAR_H
#define NBA97_GAME_TEXT_CHAIN_CLEAR_H

#include "game_countdown_ui_update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameCountdownUiUpdateWord Nba97GameTextChainClearWord;
typedef Nba97GameCountdownUiUpdateMachine Nba97GameTextChainClearMachine;
typedef Nba97GameCountdownUiUpdateAccess Nba97GameTextChainClearAccess;

enum Nba97GameTextChainClearAccessKind {
  NBA97_GAME_TEXT_CHAIN_CLEAR_READ = 1,
  NBA97_GAME_TEXT_CHAIN_CLEAR_STORE = 2
};

typedef struct Nba97GameTextChainClearContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameTextChainClearMachine machine;
  Nba97GameTextChainClearAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameTextChainClearContext;

typedef struct Nba97GameTextChainClearProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t chain_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t instruction_count;
  Nba97GameTextChainClearMachine machine;
  uint8_t completed;
} Nba97GameTextChainClearProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8003066C
 * Range: 0x8003066C..0x800306E7 (inclusive)
 * Source size: 124 bytes / 31 instructions
 * Evidence: fresh Ghidra game_8003066c.txt; instruction SHA-256 cc232ba81dfc988aabbaaa955e1d2c257f0634f49e6e49520c7ee3f1333e1746
 *
 * Purpose: Clear the linked text entries for one signed-low-half slot and mark that slot's head table cell empty.
 * Inputs: a0 carries the raw slot value; all GPRs, HI, LO, ra, font pointer 0x800B2048, and font-owned head/link memory are live source inputs.
 * Returns: Raw source register state after the JR NOP delay; HI, LO, sp, and ra are unchanged.
 * Guest memory: Reads 0x800B2048, font+0x14 head-table base, indexed head halfword, fresh font+0x10 link base per visited link, and link+0x18; stores zero to each link+0x12 before its branch resolves and -1 to the final indexed head cell.
 * Calls: None observed.
 * Original quirks: The font pointer is read before the negative-index exit; unchecked signed indices and links use wrapping mapped addresses; cycles have no source guard; terminal link entries are cleared in the BGEZ delay slot.
 * Native mapping: Guest addresses use validated retained mapped memory with per-byte knownness, access-prefix diagnostics, and an explicit operation budget; no guest address is cast to a host pointer.
 */
int nba97_game_text_chain_clear(Nba97GameTextChainClearContext *,
                                Nba97GameTextChainClearProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
