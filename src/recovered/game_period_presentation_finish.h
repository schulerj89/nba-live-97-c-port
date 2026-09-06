#ifndef NBA97_GAME_PERIOD_PRESENTATION_FINISH_H
#define NBA97_GAME_PERIOD_PRESENTATION_FINISH_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GamePeriodPresentationFinishWord;
typedef Nba97GameMatchClocksMachine Nba97GamePeriodPresentationFinishMachine;

enum Nba97GamePeriodPresentationFinishCallKind {
  NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80044550 = 1,
  NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80046C2C,
  NBA97_GAME_PERIOD_PRESENTATION_FINISH_CALL_KIND_COUNT
};

typedef struct Nba97GamePeriodPresentationFinishEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GamePeriodPresentationFinishEvent;

typedef int (*Nba97GamePeriodPresentationFinishIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GamePeriodPresentationFinishEvent *,
    Nba97GamePeriodPresentationFinishMachine *);

enum Nba97GamePeriodPresentationFinishAccessKind {
  NBA97_GAME_PERIOD_PRESENTATION_FINISH_READ = 1,
  NBA97_GAME_PERIOD_PRESENTATION_FINISH_STORE = 2
};

typedef struct Nba97GamePeriodPresentationFinishAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GamePeriodPresentationFinishAccess;

typedef struct Nba97GamePeriodPresentationFinishContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GamePeriodPresentationFinishMachine machine;
  Nba97GamePeriodPresentationFinishIo io;
  void *user;
  Nba97GamePeriodPresentationFinishAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GamePeriodPresentationFinishContext;

typedef struct Nba97GamePeriodPresentationFinishProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_PERIOD_PRESENTATION_FINISH_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GamePeriodPresentationFinishWord source_word;
  Nba97GamePeriodPresentationFinishWord gate_flag;
  Nba97GamePeriodPresentationFinishWord saved_return_address;
  Nba97GamePeriodPresentationFinishWord restored_return_address;
  Nba97GamePeriodPresentationFinishWord returned_value;
  Nba97GamePeriodPresentationFinishMachine machine;
  uint8_t optional_child_called;
  uint8_t completed;
} Nba97GamePeriodPresentationFinishProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8002DDCC
 * Range: 0x8002DDCC..0x8002DE33 (inclusive)
 * Source size: 104 bytes / 26 instructions
 * Evidence: fresh Ghidra game_8002ddcc.txt; instruction SHA-256 eacc7dbe8c2324ac5d4cd30b204d9e41b92260d9fd8e01d32ffc33db27a6712b
 *
 * Purpose: Publish first-period presentation state, run its finish service, optionally restore display state, and clear the active marker.
 * Inputs: All 32 live GPRs, HI/LO, live sp/ra, source word 0x8001EDE8, mutable gate byte 0x800FDB78, retained globals, and two typed child services.
 * Returns: Live post-child v0 and full machine state, with ra reloaded through callback-live sp, sp advanced by 0x18, and restored ra consumed by JR.
 * Guest memory: Reads 0x8001EDE8; saves ra; clears 0x800EB680; writes 1 then 0 to 0x80109AFC around presentation; publishes the source word at 0x80109AE4; reads 0x800FDB78; then reloads ra, all in source order.
 * Calls: 0x80044550 at 0x8002DDF8 with a NOP delay, then conditionally 0x80046C2C at 0x8002DE14 with a NOP delay.
 * Original quirks: sp allocation is the source LW delay; the first child may change the later gate byte; a nonzero gate leaves its LBU value in v0, while a zero gate exposes the second child's raw v0; the final clear precedes epilogue failures.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory values with little-endian per-byte knownness, exact access/call prefixes, and full mutable-machine callbacks; no guest address is cast to a host pointer.
 */
int nba97_game_period_presentation_finish(
    Nba97GamePeriodPresentationFinishContext *,
    Nba97GamePeriodPresentationFinishProgress *);

#ifdef __cplusplus
}
#endif
#endif
