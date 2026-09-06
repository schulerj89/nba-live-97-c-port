#ifndef NBA97_GAME_MATCH_BUFFER_INITIALIZE_H
#define NBA97_GAME_MATCH_BUFFER_INITIALIZE_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameMatchBufferInitializeWord;
typedef Nba97GameMatchClocksMachine Nba97GameMatchBufferInitializeMachine;
typedef Nba97GameMatchClocksAccess Nba97GameMatchBufferInitializeAccess;

enum Nba97GameMatchBufferInitializeCallKind {
  NBA97_GAME_MATCH_BUFFER_INITIALIZE_ZERO_800A3A74 = 1,
  NBA97_GAME_MATCH_BUFFER_INITIALIZE_CHILD_80076AD0,
  NBA97_GAME_MATCH_BUFFER_INITIALIZE_CALL_KIND_COUNT
};

typedef struct Nba97GameMatchBufferInitializeEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameMatchBufferInitializeEvent;

/* A callback observes JAL's ra and its completed delay slot, including the
 * full machine. It may mutate every GPR, HI/LO, mapped byte, sp, and frame. */
typedef int (*Nba97GameMatchBufferInitializeIo)(void *,
    const Nba97GameTextMemory *, const Nba97GameMatchBufferInitializeEvent *,
    Nba97GameMatchBufferInitializeMachine *);

typedef struct Nba97GameMatchBufferInitializeContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameMatchBufferInitializeMachine machine;
  Nba97GameMatchBufferInitializeIo io;
  void *user;
  Nba97GameMatchBufferInitializeAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameMatchBufferInitializeContext;

typedef struct Nba97GameMatchBufferInitializeProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_MATCH_BUFFER_INITIALIZE_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameMatchBufferInitializeWord saved_return_address;
  Nba97GameMatchBufferInitializeWord restored_return_address;
  Nba97GameMatchBufferInitializeWord returned_value;
  Nba97GameMatchBufferInitializeMachine machine;
  uint8_t completed;
} Nba97GameMatchBufferInitializeProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8006432C
 * Range: 0x8006432C..0x80064387 (inclusive)
 * Source size: 92 bytes / 23 instructions
 * Evidence: fresh Ghidra game_8006432c.txt; instruction SHA-256 e3f3b0accccac912d89dda8e1b1357e39cc3159ed73359ad0532d8cb765f260a
 *
 * Purpose: Zero the retained match buffer, publish its initial header words, and dispatch its follow-up initializer.
 * Inputs: All 32 live GPRs, HI/LO, retained sp/ra stack state, mapped buffer 0x800F9FFC..0x800FA373, recovered zero service 0x800A3A74, and typed child 0x80076AD0.
 * Returns: Raw final-child v0 and full live machine state, with ra restored through callback-live sp and sp advanced by 0x20.
 * Guest memory: Saves ra at frame+0x18, zeroes 0x378 bytes from 0x800F9FFC through the child, stores 0x0076 at 0x800FA000, 0x800CCC00 at 0x800FA004, and 0x800D5734 at 0x800FA008, then reloads ra from live sp+0x18.
 * Calls: 0x800A3A74 at 0x8006433C with delay-slot a1=0x378, then 0x80076AD0 at 0x80064370 with a NOP delay.
 * Original quirks: The first child retains incoming v0 while changing zero-fill scratch registers; all three header stores occur after the clear and may alias the saved frame; the final child's raw v0 is not normalized.
 * Native mapping: Guest addresses use validated uint32_t retained regions with little-endian access, per-byte knownness, exact failure prefixes, and full mutable-machine callbacks; the zero algorithm is composed through its existing owner rather than copied.
 */
int nba97_game_match_buffer_initialize(
    Nba97GameMatchBufferInitializeContext *,
    Nba97GameMatchBufferInitializeProgress *);

#ifdef __cplusplus
}
#endif
#endif
