#ifndef NBA97_GAME_IMAGE_RECORD_UPLOAD_H
#define NBA97_GAME_IMAGE_RECORD_UPLOAD_H

#include "game_countdown_ui_update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameCountdownUiUpdateWord Nba97GameImageRecordUploadWord;
typedef Nba97GameCountdownUiUpdateMachine Nba97GameImageRecordUploadMachine;
typedef Nba97GameCountdownUiUpdateAccess Nba97GameImageRecordUploadAccess;

enum Nba97GameImageRecordUploadCallKind {
  NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800A3BF8 = 1,
  NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4,
  NBA97_GAME_IMAGE_RECORD_UPLOAD_CALL_KIND_COUNT
};

typedef struct Nba97GameImageRecordUploadEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameImageRecordUploadEvent;

typedef int (*Nba97GameImageRecordUploadIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameImageRecordUploadEvent *, Nba97GameImageRecordUploadMachine *);

enum Nba97GameImageRecordUploadAccessKind {
  NBA97_GAME_IMAGE_RECORD_UPLOAD_READ = 1,
  NBA97_GAME_IMAGE_RECORD_UPLOAD_STORE = 2
};

typedef struct Nba97GameImageRecordUploadContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameImageRecordUploadMachine machine;
  Nba97GameImageRecordUploadIo io;
  void *user;
  Nba97GameImageRecordUploadAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameImageRecordUploadContext;

typedef struct Nba97GameImageRecordUploadProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_IMAGE_RECORD_UPLOAD_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_IMAGE_RECORD_UPLOAD_CALL_KIND_COUNT];
  size_t records_visited;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameImageRecordUploadWord saved_return_address;
  Nba97GameImageRecordUploadWord restored_return_address;
  Nba97GameImageRecordUploadMachine machine;
  uint8_t completed;
} Nba97GameImageRecordUploadProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80094540
 * Range: 0x80094540..0x800946A3 (inclusive)
 * Source size: 356 bytes / 89 instructions
 * Evidence: fresh Ghidra game_80094540.txt; instruction SHA-256 a5143c4c8d65ce4f5f2cedbc38824e250bb702edad2358fff88bbeed885c8f9d
 *
 * Purpose: Walk a linked image-record list, update supported record headers and rectangles, and submit upload descriptors through typed rendering services.
 * Inputs: Full live GPR/HI/LO state, a0 list head, a1/a2/a3 rectangle fields, the fifth argument at entry sp+0x10, mapped record/stack memory, and typed children 0x800A3BF8 and 0x800944F4.
 * Returns: Full callback-live machine state with ra/s4/s3/s2/s1/s0 restored through live sp, sp advanced by 0x30, and restored ra consumed after the JR NOP delay.
 * Guest memory: Reads linked headers and dimensions, updates record flags/halfwords, builds the four-halfword upload descriptor at live sp+0x10, follows signed relative links, and preserves exact access order and failure prefixes.
 * Calls: 0x800A3BF8 at 0x800945C8 with the s3 descriptor halfword stored in the delay slot; 0x800944F4 at 0x8009464C with a NOP delay slot.
 * Original quirks: Header dispatch masks only bit 3; type-0x23 zero tests use full a3 and the full stack argument; signed MULT publishes HI/LO; negative products use the source add-and-shift rounding path; every link word is freshly reloaded after callbacks.
 * Native mapping: All guest addresses use validated uint32_t mapped-memory accesses with per-byte knownness and an explicit operation budget; callbacks receive the complete machine and may relocate live sp and s0.
 */
int nba97_game_image_record_upload(Nba97GameImageRecordUploadContext *,
                                   Nba97GameImageRecordUploadProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
