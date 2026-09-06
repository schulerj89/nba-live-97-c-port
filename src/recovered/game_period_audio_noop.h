#ifndef NBA97_GAME_PERIOD_AUDIO_NOOP_H
#define NBA97_GAME_PERIOD_AUDIO_NOOP_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GamePeriodAudioNoopWord;
typedef Nba97GameMatchClocksMachine Nba97GamePeriodAudioNoopMachine;

typedef struct Nba97GamePeriodAudioNoopContext {
  Nba97GamePeriodAudioNoopMachine machine;
} Nba97GamePeriodAudioNoopContext;

typedef struct Nba97GamePeriodAudioNoopProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GamePeriodAudioNoopWord return_address;
  Nba97GamePeriodAudioNoopMachine machine;
  uint8_t completed;
} Nba97GamePeriodAudioNoopProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8002A254
 * Range: 0x8002A254..0x8002A25B (inclusive)
 * Source size: 8 bytes / 2 instructions
 * Evidence: fresh Ghidra game_8002a254.txt; instruction SHA-256 6d64edf91449c1b17746c1ef18afa2eb25c70bdf1322ab3df5a2630993b7e2f1
 *
 * Purpose: Return immediately from the period audio hook without changing machine or guest state.
 * Inputs: All 32 live GPRs, HI/LO, and live ra for JR; all arguments, including a0, are ignored.
 * Returns: Every GPR and HI/LO word and known mask unchanged; no return value is fabricated.
 * Guest memory: None observed.
 * Calls: None observed.
 * Original quirks: The hook performs no audio work and leaves v0 unchanged; unknown or unaligned ra is reported only after JR's NOP delay.
 * Native mapping: Full machine state passes through directly with no guest-memory access, guest-pointer cast, child callback, or host-side substitute behavior.
 */
int nba97_game_period_audio_noop(
    Nba97GamePeriodAudioNoopContext *, Nba97GamePeriodAudioNoopProgress *);

#ifdef __cplusplus
}
#endif
#endif
