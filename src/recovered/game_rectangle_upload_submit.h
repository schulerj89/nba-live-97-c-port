#ifndef NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_H
#define NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_H

#include "game_image_record_upload.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameImageRecordUploadWord Nba97GameRectangleUploadSubmitWord;
typedef Nba97GameImageRecordUploadMachine Nba97GameRectangleUploadSubmitMachine;
typedef Nba97GameImageRecordUploadAccess Nba97GameRectangleUploadSubmitAccess;

enum Nba97GameRectangleUploadSubmitCallKind {
  NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440 = 1,
  NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_8009971C,
  NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CALL_KIND_COUNT
};

typedef struct Nba97GameRectangleUploadSubmitEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameRectangleUploadSubmitEvent;

typedef int (*Nba97GameRectangleUploadSubmitIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameRectangleUploadSubmitEvent *,
    Nba97GameRectangleUploadSubmitMachine *);

enum Nba97GameRectangleUploadSubmitAccessKind {
  NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_READ = 1,
  NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_STORE = 2
};

typedef struct Nba97GameRectangleUploadSubmitContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameRectangleUploadSubmitMachine machine;
  Nba97GameRectangleUploadSubmitIo io;
  void *user;
  Nba97GameRectangleUploadSubmitAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameRectangleUploadSubmitContext;

typedef struct Nba97GameRectangleUploadSubmitProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameRectangleUploadSubmitWord saved_return_address;
  Nba97GameRectangleUploadSubmitWord restored_return_address;
  Nba97GameRectangleUploadSubmitMachine machine;
  uint8_t completed;
} Nba97GameRectangleUploadSubmitProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x800944F4
 * Range: 0x800944F4..0x8009453F (inclusive)
 * Source size: 76 bytes / 19 instructions
 * Evidence: fresh Ghidra game_800944f4.txt; instruction SHA-256 bc37688dc8aae684e9c98121320f42e0385f49e7ddad7e6cc93f3257305b2f75; child signature confirmed by game_80094440.txt SHA-256 758caadbf9bfc2e94e142374466f354974715d9525934b12817928022cb73d7f
 *
 * Purpose: Normalize a rectangle through a typed helper, submit its payload through a second typed helper, then mark the upload pending.
 * Inputs: Full live GPR/HI/LO state, a0 rectangle pointer, a1 payload pointer, live sp/ra, mapped stack and 0x800D7B14 memory, and typed children 0x80094440 and 0x8009971C.
 * Returns: V0 is 1 and AT is 0x800D0000 after both callbacks; callback-live ra/s1/s0 are restored through callback-live sp, which advances by 0x20 before the JR NOP delay; all other callback-live state is preserved.
 * Guest memory: Saves s0/s1/ra at frame offsets 0x10/0x14/0x18, writes 1 to 0x800D7B14 only after both callbacks, and restores the three saved words in source order.
 * Calls: 0x80094440 at 0x80094508 with argc 1 and delay-slot s1=a1; 0x8009971C at 0x80094514 with argc 2 and delay-slot a1=callback-live s1.
 * Original quirks: The first callback observes unchanged a0 and the completed delay-slot capture of a1 into s1; both callbacks may replace callee-saved registers and sp; the pending flag is never written after a refused or invalid callback.
 * Native mapping: Guest addresses use validated uint32_t mapped-memory accesses with per-byte knownness and an explicit operation budget; unresolved children receive the complete machine through typed callbacks.
 */
int nba97_game_rectangle_upload_submit(
    Nba97GameRectangleUploadSubmitContext *,
    Nba97GameRectangleUploadSubmitProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
