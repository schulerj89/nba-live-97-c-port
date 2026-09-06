#ifndef NBA97_GAME_GTE_TRANSLATION_INSTALL_H
#define NBA97_GAME_GTE_TRANSLATION_INSTALL_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameGteTranslationInstallWord;
typedef Nba97GameMatchClocksMachine Nba97GameGteTranslationInstallMachine;
typedef Nba97GameMatchClocksAccess Nba97GameGteTranslationInstallAccess;

typedef struct Nba97GameGteTranslationInstallControlWrite {
  uint32_t pc;
  size_t operation;
  Nba97GameGteTranslationInstallWord value;
  uint8_t index;
} Nba97GameGteTranslationInstallControlWrite;

typedef struct Nba97GameGteTranslationInstallContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameGteTranslationInstallMachine machine;
  Nba97GameGteTranslationInstallWord *control;
  Nba97GameGteTranslationInstallAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameGteTranslationInstallControlWrite *control_journal;
  size_t control_journal_capacity;
} Nba97GameGteTranslationInstallContext;

typedef struct Nba97GameGteTranslationInstallProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t control_writes;
  size_t access_events;
  size_t control_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint8_t stopped_control;
  Nba97GameGteTranslationInstallMachine machine;
  Nba97GameGteTranslationInstallWord control[32];
  uint8_t completed;
} Nba97GameGteTranslationInstallProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80055F44
 * Range: 0x80055F44..0x80055F5F (inclusive)
 * Source size: 28 bytes / 7 instructions
 * Evidence: fresh Ghidra game_80055f44.txt; instruction SHA-256
 * 78671a48aee9c49116cf1dea240b731b38a17237d4975e6421f76dc727e5dd25;
 * DuckStation GTE control-write behavior for controls 5 through 7.
 *
 * Purpose: Load a three-word translation vector and install its raw values in
 * GTE controls 5 through 7.
 * Inputs: Full 32-GPR/HI-LO machine, raw guest pointer a0, three mapped words
 * at a0+0x14/+0x18/+0x1C, and explicit retained GTE control state.
 * Returns: Leaves t0..t2 holding the three raw loads, preserves every other
 * GPR and HI/LO, and consumes live ra after the final delay-slot CTC2.
 * Guest memory: Reads three words at a0+0x14, +0x18, and +0x1C in that order;
 * writes no guest memory.
 * Calls: None observed.
 * Original quirks: All three LW complete before any CTC2, all controls retain
 * raw 32-bit values, and control 7 is written in the JR delay slot.
 * Native mapping: Validated uint32_t guest regions retain byte knownness;
 * explicit byte-known GTE controls model CTC2 without host-pointer casts or a
 * CPU/GTE interpreter.
 */
int nba97_game_gte_translation_install(
    Nba97GameGteTranslationInstallContext *,
    Nba97GameGteTranslationInstallProgress *);

#ifdef __cplusplus
}
#endif
#endif
