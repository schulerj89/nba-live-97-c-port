#ifndef NBA97_GAME_FRAME_UI_SERVICE_H
#define NBA97_GAME_FRAME_UI_SERVICE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameFrameUiServiceWord;
typedef Nba97GameMatchClocksMachine Nba97GameFrameUiServiceMachine;
typedef Nba97GameMatchClocksAccess Nba97GameFrameUiServiceAccess;

enum Nba97GameFrameUiServiceCallKind {
  NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003287C = 1,
  NBA97_GAME_FRAME_UI_SERVICE_CHILD_80031C5C,
  NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003066C,
  NBA97_GAME_FRAME_UI_SERVICE_CHILD_80032774,
  NBA97_GAME_FRAME_UI_SERVICE_CALL_KIND_COUNT
};

typedef struct Nba97GameFrameUiServiceEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameFrameUiServiceEvent;

typedef int (*Nba97GameFrameUiServiceIo)(void *, const Nba97GameTextMemory *,
                                         const Nba97GameFrameUiServiceEvent *,
                                         Nba97GameFrameUiServiceMachine *);

enum Nba97GameFrameUiServiceAccessKind {
  NBA97_GAME_FRAME_UI_SERVICE_READ = 1,
  NBA97_GAME_FRAME_UI_SERVICE_STORE = 2
};

typedef struct Nba97GameFrameUiServiceContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameFrameUiServiceMachine machine;
  Nba97GameFrameUiServiceIo io;
  void *user;
  Nba97GameFrameUiServiceAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameFrameUiServiceContext;

typedef struct Nba97GameFrameUiServiceProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_FRAME_UI_SERVICE_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_FRAME_UI_SERVICE_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameFrameUiServiceWord saved_return_address;
  Nba97GameFrameUiServiceWord restored_return_address;
  Nba97GameFrameUiServiceMachine machine;
  uint8_t completed;
} Nba97GameFrameUiServiceProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80032B10
 * Range: 0x80032B10..0x80032BB7 (inclusive)
 * Source size: 168 bytes / 42 instructions
 * Evidence: fresh Ghidra game_80032b10.txt; instruction SHA-256 59cb86799b2fb1c4a46167abe168c76499666269eb593de50ac21205b40506a9
 *
 * Purpose: Run the per-frame UI service and dispatch its conditional update and command children from retained presentation flags.
 * Inputs: Full live GPR/HI/LO state, live sp/ra, signed halfword 0x800FA038, byte 0x800EB680, retained stack, and four typed child kinds.
 * Returns: The final raw child/global value in v0 and full callback-live machine state, with ra reloaded through live sp, sp advanced by 0x18, and restored ra consumed by JR.
 * Guest memory: Saves ra at sp-8, reads 0x800FA038 and conditionally 0x800EB680 in source order, then reloads ra through callback-live sp+0x10.
 * Calls: 0x8003287C at 0x80032B18; 0x80031C5C at 0x80032B34 and 0x80032B60; 0x8003066C at 0x80032B48, 0x80032B50, 0x80032B74, and 0x80032B7C; 0x80032774 at 0x80032BA0.
 * Original quirks: Branches consume the query child's low byte only; the zero-mode path returns the raw presentation byte or 0x80032774 child value; callback mutations to sp and the saved return slot remain live.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory values with per-byte knownness, exact access/call prefixes, and an explicit operation budget; no guest address is cast to a host pointer.
 */
int nba97_game_frame_ui_service(Nba97GameFrameUiServiceContext *,
                                Nba97GameFrameUiServiceProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
