#ifndef NBA97_GAME_CAMERA_REMAINDER_GATE_H
#define NBA97_GAME_CAMERA_REMAINDER_GATE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameCameraRemainderGateWord;
typedef Nba97GameMatchClocksMachine Nba97GameCameraRemainderGateMachine;
typedef Nba97GameMatchClocksAccess Nba97GameCameraRemainderGateAccess;

typedef struct Nba97GameCameraRemainderGateContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCameraRemainderGateMachine machine;
  Nba97GameCameraRemainderGateAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameCameraRemainderGateContext;

typedef struct Nba97GameCameraRemainderGateProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t instruction_count;
  Nba97GameCameraRemainderGateWord source_value;
  Nba97GameCameraRemainderGateWord adjusted_value;
  Nba97GameCameraRemainderGateWord remainder_value;
  Nba97GameCameraRemainderGateWord returned_value;
  Nba97GameCameraRemainderGateMachine machine;
  uint8_t negative;
  uint8_t completed;
} Nba97GameCameraRemainderGateProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8007A468
 * Range: 0x8007A468..0x8007A497 (inclusive)
 * Source size: 48 bytes / 12 instructions
 * Evidence: fresh Ghidra game_8007a468.txt; instruction SHA-256
 * 23f32766a2cfbb7a3a6a307a53bd48082d5dc9b6230fc612f6d59302b90f39f6
 *
 * Purpose: Test whether the signed camera-state remainder modulo 2048 is
 * within the inclusive range -50..50.
 * Inputs: Full live GPR/HI-LO state and the signed source word at 0x800FC9AC.
 * Returns: The unsigned SLTIU Boolean and knownness in v0, with the raw source
 * retained in v1 and all other machine state preserved.
 * Guest memory: Reads the little-endian word at 0x800FC9AC exactly once.
 * Calls: None observed.
 * Original quirks: Negative division is implemented by adding 0x7FF with
 * 32-bit wrap before SRA; JR's delay computes the final unsigned comparison.
 * Native mapping: The guest address is resolved through validated retained
 * memory with per-byte knownness and is never cast to a host pointer.
 */
int nba97_game_camera_remainder_gate(Nba97GameCameraRemainderGateContext *,
                                     Nba97GameCameraRemainderGateProgress *);

#ifdef __cplusplus
}
#endif
#endif
