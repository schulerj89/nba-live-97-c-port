#ifndef NBA97_GAME_PREGAME_SELECTION_SCREEN_H
#define NBA97_GAME_PREGAME_SELECTION_SCREEN_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GamePregameSelectionScreenWord;
typedef Nba97GameMatchClocksMachine Nba97GamePregameSelectionScreenMachine;

enum Nba97GamePregameSelectionScreenCallKind {
  NBA97_GAME_PREGAME_SELECTION_SCREEN_80081358 = 1,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_800363DC,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_8003081C,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_80035678,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_80046738,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_80049018,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_800A5810,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_80083EEC,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_80036478,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_80029258,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_80036BE4,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_80036600,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_8008048C,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_CALL_KIND_COUNT
};

typedef struct Nba97GamePregameSelectionScreenEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GamePregameSelectionScreenEvent;

/* The callback receives JAL's ra after the exact delay instruction. It may
 * mutate all GPRs, HI/LO, retained memory, stack locals, and saved registers.
 */
typedef int (*Nba97GamePregameSelectionScreenIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97GamePregameSelectionScreenEvent *,
    Nba97GamePregameSelectionScreenMachine *);

enum Nba97GamePregameSelectionScreenAccessKind {
  NBA97_GAME_PREGAME_SELECTION_SCREEN_READ = 1,
  NBA97_GAME_PREGAME_SELECTION_SCREEN_STORE = 2
};

typedef struct Nba97GamePregameSelectionScreenAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GamePregameSelectionScreenAccess;

typedef struct Nba97GamePregameSelectionScreenContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GamePregameSelectionScreenMachine machine;
  Nba97GamePregameSelectionScreenIo io;
  void *user;
  Nba97GamePregameSelectionScreenAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GamePregameSelectionScreenContext;

typedef struct Nba97GamePregameSelectionScreenProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_GAME_PREGAME_SELECTION_SCREEN_CALL_KIND_COUNT];
  size_t call_count[NBA97_GAME_PREGAME_SELECTION_SCREEN_CALL_KIND_COUNT];
  size_t redraws;
  size_t polls;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GamePregameSelectionScreenWord restored_return_address;
  Nba97GamePregameSelectionScreenWord restored_s[9];
  Nba97GamePregameSelectionScreenMachine machine;
  uint8_t demo_skip;
  uint8_t input_latched;
  uint8_t completed;
} Nba97GamePregameSelectionScreenProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80046C2C
 * Range: 0x80046C2C..0x80046F67 (inclusive)
 * Source size: 828 bytes / 207 instructions
 * Evidence: fresh Ghidra game_80046c2c.txt; instruction SHA-256
 * 4f301a8398a9415f58686b7bd861a1c0d85207f3784538f2025047a7cc820bb1
 *
 * Purpose: Display and operate the optional pregame selection loop until
 * input, demo mode, or its source timer selects an exit.
 * Inputs: Full 32-GPR/HI-LO machine; retained 0x40-byte stack frame and two
 * halfword locals; mode, controller, and demo globals; thirteen typed services.
 * Returns: Callback-live machine with ra and s8..s0 restored through live sp,
 * sp advanced by 0x40, and restored ra consumed after the final NOP delay.
 * Guest memory: Saves ra/s8..s0; reads and writes stack halfwords and globals
 * 0x8001EDEC, 0x800FDB78, and 0x800FDB9C in exact source order; restores the
 * saved frame after all exit services.
 * Calls: 0x80081358 at 0x80046C54; 0x800363DC at 0x80046C70; 0x8003081C at
 * 0x80046C7C; 0x80035678 at 0x80046C9C; 0x80046738 at 0x80046CA8;
 * 0x80049018 at 0x80046CB0; 0x800A5810 at 0x80046CB8; 0x80083EEC at
 * 0x80046CCC; 0x80036478 at 0x80046CD8; 0x800A5810 at 0x80046D24;
 * 0x80029258 at 0x80046D68; 0x80036BE4 at 0x80046D84; 0x80029258 at
 * 0x80046DB8, 0x80046E0C, 0x80046E5C, 0x80046E80, 0x80046EA4, and
 * 0x80046EC8; 0x80049018 at 0x80046ED8; 0x80035678 at 0x80046F00;
 * 0x80029258 at 0x80046F08; 0x80036600 at 0x80046F14; 0x8003081C at
 * 0x80046F1C; 0x80049018 at 0x80046F24; 0x8008048C at 0x80046F2C.
 * Original quirks: Both selection indices are coupled by branch-delay writes;
 * elapsed time uses wrapping SUBU followed by signed tests; the controller
 * halfword is temporarily replaced and restored through callback-live s0/s6;
 * every callback may relocate sp and the saved frame.
 * Native mapping: Guest addresses remain uint32_t values resolved through
 * validated mapped regions; full-machine callbacks and per-byte knownness keep
 * access, delay, alias, unknown, refusal, and bounded-loop prefixes observable.
 */
int nba97_game_pregame_selection_screen(
    Nba97GamePregameSelectionScreenContext *,
    Nba97GamePregameSelectionScreenProgress *);

#ifdef __cplusplus
}
#endif
#endif
