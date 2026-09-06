#ifndef NBA97_GAME_TEXT_SUBMISSION_H
#define NBA97_GAME_TEXT_SUBMISSION_H

#include "game_countdown_ui_update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameCountdownUiUpdateWord Nba97GameTextSubmissionWord;
typedef Nba97GameCountdownUiUpdateMachine Nba97GameTextSubmissionMachine;
typedef Nba97GameCountdownUiUpdateAccess Nba97GameTextSubmissionAccess;

enum Nba97GameTextSubmissionCallKind {
  NBA97_GAME_TEXT_SUBMISSION_CHILD_8002EB50 = 1,
  NBA97_GAME_TEXT_SUBMISSION_CHILD_8002EF88,
  NBA97_GAME_TEXT_SUBMISSION_CHILD_80099960,
  NBA97_GAME_TEXT_SUBMISSION_CHILD_8002ECD4,
  NBA97_GAME_TEXT_SUBMISSION_CHILD_800AA468,
  NBA97_GAME_TEXT_SUBMISSION_CHILD_80056914,
  NBA97_GAME_TEXT_SUBMISSION_CALL_KIND_COUNT
};

typedef struct Nba97GameTextSubmissionEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameTextSubmissionEvent;

typedef int (*Nba97GameTextSubmissionIo)(void *, const Nba97GameTextMemory *,
                                         const Nba97GameTextSubmissionEvent *,
                                         Nba97GameTextSubmissionMachine *);

enum Nba97GameTextSubmissionAccessKind {
  NBA97_GAME_TEXT_SUBMISSION_READ = 1,
  NBA97_GAME_TEXT_SUBMISSION_STORE = 2
};

typedef struct Nba97GameTextSubmissionContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameTextSubmissionMachine machine;
  Nba97GameTextSubmissionIo io;
  void *user;
  Nba97GameTextSubmissionAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameTextSubmissionContext;

typedef struct Nba97GameTextSubmissionProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_TEXT_SUBMISSION_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_TEXT_SUBMISSION_CALL_KIND_COUNT];
  size_t allocation_iterations;
  size_t glyph_iterations;
  size_t packet_link_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GameTextSubmissionWord return_v0;
  Nba97GameTextSubmissionMachine machine;
  uint8_t completed;
} Nba97GameTextSubmissionProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80030D18
 * Range: 0x80030D18..0x80031523 (inclusive)
 * Source size: 2060 bytes / 515 instructions
 * Evidence: fresh Ghidra game_80030d18.txt; instruction SHA-256 5bb63d6349c6c14fc92caa3cfef8326b9d02eb4b87d6a9477ee9afcbae97daec
 *
 * Purpose: Allocate and link a text record, parse its control string, build paired glyph packets, and submit their ordering-table links.
 * Inputs: a0 signed-low-half slot, a1 string, a2 horizontal coordinate, a3 vertical coordinate, original fifth argument byte at entry sp+0x10, full live machine, font state at 0x800B2048, runtime character tables, and typed child services.
 * Returns: v0 is zero on allocation failure or the allocated record pointer on success; saved registers and ra reload through callback-live sp before the JR NOP delay.
 * Guest memory: Uses the live 0x80-byte frame, font record/head/glyph/alignment tables, caller string and runtime character maps, allocated record and glyph packets, and every callback-visible alias in exact source order.
 * Calls: 0x8002EB50 at 0x80030E14 and 0x800311B4; 0x8002EF88 at 0x80030E20; 0x80099960 at 0x800310A8 and 0x800310B4; 0x8002ECD4 at 0x80031138 and 0x80031228; 0x800AA468 at 0x80031470; and 0x80056914 at 0x800314B8 and 0x800314C4.
 * Original quirks: Exhausted allocation wraps and may reuse a nonfree record; the internal JR dispatch trusts a runtime target table; signed slots and glyph offsets wrap through mapped addresses; packet linking walks backward without a source cycle repair.
 * Native mapping: All guest addresses remain uint32_t retained-memory values with per-byte knownness and explicit access/call budgets; unresolved services remain typed and no retail font, glyph, or string bytes are embedded.
 */
int nba97_game_text_submission(Nba97GameTextSubmissionContext *,
                               Nba97GameTextSubmissionProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
