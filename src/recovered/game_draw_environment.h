#ifndef NBA97_GAME_DRAW_ENVIRONMENT_H
#define NBA97_GAME_DRAW_ENVIRONMENT_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameDrawEnvironmentWord;
typedef Nba97GameMatchClocksMachine Nba97GameDrawEnvironmentMachine;
typedef Nba97GameMatchClocksAccess Nba97GameDrawEnvironmentAccess;

enum Nba97GameDrawEnvironmentCallKind {
  NBA97_GAME_DRAW_ENVIRONMENT_DEBUG_INDIRECT = 1,
  NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344,
  NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT,
  NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C,
  NBA97_GAME_DRAW_ENVIRONMENT_CALL_KIND_COUNT
};

typedef struct Nba97GameDrawEnvironmentEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t kind;
  uint8_t argument_count;
} Nba97GameDrawEnvironmentEvent;

/* The callback observes JAL/JALR's ra and the completed delay slot. It may
 * synchronously mutate mapped memory and every CPU GPR, HI, and LO. Return
 * exactly 1 only after the original child boundary returns. */
typedef int (*Nba97GameDrawEnvironmentIo)(void *, const Nba97GameTextMemory *,
                                          const Nba97GameDrawEnvironmentEvent *,
                                          Nba97GameDrawEnvironmentMachine *);

typedef struct Nba97GameDrawEnvironmentContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameDrawEnvironmentMachine machine;
  Nba97GameDrawEnvironmentIo io;
  void *user;
  Nba97GameDrawEnvironmentAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameDrawEnvironmentContext;

typedef struct Nba97GameDrawEnvironmentProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_count[NBA97_GAME_DRAW_ENVIRONMENT_CALL_KIND_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_entry;
  uint32_t frame_stack_pointer;
  Nba97GameDrawEnvironmentWord restored_return_address;
  Nba97GameDrawEnvironmentWord restored_s2;
  Nba97GameDrawEnvironmentWord restored_s1;
  Nba97GameDrawEnvironmentWord restored_s0;
  Nba97GameDrawEnvironmentMachine machine;
  uint8_t completed;
} Nba97GameDrawEnvironmentProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x80099ACC
 * Range: 0x80099ACC..0x80099B8F (inclusive)
 * Source size: 196 bytes / 49 instructions
 * Evidence: fresh Ghidra game_80099acc.txt; instruction SHA-256
 * e3a8b21fbffb78d4de7709eabc4d916d87a024eb8092b9472d95627f1b8e40f4
 *
 * Purpose: Prepare, tag, submit, and retain the current draw-environment
 * packet.
 * Inputs: Full 32-GPR/HI-LO machine, draw environment in a0, graphics globals
 * at 0x800C55B8..0x800C55C2, retained stack, and four typed child boundaries.
 * Returns: v0 captures callback-live s1 before ra/s2/s1/s0 are restored through
 * callback-live sp; sp is then advanced by 0x20 and live ra is consumed.
 * Guest memory: Saves four words, reads the debug/table/packet words, tags
 * s1+0x1C, submits it, copies 0x5C bytes to live s2+0xE, then restores four
 * words in source order.
 * Calls: indirect [0x800C55BC] at 0x80099B10; 0x8009A344 at 0x80099B20;
 * indirect [[0x800C55B8]+8] at 0x80099B58; 0x8009CB0C at 0x80099B68.
 * Original quirks: Debug values 0/1 skip diagnostics; OR 0x00FFFFFF preserves
 * the packet's upper byte; copy v0 is ignored; callbacks relocate all live
 * saved registers and sp; final unknown ra refuses after the full epilogue.
 * Native mapping: Guest addresses use validated uint32_t regions with per-byte
 * knownness; unresolved calls remain typed full-machine callbacks.
 */
int nba97_game_draw_environment(Nba97GameDrawEnvironmentContext *,
                                Nba97GameDrawEnvironmentProgress *);

#ifdef __cplusplus
}
#endif
#endif
