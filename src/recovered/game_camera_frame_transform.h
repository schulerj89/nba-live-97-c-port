#ifndef NBA97_GAME_CAMERA_FRAME_TRANSFORM_H
#define NBA97_GAME_CAMERA_FRAME_TRANSFORM_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameCameraFrameTransformWord;
typedef Nba97GameMatchClocksMachine Nba97GameCameraFrameTransformMachine;
typedef Nba97GameMatchClocksAccess Nba97GameCameraFrameTransformAccess;

enum Nba97GameCameraFrameTransformCallKind {
  NBA97_GAME_CAMERA_FRAME_TRANSFORM_CONTROLLER_8004EA88 = 1,
  NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080,
  NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_ROTATION_80055F18,
  NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_TRANSLATION_80055F44,
  NBA97_GAME_CAMERA_FRAME_TRANSFORM_REFERENCE_80056650,
  NBA97_GAME_CAMERA_FRAME_TRANSFORM_CALL_KIND_COUNT
};

typedef struct Nba97GameCameraFrameTransformEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameCameraFrameTransformEvent;

typedef int (*Nba97GameCameraFrameTransformIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameCameraFrameTransformEvent *,
    Nba97GameCameraFrameTransformMachine *);

typedef struct Nba97GameCameraFrameTransformContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCameraFrameTransformMachine machine;
  Nba97GameCameraFrameTransformIo io;
  void *user;
  Nba97GameCameraFrameTransformAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameCameraFrameTransformContext;

typedef struct Nba97GameCameraFrameTransformMultiply {
  uint32_t pc;
  uint32_t mfhi_pc;
  Nba97GameCameraFrameTransformWord multiplicand;
  Nba97GameCameraFrameTransformWord multiplier;
  Nba97GameCameraFrameTransformWord hi;
  Nba97GameCameraFrameTransformWord lo;
  Nba97GameCameraFrameTransformWord mfhi;
} Nba97GameCameraFrameTransformMultiply;

typedef struct Nba97GameCameraFrameTransformProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t multiply_count;
  size_t call_count[NBA97_GAME_CAMERA_FRAME_TRANSFORM_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameCameraFrameTransformWord restored_return_address;
  Nba97GameCameraFrameTransformWord restored_s1;
  Nba97GameCameraFrameTransformWord restored_s0;
  Nba97GameCameraFrameTransformMultiply multiply[3];
  Nba97GameCameraFrameTransformMachine machine;
  uint8_t completed;
} Nba97GameCameraFrameTransformProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80051098
 * Range: 0x80051098..0x80051293 (inclusive)
 * Source size: 508 bytes / 127 instructions
 * Evidence: fresh Ghidra game_80051098.txt; instruction SHA-256
 * e6df654e5e06920a44bd20d440af798af4c58e44f4c82c0be72a39c5b8bc54d2
 *
 * Purpose: Build and install the camera transform, transform its reference, and compose translation.
 * Inputs: Full 32-GPR/HI-LO machine, retained stack, camera globals, matrix storage, and typed children.
 * Returns: Preserves source-live GPR/HI-LO effects, restores ra/s1/s0 through live sp, then adds 0x30 to sp.
 * Guest memory: Reads 0x800EB678 and camera inputs, writes angles/copy/stack/matrix/translation, then reloads the frame.
 * Calls: 0x8004EA88 at 0x800510B4; 0x80056080 at 0x80051168; 0x80055F18 at 0x80051204; 0x80055F44 at 0x8005120C; 0x80056650 at 0x80051228.
 * Original quirks: Signed MULT/MFHI division by ten preserves wrapping shifts, INT16 edges, and live child mutations.
 * Native mapping: Guest addresses use validated uint32_t regions with byte knownness; callbacks carry the full machine.
 */
int nba97_game_camera_frame_transform(Nba97GameCameraFrameTransformContext *,
                                      Nba97GameCameraFrameTransformProgress *);

#ifdef __cplusplus
}
#endif
#endif
