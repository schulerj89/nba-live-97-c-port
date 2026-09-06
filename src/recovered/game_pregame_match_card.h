#ifndef NBA97_GAME_PREGAME_MATCH_CARD_H
#define NBA97_GAME_PREGAME_MATCH_CARD_H

#include "game_period_presentation_finish.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GamePeriodPresentationFinishWord Nba97GamePregameMatchCardWord;
typedef Nba97GamePeriodPresentationFinishMachine
    Nba97GamePregameMatchCardMachine;
typedef Nba97GamePeriodPresentationFinishAccess Nba97GamePregameMatchCardAccess;

enum Nba97GamePregameMatchCardCallKind {
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_800810A4 = 1,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_8003081C,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80031614,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80030D18,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036688,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_8009CB6C,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80081B50,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_8009CB7C,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80035678,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_800A5810,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_800363DC,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80083EEC,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036478,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80088D0C,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_8002DE34,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80029880,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80049018,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80029258,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036600,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_8008048C,
  NBA97_GAME_PREGAME_MATCH_CARD_CALL_KIND_COUNT
};

typedef struct Nba97GamePregameMatchCardEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GamePregameMatchCardEvent;

typedef int (*Nba97GamePregameMatchCardIo)(
    void *, const Nba97GameTextMemory *, const Nba97GamePregameMatchCardEvent *,
    Nba97GamePregameMatchCardMachine *);

enum Nba97GamePregameMatchCardAccessKind {
  NBA97_GAME_PREGAME_MATCH_CARD_READ = 1,
  NBA97_GAME_PREGAME_MATCH_CARD_STORE = 2
};

typedef struct Nba97GamePregameMatchCardContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GamePregameMatchCardMachine machine;
  Nba97GamePregameMatchCardIo io;
  void *user;
  Nba97GamePregameMatchCardAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GamePregameMatchCardContext;

typedef struct Nba97GamePregameMatchCardProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_PREGAME_MATCH_CARD_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_PREGAME_MATCH_CARD_CALL_KIND_COUNT];
  size_t polling_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97GamePregameMatchCardWord saved_return_address;
  Nba97GamePregameMatchCardWord restored_return_address;
  Nba97GamePregameMatchCardMachine machine;
  uint8_t exited_for_input;
  uint8_t exited_for_timeout;
  uint8_t completed;
} Nba97GamePregameMatchCardProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80044550
 * Range: 0x80044550..0x80044997 (inclusive)
 * Source size: 1096 bytes / 274 instructions
 * Evidence: fresh Ghidra game_80044550.txt; instruction SHA-256 ceffd8479963e8b590f58996f88f2e95d737f1412a272a90a31c82d561c1d834
 *
 * Purpose: Build the pregame team/location card through typed UI services, then pump input, audio, timing, and frames until timeout or accepted skip.
 * Inputs: Full live GPR/HI/LO state, live sp/ra, retained team/location/demo/presentation globals, mapped frame scratch, and twenty typed service kinds.
 * Returns: Full callback-live machine state with ra/s4/s3/s2/s1/s0 reloaded through live sp, sp advanced by 0x68, and the restored ra consumed by JR.
 * Guest memory: Uses the live stack frame at sp+0x10..0x64, font mode [0x800B2048]+0x26, team/location/demo globals, skip byte 0x800FDB78, and presentation byte 0x800EB680 in exact source order.
 * Calls: Thirty-seven typed calls from 0x80044568 through 0x8004496C in exact source order, including seven 0x80031614 layout calls and eight 0x80030D18 text calls.
 * Original quirks: Location selection branches on the full word but passes its signed low half; readiness accumulation starts at -1 and resets on zero; signed/wrapping clock deltas and callback-mutated saved registers/sp remain live.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory values with per-byte knownness, observable access/call prefixes, and an explicit operation budget; no guest address is cast to a host pointer.
 */
int nba97_game_pregame_match_card(Nba97GamePregameMatchCardContext *,
                                   Nba97GamePregameMatchCardProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
