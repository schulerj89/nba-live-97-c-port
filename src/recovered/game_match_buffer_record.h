#ifndef NBA97_GAME_MATCH_BUFFER_RECORD_H
#define NBA97_GAME_MATCH_BUFFER_RECORD_H

#include "game_match_buffer_rewind.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchBufferRewindWord Nba97GameMatchBufferRecordWord;
typedef Nba97GameMatchBufferRewindMachine Nba97GameMatchBufferRecordMachine;
typedef Nba97GameMatchBufferRewindAccess Nba97GameMatchBufferRecordAccess;

enum Nba97GameMatchBufferRecordCallKind {
  NBA97_GAME_MATCH_BUFFER_RECORD_REWIND_80076AD0 = 1,
  NBA97_GAME_MATCH_BUFFER_RECORD_COMPRESS_800767FC,
  NBA97_GAME_MATCH_BUFFER_RECORD_CALL_KIND_COUNT
};

typedef struct Nba97GameMatchBufferRecordEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameMatchBufferRecordEvent;

typedef int (*Nba97GameMatchBufferRecordIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GameMatchBufferRecordEvent *,
    Nba97GameMatchBufferRecordMachine *);

typedef struct Nba97GameMatchBufferRecordContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameMatchBufferRecordMachine machine;
  Nba97GameMatchBufferRecordIo io;
  void *user;
  Nba97GameMatchBufferRecordAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameMatchBufferRecordContext;

typedef struct Nba97GameMatchBufferRecordProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_MATCH_BUFFER_RECORD_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_MATCH_BUFFER_RECORD_CALL_KIND_COUNT];
  size_t entity_iterations;
  size_t cursor_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameMatchBufferRecordWord restored_return_address;
  Nba97GameMatchBufferRecordMachine machine;
  uint8_t used_rewind;
  uint8_t completed;
} Nba97GameMatchBufferRecordProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80076B3C
 * Range: 0x80076B3C..0x80076FC7 (inclusive)
 * Source size: 1164 bytes / 291 instructions
 * Evidence: fresh Ghidra game_80076b3c.txt; instruction SHA-256 60c9e13e670a1686ddf55fafbc6c5798fab0cac87ebd20c6feedc78fe7e6d5be
 *
 * Purpose: Capture one retained match-state frame and advance the compressed match-buffer cursors.
 * Inputs: Full live GPR/HI/LO state; retained globals, two snapshots, controller and ball pointers, eleven physical entities, and typed rewind/compression children.
 * Returns: v0 holds the final source cursor-logic value; ra is restored through callback-live sp, sp advances by 0x18, and all other source-visible machine mutations remain live.
 * Guest memory: Reads/writes the stack, snapshots 0x800F1814/0x800F1918, retained globals 0x8001/0x8002/0x800B/0x800F/0x8010, eleven 244-byte physical entities, and variable encoded records in exact source order.
 * Calls: 0x80076AD0 at 0x80076B50 with NOP delay; 0x800767FC at 0x80076E58 with delay a3=0x82.
 * Original quirks: The first ten entities copy thirteen byte fields while all eleven copy signed SRA8 coordinates; globals truncate to bytes/halfwords; cursor record lengths may be zero and create budget-bounded source runaways; all callback mutations and delay prefixes remain live.
 * Native mapping: Guest addresses are validated uint32_t mapped values with per-byte knownness and observable accesses; rewind composes its existing owner and compression remains a typed full-machine dependency.
 */
int nba97_game_match_buffer_record(Nba97GameMatchBufferRecordContext *,
                                   Nba97GameMatchBufferRecordProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
