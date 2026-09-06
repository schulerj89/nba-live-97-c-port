#ifndef NBA97_GAME_CAMERA_ELAPSED_DISPATCH_H
#define NBA97_GAME_CAMERA_ELAPSED_DISPATCH_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameCameraElapsedDispatchWord;
typedef Nba97GameMatchClocksMachine Nba97GameCameraElapsedDispatchMachine;
typedef Nba97GameMatchClocksAccess Nba97GameCameraElapsedDispatchAccess;

enum Nba97GameCameraElapsedDispatchCallKind {
  NBA97_GAME_CAMERA_ELAPSED_DISPATCH_INDIRECT_8007995C = 1,
  NBA97_GAME_CAMERA_ELAPSED_DISPATCH_PROBE_8007A468,
  NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410,
  NBA97_GAME_CAMERA_ELAPSED_DISPATCH_CALL_KIND_COUNT
};

typedef struct Nba97GameCameraElapsedDispatchEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameCameraElapsedDispatchEvent;

typedef int (*Nba97GameCameraElapsedDispatchIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraElapsedDispatchEvent *,
    Nba97GameCameraElapsedDispatchMachine *);

typedef struct Nba97GameCameraElapsedDispatchContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCameraElapsedDispatchMachine machine;
  Nba97GameCameraElapsedDispatchIo io;
  void *user;
  Nba97GameCameraElapsedDispatchAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameCameraElapsedDispatchContext;

typedef struct Nba97GameCameraElapsedDispatchProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_CAMERA_ELAPSED_DISPATCH_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_CAMERA_ELAPSED_DISPATCH_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameCameraElapsedDispatchWord requested_delta;
  Nba97GameCameraElapsedDispatchWord elapsed_value;
  Nba97GameCameraElapsedDispatchWord indirect_target;
  Nba97GameCameraElapsedDispatchWord published_value;
  Nba97GameCameraElapsedDispatchWord restored_return_address;
  Nba97GameCameraElapsedDispatchMachine machine;
  uint8_t elapsed_reset;
  uint8_t completed;
} Nba97GameCameraElapsedDispatchProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800798B4
 * Range: 0x800798B4..0x800799CB (inclusive)
 * Source size: 280 bytes / 70 instructions
 * Evidence: fresh Ghidra game_800798b4.txt; instruction SHA-256
 * 731f6e8cdd8970250f106b51aaa7cdb2382737c64e28ea4fea1825a65ab13fbf
 *
 * Purpose: Clamp camera elapsed state and dispatch its threshold services.
 * Inputs: Full live GPR/HI-LO state, signed a0 delta or 0xFFFFFFFF sentinel,
 * camera elapsed globals, callback-live stack, and typed child services.
 * Returns: Live callback machine with source v0, ra reloaded through live sp,
 * and sp advanced by 0x18; threshold exits retain their source Boolean v0.
 * Guest memory: Reads and writes 0x80106074, 0x800BC1F4..0x800BC200,
 * 0x800FC9D0, the dynamic descriptor at +0x5C, 0x800D8EEC, and live sp+0x10
 * in exact source order.
 * Calls: Dynamic target at 0x8007995C; 0x8007A468 at 0x80079978; and
 * 0x8007A410 at 0x8007999C.
 * Original quirks: The sentinel reloads the lower bound, signed inverted bounds
 * can exit after upper clamping, and publication to 0x800D8EEC precedes the
 * elapsed reset at 0x80106074.
 * Native mapping: uint32_t guest addresses use validated retained regions and
 * per-byte knownness; all children receive typed full-machine events.
 */
int nba97_game_camera_elapsed_dispatch(
    Nba97GameCameraElapsedDispatchContext *,
    Nba97GameCameraElapsedDispatchProgress *);

#ifdef __cplusplus
}
#endif
#endif
