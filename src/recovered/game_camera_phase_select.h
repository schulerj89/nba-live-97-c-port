#ifndef NBA97_GAME_CAMERA_PHASE_SELECT_H
#define NBA97_GAME_CAMERA_PHASE_SELECT_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameCameraPhaseSelectWord;
typedef Nba97GameMatchClocksMachine Nba97GameCameraPhaseSelectMachine;
typedef Nba97GameMatchClocksAccess Nba97GameCameraPhaseSelectAccess;

enum Nba97GameCameraPhaseSelectCallKind {
  NBA97_GAME_CAMERA_PHASE_SELECT_CAMERA_800799CC = 1,
  NBA97_GAME_CAMERA_PHASE_SELECT_ADJUST_80079EBC,
  NBA97_GAME_CAMERA_PHASE_SELECT_FINALIZE_80079F78,
  NBA97_GAME_CAMERA_PHASE_SELECT_CALL_KIND_COUNT
};

typedef struct Nba97GameCameraPhaseSelectEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameCameraPhaseSelectEvent;

typedef int (*Nba97GameCameraPhaseSelectIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraPhaseSelectEvent *,
    Nba97GameCameraPhaseSelectMachine *);

typedef struct Nba97GameCameraPhaseSelectContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCameraPhaseSelectMachine machine;
  Nba97GameCameraPhaseSelectIo io;
  void *user;
  Nba97GameCameraPhaseSelectAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameCameraPhaseSelectContext;

typedef struct Nba97GameCameraPhaseSelectProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_CAMERA_PHASE_SELECT_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_CAMERA_PHASE_SELECT_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameCameraPhaseSelectWord selected_phase;
  Nba97GameCameraPhaseSelectWord published_phase;
  Nba97GameCameraPhaseSelectWord restored_return_address;
  Nba97GameCameraPhaseSelectMachine machine;
  uint8_t phase_changed;
  uint8_t completed;
} Nba97GameCameraPhaseSelectProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8007E26C
 * Range: 0x8007E26C..0x8007E463 (inclusive)
 * Source size: 504 bytes / 126 instructions
 * Evidence: fresh Ghidra game_8007e26c.txt; instruction SHA-256
 * e824731f1e58d3d3c6c6e664d93f2d0d6375f4a362a96eb863337521dbccd7bb
 *
 * Purpose: Select and publish camera phase 1 through 4 from live match state.
 * Inputs: Full live GPR/HI-LO state, low byte of a0, camera/match globals,
 * callback-live stack, and typed camera/adjust/finalize children.
 * Returns: Live callback machine with ra reloaded through live sp, sp advanced
 * by 0x18, and source-selected v0/v1/a0/a1/at values.
 * Guest memory: Reads and writes 0x800FC99C, 0x800F9FFE, 0x800BC940/44,
 * 0x800FDB90/68/58/60, 0x800FE884, 0x80021ED8, and live sp+0x10 in source
 * order.
 * Calls: 0x800799CC at 0x8007E3A0; 0x80079EBC at 0x8007E3A8,
 * 0x8007E3B0, and 0x8007E3B8; 0x800799CC at 0x8007E3DC; 0x80079EBC at
 * 0x8007E3E4; 0x800799CC at 0x8007E424; and 0x80079F78 at 0x8007E434.
 * Original quirks: Busy/guard exits retain their delay-slot stores and ANDI;
 * unchanged/invalid phases still publish, and changed paths reread
 * callback-mutable current phase before publishing it.
 * Native mapping: uint32_t guest addresses use validated retained regions and
 * per-byte knownness; children receive typed full-machine events.
 */
int nba97_game_camera_phase_select(Nba97GameCameraPhaseSelectContext *,
                                   Nba97GameCameraPhaseSelectProgress *);

#ifdef __cplusplus
}
#endif
#endif
