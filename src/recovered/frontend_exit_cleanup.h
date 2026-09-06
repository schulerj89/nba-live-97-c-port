#ifndef NBA97_FRONTEND_EXIT_CLEANUP_H
#define NBA97_FRONTEND_EXIT_CLEANUP_H

#include "frontend_main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendMainWord Nba97FrontendExitCleanupWord;
typedef Nba97FrontendMainMachine Nba97FrontendExitCleanupMachine;

enum Nba97FrontendExitCleanupRegister {
  NBA97_FRONTEND_EXIT_CLEANUP_ZERO = 0,
  NBA97_FRONTEND_EXIT_CLEANUP_AT = 1,
  NBA97_FRONTEND_EXIT_CLEANUP_V0 = 2,
  NBA97_FRONTEND_EXIT_CLEANUP_A0 = 4,
  NBA97_FRONTEND_EXIT_CLEANUP_SP = 29,
  NBA97_FRONTEND_EXIT_CLEANUP_RA = 31,
  NBA97_FRONTEND_EXIT_CLEANUP_REGISTER_COUNT = 32
};

enum Nba97FrontendExitCleanupSite {
  NBA97_FRONTEND_EXIT_CLEANUP_SITE_NONE = 0,
  NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F08C,
  NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F094,
  NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0A4,
  NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0C0,
  NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0D0,
  NBA97_FRONTEND_EXIT_CLEANUP_SITE_COUNT
};

enum Nba97FrontendExitCleanupAccessKind {
  NBA97_FRONTEND_EXIT_CLEANUP_READ = 1,
  NBA97_FRONTEND_EXIT_CLEANUP_STORE = 2
};

typedef struct Nba97FrontendExitCleanupEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendExitCleanupEvent;

typedef int (*Nba97FrontendExitCleanupIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97FrontendExitCleanupEvent *, Nba97FrontendExitCleanupMachine *);

typedef struct Nba97FrontendExitCleanupAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendExitCleanupAccess;

typedef struct Nba97FrontendExitCleanupContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendExitCleanupMachine machine;
  Nba97FrontendExitCleanupIo io;
  void *user;
  Nba97FrontendExitCleanupAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendExitCleanupContext;

typedef struct Nba97FrontendExitCleanupProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t call_attempts[NBA97_FRONTEND_EXIT_CLEANUP_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_EXIT_CLEANUP_SITE_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendExitCleanupWord loaded_cleanup_selector;
  Nba97FrontendExitCleanupWord loaded_release_flag;
  Nba97FrontendExitCleanupWord saved_return_address;
  Nba97FrontendExitCleanupWord restored_return_address;
  Nba97FrontendExitCleanupMachine machine;
  uint8_t completed;
} Nba97FrontendExitCleanupProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x8002F084
 * Range: 0x8002F084..0x8002F0E7 (inclusive)
 * Source size: 100 bytes / 25 instructions
 * Evidence: fresh Ghidra feonly_8002f084_resume.txt and independently hashed FEONLY.BIN range; SHA-256 38b3b7e879958bf82f3c214dea99f4b4bdb0c69f77eae68ae32692c7c9da29ec
 *
 * Purpose: Run the frontend exit services, optionally release the live resource, clear its global, and finish teardown.
 * Inputs: No formal arguments; all 32 live MIPS GPRs, HI/LO, retained guest memory, and five typed FEONLY child services.
 * Returns: Restores ra through callback-live sp+16, adds 24 to callback-live sp, and otherwise preserves the last child-returned full machine.
 * Guest memory: Saves ra at entry-sp-8, loads the raw signed selector word at 0x80021D6C then the resource pointer at 0x8001502C, conditionally stores zero to 0x8001502C, and reloads ra from callback-live sp+16 in source order.
 * Calls: 0x8002EFBC, 0x800394D4, 0x80028C90(a0=raw signed selector word 0x80021D6C), conditional 0x8007760C(a0=resource pointer word 0x8001502C), then 0x80076540.
 * Original quirks: The release flag is reloaded after three callbacks; callback-live sp controls the final restore; aliasing the conditional global store with the saved-ra word can change the eventual return address.
 * Native mapping: Guest addresses remain uint32_t values over validated retained regions with per-byte knownness; all children remain typed callbacks and no guest integer is cast to a host pointer.
 */
int nba97_frontend_exit_cleanup(Nba97FrontendExitCleanupContext *,
                                Nba97FrontendExitCleanupProgress *);

#ifdef __cplusplus
}
#endif
#endif
