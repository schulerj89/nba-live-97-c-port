#ifndef NBA97_GAME_PERIOD_MUSIC_START_H
#define NBA97_GAME_PERIOD_MUSIC_START_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GamePeriodMusicStartWord;
typedef Nba97GameMatchClocksMachine Nba97GamePeriodMusicStartMachine;

enum Nba97GamePeriodMusicStartCallKind {
  NBA97_GAME_PERIOD_MUSIC_START_LOAD_800AAE7C = 1,
  NBA97_GAME_PERIOD_MUSIC_START_800AAFA0,
  NBA97_GAME_PERIOD_MUSIC_START_800AB224,
  NBA97_GAME_PERIOD_MUSIC_START_800AB388,
  NBA97_GAME_PERIOD_MUSIC_START_800AB2C8,
  NBA97_GAME_PERIOD_MUSIC_START_CALL_KIND_COUNT
};

typedef struct Nba97GamePeriodMusicStartEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GamePeriodMusicStartEvent;

/* The callback observes JAL's ra and the completed delay instruction. It may
 * mutate every GPR, HI/LO, retained memory, and the caller's saved frame. */
typedef int (*Nba97GamePeriodMusicStartIo)(
    void *, const Nba97GameTextMemory *, const Nba97GamePeriodMusicStartEvent *,
    Nba97GamePeriodMusicStartMachine *);

enum Nba97GamePeriodMusicStartAccessKind {
  NBA97_GAME_PERIOD_MUSIC_START_READ = 1,
  NBA97_GAME_PERIOD_MUSIC_START_STORE = 2
};

typedef struct Nba97GamePeriodMusicStartAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GamePeriodMusicStartAccess;

typedef struct Nba97GamePeriodMusicStartContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GamePeriodMusicStartMachine machine;
  Nba97GamePeriodMusicStartIo io;
  void *user;
  Nba97GamePeriodMusicStartAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GamePeriodMusicStartContext;

typedef struct Nba97GamePeriodMusicStartProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_PERIOD_MUSIC_START_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GamePeriodMusicStartWord scaled_volume;
  Nba97GamePeriodMusicStartWord restored_return_address;
  Nba97GamePeriodMusicStartWord restored_s0;
  Nba97GamePeriodMusicStartMachine machine;
  uint8_t load_music_executed;
  uint8_t playback_executed;
  uint8_t completed;
} Nba97GamePeriodMusicStartProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800295D0
 * Range: 0x800295D0..0x8002968B (inclusive)
 * Source size: 188 bytes / 47 instructions
 * Evidence: fresh Ghidra game_800295d0.txt; instruction SHA-256
 * ddd6f1b71a8e8d0bd5bcb770afb224b5c3dc4b60c084478b53f9cf3555c3072e
 *
 * Purpose: Load first-period music when needed, configure its saturated
 * volume and playback services, and publish the music-start flags.
 * Inputs: All 32 live MIPS GPRs, HI/LO, retained stack, volume byte through
 * live s0, music descriptor words, two flag bytes, and five typed children.
 * Returns: Live child-mutated machine state with ra and s0 reloaded through
 * callback-live sp, sp advanced by 0x18, and the restored ra consumed by JR.
 * Guest memory: Saves s0 then ra; reads 0x80021D7F, optionally reads
 * 0x800B1F38/0x80021D6C/0x800B1F34 and stores 1 at 0x800B1F38; rereads volume
 * through live s0; stores 1 at 0x800B1F39; then reloads ra and s0 in order.
 * Calls: 0x800AAE7C at 0x80029618, 0x800AAFA0 at 0x8002964C,
 * 0x800AB224 at 0x80029654, 0x800AB388 at 0x8002965C, and 0x800AB2C8 at
 * 0x80029664.
 * Original quirks: Zero volume exits without children; load-music mutations
 * of s0 select the reread address; unsigned volume is multiplied by nine and
 * clamped to 127; all delay-slot argument writes and callback mutations live.
 * Native mapping: Guest addresses remain uint32_t values resolved through
 * validated retained regions; typed full-machine callbacks preserve per-byte
 * knownness, failure prefixes, aliases, and HI/LO without host-pointer casts.
 */
int nba97_game_period_music_start(Nba97GamePeriodMusicStartContext *,
                                  Nba97GamePeriodMusicStartProgress *);

#ifdef __cplusplus
}
#endif
#endif
