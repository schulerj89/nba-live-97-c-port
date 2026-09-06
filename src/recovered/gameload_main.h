#ifndef NBA97_GAMELOAD_MAIN_H
#define NBA97_GAMELOAD_MAIN_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameloadMainWord {
  uint32_t word;
  uint8_t known_mask;
} Nba97GameloadMainWord;

typedef struct Nba97GameloadMainRegisters {
  Nba97GameloadMainWord gpr[32];
} Nba97GameloadMainRegisters;

typedef struct Nba97GameloadMainMachine {
  Nba97GameloadMainRegisters registers;
  Nba97GameloadMainWord hi;
  Nba97GameloadMainWord lo;
} Nba97GameloadMainMachine;

enum Nba97GameloadMainRegister {
  NBA97_GAMELOAD_MAIN_ZERO = 0,
  NBA97_GAMELOAD_MAIN_AT = 1,
  NBA97_GAMELOAD_MAIN_V0 = 2,
  NBA97_GAMELOAD_MAIN_V1 = 3,
  NBA97_GAMELOAD_MAIN_A0 = 4,
  NBA97_GAMELOAD_MAIN_A1 = 5,
  NBA97_GAMELOAD_MAIN_A2 = 6,
  NBA97_GAMELOAD_MAIN_A3 = 7,
  NBA97_GAMELOAD_MAIN_S0 = 16,
  NBA97_GAMELOAD_MAIN_SP = 29,
  NBA97_GAMELOAD_MAIN_RA = 31,
  NBA97_GAMELOAD_MAIN_REGISTER_COUNT = 32
};

enum Nba97GameloadMainProgram {
  NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD = 1,
  NBA97_GAMELOAD_MAIN_PROGRAM_GAMEONLY = 2
};

enum Nba97GameloadMainSite {
  NBA97_GAMELOAD_MAIN_SITE_NONE = 0,
  NBA97_GAMELOAD_MAIN_SITE_801E1374,
  NBA97_GAMELOAD_MAIN_SITE_801E137C,
  NBA97_GAMELOAD_MAIN_SITE_801E1384,
  NBA97_GAMELOAD_MAIN_SITE_801E1394,
  NBA97_GAMELOAD_MAIN_SITE_801E13B0,
  NBA97_GAMELOAD_MAIN_SITE_801E13C4,
  NBA97_GAMELOAD_MAIN_SITE_801E13CC,
  NBA97_GAMELOAD_MAIN_SITE_801E13E0,
  NBA97_GAMELOAD_MAIN_SITE_801E13F4,
  NBA97_GAMELOAD_MAIN_SITE_COUNT
};

typedef enum Nba97GameloadMainCalleeOutcome {
  NBA97_GAMELOAD_MAIN_CALLEE_RETURNED = 1,
  NBA97_GAMELOAD_MAIN_CALLEE_TRANSFERRED = 2
} Nba97GameloadMainCalleeOutcome;

typedef struct Nba97GameloadMainEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97GameloadMainEvent;

typedef int (*Nba97GameloadMainIo)(
    void *, const Nba97GameTextMemory *, const Nba97GameloadMainEvent *,
    Nba97GameloadMainMachine *, Nba97GameloadMainCalleeOutcome *);

enum Nba97GameloadMainAccessKind {
  NBA97_GAMELOAD_MAIN_READ = 1,
  NBA97_GAMELOAD_MAIN_STORE = 2
};

typedef struct Nba97GameloadMainAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameloadMainAccess;

typedef struct Nba97GameloadMainContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameloadMainMachine machine;
  Nba97GameloadMainIo io;
  void *user;
  Nba97GameloadMainAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97GameloadMainContext;

typedef struct Nba97GameloadMainProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t instruction_events;
  size_t call_attempts[NBA97_GAMELOAD_MAIN_SITE_COUNT];
  size_t call_count[NBA97_GAMELOAD_MAIN_SITE_COUNT];
  uint32_t instruction_count;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  Nba97GameloadMainWord saved_return_address;
  Nba97GameloadMainWord saved_s0;
  Nba97GameloadMainWord loaded_copy_size;
  Nba97GameloadMainWord first_copy_length;
  Nba97GameloadMainWord second_copy_length;
  Nba97GameloadMainWord loaded_gameonly_entry;
  Nba97GameloadMainWord restored_return_address;
  Nba97GameloadMainWord restored_s0;
  Nba97GameloadMainMachine machine;
  uint8_t completed;
  uint8_t transferred;
  uint8_t trapped;
} Nba97GameloadMainProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMELOAD
 * Address: 0x801E136C
 * Range: 0x801E136C..0x801E140F (inclusive)
 * Source size: 164 bytes / 41 instructions
 * Evidence: fresh Ghidra gameload_801e136c_continue.txt and mapped GAMELOAD.BIN range; SHA-256 a2d2a4b742c47b1c72d89e7c8b2ddbada0fee604cef947e11914515653e82398
 *
 * Purpose: Initialize GAMELOAD services, load GAMEONLY, restore the copied staging bytes, and invoke the freshly loaded GAMEONLY entry.
 * Inputs: No formal arguments; all 32 live GPR words and byte-known masks, HI/LO, callback-live S0/SP, retained stack and globals, and nine typed child boundaries.
 * Returns: A transferring GAMEONLY child publishes its live machine without an ordinary return; a returning child restores RA and S0 through callback-live SP, raises SP by 24, and returns through live RA.
 * Guest memory: Stores RA and S0 in the 24-byte live frame; reads copy length at 0x80015004 and GAMEONLY entry at 0x80015000 in source order; child services own the two byte copies and loader effects.
 * Calls: GAMELOAD targets 0x801E14B8, 0x801E000C, 0x801E059C, 0x801E0938, 0x801E1344, 0x801E1300, 0x801E1670, and 0x801E1344, then a dynamic GAMEONLY target loaded at 0x801E13EC.
 * Original quirks: Both copy delay slots forward callback-live S0; the GAMEONLY target is loaded late and latched before JALR; a returning dynamic child restores through callback-live SP; SP arithmetic wraps; late unknown or misaligned targets and RA trap after their NOP delays.
 * Native mapping: Guest addresses remain uint32_t values over validated retained regions; all children remain typed callbacks with explicit returned/transferred outcomes and no host-pointer cast.
 */
int nba97_gameload_main(Nba97GameloadMainContext *,
                        Nba97GameloadMainProgress *);

#ifdef __cplusplus
}
#endif
#endif
