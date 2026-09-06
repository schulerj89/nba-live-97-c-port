#ifndef NBA97_GAMELOAD_ENTRY_H
#define NBA97_GAMELOAD_ENTRY_H

#include "frontend_main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendMainWord Nba97GameloadEntryWord;
typedef Nba97FrontendMainMachine Nba97GameloadEntryMachine;

enum Nba97GameloadEntryRegister {
  NBA97_GAMELOAD_ENTRY_ZERO = 0,
  NBA97_GAMELOAD_ENTRY_AT = 1,
  NBA97_GAMELOAD_ENTRY_V0 = 2,
  NBA97_GAMELOAD_ENTRY_V1 = 3,
  NBA97_GAMELOAD_ENTRY_A0 = 4,
  NBA97_GAMELOAD_ENTRY_A1 = 5,
  NBA97_GAMELOAD_ENTRY_T0 = 8,
  NBA97_GAMELOAD_ENTRY_GP = 28,
  NBA97_GAMELOAD_ENTRY_SP = 29,
  NBA97_GAMELOAD_ENTRY_S8 = 30,
  NBA97_GAMELOAD_ENTRY_RA = 31,
  NBA97_GAMELOAD_ENTRY_REGISTER_COUNT = 32
};

enum Nba97GameloadEntrySite {
  NBA97_GAMELOAD_ENTRY_SITE_NONE = 0,
  NBA97_GAMELOAD_ENTRY_SITE_801E1498,
  NBA97_GAMELOAD_ENTRY_SITE_801E14AC,
  NBA97_GAMELOAD_ENTRY_SITE_COUNT
};

enum Nba97GameloadEntryAccessKind {
  NBA97_GAMELOAD_ENTRY_READ = 1,
  NBA97_GAMELOAD_ENTRY_STORE = 2
};

typedef enum Nba97GameloadEntryCalleeOutcome {
  NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED = 1,
  NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED = 2
} Nba97GameloadEntryCalleeOutcome;

typedef struct Nba97GameloadEntryEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97GameloadEntryEvent;

typedef int (*Nba97GameloadEntryIo)(
    void *, const Nba97GameTextMemory *, const Nba97GameloadEntryEvent *,
    Nba97GameloadEntryMachine *, Nba97GameloadEntryCalleeOutcome *);

typedef struct Nba97GameloadEntryAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97GameloadEntryAccess;

typedef struct Nba97GameloadEntryContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameloadEntryMachine machine;
  Nba97GameloadEntryIo io;
  void *user;
  Nba97GameloadEntryAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97GameloadEntryContext;

typedef struct Nba97GameloadEntryProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t words_cleared;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t call_attempts[NBA97_GAMELOAD_ENTRY_SITE_COUNT];
  size_t call_count[NBA97_GAMELOAD_ENTRY_SITE_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t instruction_count;
  Nba97GameloadEntryWord loaded_stack_top;
  Nba97GameloadEntryWord loaded_heap_reserve;
  Nba97GameloadEntryWord heap_base;
  Nba97GameloadEntryWord heap_size;
  Nba97GameloadEntryWord saved_return_address;
  Nba97GameloadEntryWord restored_return_address;
  Nba97GameloadEntryMachine machine;
  uint8_t first_child_entered;
  uint8_t second_child_entered;
  uint8_t completed;
  uint8_t transferred;
  uint8_t trapped;
} Nba97GameloadEntryProgress;

enum Nba97GameloadEntryResult {
  NBA97_GAMELOAD_ENTRY_BREAK_TRAP = -6,
  NBA97_GAMELOAD_ENTRY_ARITHMETIC_TRAP = -7
};

/*
 * PS1 SUBROUTINE
 * Program: GAMELOAD
 * Address: 0x801E1410
 * Range: 0x801E1410..0x801E14B7 (inclusive)
 * Source size: 168 bytes / 42 instructions
 * Evidence: fresh Ghidra gameload_801e1410_continue.txt and independently
 * hashed GAMELOAD.BIN range; SHA-256
 * 86de52922bd45fe1e8c5dd5768bb04d31a1a1ba8d0c9bc429d8a53b1919ae560
 *
 * Purpose: Clear GAMELOAD BSS, establish the startup stack, heap and global
 * pointer, invoke InitHeap and GAMELOAD main, then execute BREAK 1 if main
 * returns.
 * Inputs: All 32 live MIPS GPRs, HI/LO, retained GAMELOAD memory, and typed
 * full-machine children at 0x801E1590 and 0x801E136C.
 * Returns: No ordinary source return. A transferred child exposes its exact
 * live machine; returned GAMELOAD main reaches BREAK 1.
 * Guest memory: Clears 2,073 words in [0x801E903C,0x801EB0A0), reads stack
 * top 0x801E8B70 and heap subtraction value 0x801E8B6C, stores heap size/base
 * and entry ra,
 * then reloads saved ra after InitHeap.
 * Calls: 0x801E1590(a0,a1) at 0x801E1498 and 0x801E136C() at 0x801E14AC.
 * Original quirks: Both ADDI instructions trap on signed overflow; the first
 * call's delay adds four to a0; callback-live memory supplies restored ra; a
 * returning second child falls into BREAK 1.
 * Native mapping: Guest addresses remain uint32_t values over validated
 * retained regions with per-byte knownness. The same-address FELOAD routine is
 * a different program owner and is not reused.
 */
int nba97_gameload_entry(Nba97GameloadEntryContext *,
                         Nba97GameloadEntryProgress *);

#ifdef __cplusplus
}
#endif
#endif
