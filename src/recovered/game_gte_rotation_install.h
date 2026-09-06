#ifndef NBA97_GAME_GTE_ROTATION_INSTALL_H
#define NBA97_GAME_GTE_ROTATION_INSTALL_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameGteRotationInstallWord;
typedef Nba97GameMatchClocksMachine Nba97GameGteRotationInstallMachine;
typedef Nba97GameMatchClocksAccess Nba97GameGteRotationInstallAccess;

typedef struct Nba97GameGteRotationInstallControlWrite {
  uint32_t pc;
  size_t operation;
  Nba97GameGteRotationInstallWord value;
  uint8_t index;
} Nba97GameGteRotationInstallControlWrite;

typedef struct Nba97GameGteRotationInstallContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameGteRotationInstallMachine machine;
  Nba97GameGteRotationInstallWord *control;
  Nba97GameGteRotationInstallAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameGteRotationInstallControlWrite *control_journal;
  size_t control_journal_capacity;
} Nba97GameGteRotationInstallContext;

typedef struct Nba97GameGteRotationInstallProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t control_writes;
  size_t access_events;
  size_t control_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint8_t stopped_control;
  Nba97GameGteRotationInstallMachine machine;
  Nba97GameGteRotationInstallWord control[32];
  uint8_t completed;
} Nba97GameGteRotationInstallProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80055F18
 * Range: 0x80055F18..0x80055F43 (inclusive)
 * Source size: 44 bytes / 11 instructions
 * Evidence: fresh Ghidra game_80055f18.txt; instruction SHA-256
 * a94f6d55839c2c553ac20e1d1283001a5a1de0419f63333fb421db4d0d1a085c
 *
 * Purpose: Load a five-word rotation matrix and install it in GTE controls
 * 0 through 4.
 * Inputs: Full 32-GPR/HI-LO machine, raw guest pointer a0, five mapped words,
 * and explicit retained GTE control state.
 * Returns: Leaves t0..t4 holding the five raw loads, preserves every other
 * GPR and HI/LO, and consumes live ra after the final delay-slot CTC2.
 * Guest memory: Reads words at a0+0, +4, +8, +12, and +16 in that order;
 * writes no guest memory.
 * Calls: None observed.
 * Original quirks: All five LW complete before any CTC2; RT33/control 4
 * sign-extends its low half while t4 retains the raw loaded word.
 * Native mapping: Validated uint32_t guest regions retain byte knownness;
 * explicit byte-known GTE controls model CTC2 without a CPU/GTE interpreter.
 */
int nba97_game_gte_rotation_install(
    Nba97GameGteRotationInstallContext *,
    Nba97GameGteRotationInstallProgress *);

#ifdef __cplusplus
}
#endif
#endif
