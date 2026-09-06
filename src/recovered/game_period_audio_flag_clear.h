#ifndef NBA97_GAME_PERIOD_AUDIO_FLAG_CLEAR_H
#define NBA97_GAME_PERIOD_AUDIO_FLAG_CLEAR_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GamePeriodAudioFlagClearWord;
typedef Nba97GameMatchClocksMachine Nba97GamePeriodAudioFlagClearMachine;

enum Nba97GamePeriodAudioFlagClearAccessKind {
  NBA97_GAME_PERIOD_AUDIO_FLAG_CLEAR_STORE = 1
};

typedef struct Nba97GamePeriodAudioFlagClearAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GamePeriodAudioFlagClearAccess;

typedef struct Nba97GamePeriodAudioFlagClearContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GamePeriodAudioFlagClearMachine machine;
  Nba97GamePeriodAudioFlagClearAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GamePeriodAudioFlagClearContext;

typedef struct Nba97GamePeriodAudioFlagClearProgress {
  size_t operations;
  size_t accesses;
  size_t stores;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t instruction_count;
  Nba97GamePeriodAudioFlagClearMachine machine;
  uint8_t completed;
} Nba97GamePeriodAudioFlagClearProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8002A244
 * Range: 0x8002A244..0x8002A253 (inclusive)
 * Source size: 16 bytes / 4 instructions
 * Evidence: fresh Ghidra game_8002a244.txt; instruction SHA-256
 * 3f92cb180bea44bc3125f0b4f19228ba4a282f8d4a48af628af3a2c20c4786af
 *
 * Purpose: Clear the retained first-period audio flag byte.
 * Inputs: Full live 32-GPR/HI-LO machine, mapped byte 0x800B1FD5, and live ra.
 * Returns: at is 0x800B0000; every other machine word and mask is preserved.
 * Guest memory: Stores one known zero byte to 0x800B1FD5 at 0x8002A248.
 * Calls: None observed.
 * Original quirks: The store precedes validation of the JR target, and the JR
 * NOP executes before unknown or misaligned ra refusal.
 * Native mapping: The fixed uint32_t guest address uses validated retained
 * regions and per-byte knownness without conversion to a host pointer.
 */
int nba97_game_period_audio_flag_clear(Nba97GamePeriodAudioFlagClearContext *,
                                       Nba97GamePeriodAudioFlagClearProgress *);

#ifdef __cplusplus
}
#endif
#endif
