#ifndef NBA97_GAME_COUNTDOWN_UI_UPDATE_H
#define NBA97_GAME_COUNTDOWN_UI_UPDATE_H

#include "game_frame_ui_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameFrameUiServiceWord Nba97GameCountdownUiUpdateWord;
typedef Nba97GameFrameUiServiceMachine Nba97GameCountdownUiUpdateMachine;
typedef Nba97GameFrameUiServiceAccess Nba97GameCountdownUiUpdateAccess;

enum Nba97GameCountdownUiUpdateCallKind {
  NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_8003066C = 1,
  NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80030D18,
  NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80094540,
  NBA97_GAME_COUNTDOWN_UI_UPDATE_CALL_KIND_COUNT
};

typedef struct Nba97GameCountdownUiUpdateEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameCountdownUiUpdateEvent;

typedef int (*Nba97GameCountdownUiUpdateIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCountdownUiUpdateEvent *,
    Nba97GameCountdownUiUpdateMachine *);

enum Nba97GameCountdownUiUpdateAccessKind {
  NBA97_GAME_COUNTDOWN_UI_UPDATE_READ = 1,
  NBA97_GAME_COUNTDOWN_UI_UPDATE_STORE = 2
};

typedef struct Nba97GameCountdownUiUpdateContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCountdownUiUpdateMachine machine;
  Nba97GameCountdownUiUpdateIo io;
  void *user;
  Nba97GameCountdownUiUpdateAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameCountdownUiUpdateContext;

typedef struct Nba97GameCountdownUiUpdateProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_COUNTDOWN_UI_UPDATE_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_COUNTDOWN_UI_UPDATE_CALL_KIND_COUNT];
  size_t palette_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameCountdownUiUpdateWord saved_return_address;
  Nba97GameCountdownUiUpdateWord restored_return_address;
  Nba97GameCountdownUiUpdateMachine machine;
  uint8_t active_gate;
  uint8_t record_uploaded;
  uint8_t completed;
} Nba97GameCountdownUiUpdateProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8003287C
 * Range: 0x8003287C..0x80032B0F (inclusive)
 * Source size: 660 bytes / 165 instructions
 * Evidence: fresh Ghidra game_8003287c.txt; instruction SHA-256 168714753e5981c88269049193aeb3f167d2f668938e801f0f4780f6b0442821
 *
 * Purpose: Update the retained countdown display, rebuild its palette record when the displayed quotient changes, and submit that record through typed UI services.
 * Inputs: Full live GPR/HI/LO state, live sp/ra, countdown and gate globals, runtime table bytes at 0x800249E4..0x800249F9, font pointer 0x800B2048, cache 0x800FEA2E, and three typed child kinds.
 * Returns: Full callback-live machine state with ra/s2/s1/s0 reloaded through live sp, sp advanced by 0x40, and restored ra consumed after the JR NOP delay.
 * Guest memory: Reads the countdown/gate/cache/font globals and runtime table, uses live stack 0x10..0x3C, builds 0x800FB5C0..0x800FB5EE through ordered byte/halfword/word stores, and publishes the callback-live quotient to 0x800FEA2E.
 * Calls: 0x8003066C at 0x8003295C; 0x80030D18 at 0x800329E8; and 0x80094540 at 0x80032AE4, with their exact delay-slot arguments/stores.
 * Original quirks: Five words are copied with redundant LWL/LWR then SWL/SWR pairs; failed gates store -1 through callback-live s0; signed magic MULT leaves HI/LO and t2 live; the unchecked signed quotient indexes the copied table through mapped guest semantics.
 * Native mapping: Runtime tables and all guest addresses remain validated uint32_t retained-memory values with per-byte knownness, exact access/call prefixes, and an explicit operation budget; no retail table is embedded and no guest address is cast to a host pointer.
 */
int nba97_game_countdown_ui_update(Nba97GameCountdownUiUpdateContext *,
                                   Nba97GameCountdownUiUpdateProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
