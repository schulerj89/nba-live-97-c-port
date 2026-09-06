#ifndef NBA97_FRONTEND_DISPATCH_ENTRY_H
#define NBA97_FRONTEND_DISPATCH_ENTRY_H

#include "frontend_dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendDispatchWord Nba97FrontendDispatchEntryWord;
typedef Nba97FrontendDispatchMachine Nba97FrontendDispatchEntryMachine;

enum Nba97FrontendDispatchEntryRegister {
  NBA97_FRONTEND_DISPATCH_ENTRY_ZERO = 0,
  NBA97_FRONTEND_DISPATCH_ENTRY_AT = 1,
  NBA97_FRONTEND_DISPATCH_ENTRY_V0 = 2,
  NBA97_FRONTEND_DISPATCH_ENTRY_S0 = 16,
  NBA97_FRONTEND_DISPATCH_ENTRY_SP = 29,
  NBA97_FRONTEND_DISPATCH_ENTRY_RA = 31,
  NBA97_FRONTEND_DISPATCH_ENTRY_REGISTER_COUNT = 32
};

enum Nba97FrontendDispatchEntryAccessKind {
  NBA97_FRONTEND_DISPATCH_ENTRY_READ = 1,
  NBA97_FRONTEND_DISPATCH_ENTRY_STORE = 2
};

typedef struct Nba97FrontendDispatchEntryAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendDispatchEntryAccess;

typedef struct Nba97FrontendDispatchEntryEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t argument_count;
} Nba97FrontendDispatchEntryEvent;

typedef int (*Nba97FrontendDispatchEntryIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97FrontendDispatchEntryEvent *,
    Nba97FrontendDispatchEntryMachine *);

typedef struct Nba97FrontendDispatchEntryContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendDispatchEntryMachine machine;
  Nba97FrontendDispatchEntryIo io;
  void *user;
  Nba97FrontendDispatchEntryAccess *access_journal;
  size_t access_journal_capacity;
} Nba97FrontendDispatchEntryContext;

typedef struct Nba97FrontendDispatchEntryProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callback_attempts;
  size_t callbacks_completed;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendDispatchEntryWord saved_return_address;
  Nba97FrontendDispatchEntryWord restored_return_address;
  Nba97FrontendDispatchEntryMachine machine;
  uint8_t completed;
} Nba97FrontendDispatchEntryProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x800360D4
 * Range: 0x800360D4..0x8003610B (inclusive)
 * Source size: 56 bytes / 14 instructions
 * Evidence: fresh Ghidra feonly_800360d4_resume.txt and independently hashed FEONLY.BIN range; SHA-256 6af71d91fded3e2b5260c84bb86fd101539e86fca86ffef2b9e06b93e32dbce0
 *
 * Purpose: Publish frontend initialization globals and synchronously enter the recovered frontend state dispatcher.
 * Inputs: No formal arguments; all 32 live MIPS GPRs, HI/LO, retained guest memory, and the typed 0x8003F7C8 child callback.
 * Returns: Restores ra from callback-live sp+16, adds 24 to callback-live sp, and otherwise preserves the child-returned GPR/HI/LO state.
 * Guest memory: Stores 1 to 0x80021EE4, saves entry ra to entry-sp-8, stores 32 to 0x800C6E68, then loads ra from callback-live sp+16 in exact source order.
 * Calls: 0x8003F7C8 at JAL 0x800360F4 with NOP delay 0x800360F8 and return address 0x800360FC.
 * Original quirks: The initialization flag is written before ra is saved; the callback may relocate sp or mutate the saved-ra word; partial failures retain the exact completed access/call prefix.
 * Native mapping: Guest addresses remain uint32_t values over validated retained regions with per-byte knownness; the recovered dispatcher is composed through a typed adapter and no guest integer is cast to a host pointer.
 */
int nba97_frontend_dispatch_entry(Nba97FrontendDispatchEntryContext *,
                                  Nba97FrontendDispatchEntryProgress *);

#ifdef __cplusplus
}
#endif
#endif
