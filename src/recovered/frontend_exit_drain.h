#ifndef NBA97_FRONTEND_EXIT_DRAIN_H
#define NBA97_FRONTEND_EXIT_DRAIN_H

#include "frontend_exit_cleanup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendExitCleanupWord Nba97FrontendExitDrainWord;
typedef Nba97FrontendExitCleanupMachine Nba97FrontendExitDrainMachine;

enum Nba97FrontendExitDrainRegister {
  NBA97_FRONTEND_EXIT_DRAIN_ZERO = 0,
  NBA97_FRONTEND_EXIT_DRAIN_AT = 1,
  NBA97_FRONTEND_EXIT_DRAIN_V0 = 2,
  NBA97_FRONTEND_EXIT_DRAIN_A0 = 4,
  NBA97_FRONTEND_EXIT_DRAIN_A1 = 5,
  NBA97_FRONTEND_EXIT_DRAIN_SP = 29,
  NBA97_FRONTEND_EXIT_DRAIN_RA = 31,
  NBA97_FRONTEND_EXIT_DRAIN_REGISTER_COUNT = 32
};

enum Nba97FrontendExitDrainSite {
  NBA97_FRONTEND_EXIT_DRAIN_SITE_NONE = 0,
  NBA97_FRONTEND_EXIT_DRAIN_SITE_800394E8,
  NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0,
  NBA97_FRONTEND_EXIT_DRAIN_SITE_80039500,
  NBA97_FRONTEND_EXIT_DRAIN_SITE_80039530,
  NBA97_FRONTEND_EXIT_DRAIN_SITE_80039538,
  NBA97_FRONTEND_EXIT_DRAIN_SITE_80039554,
  NBA97_FRONTEND_EXIT_DRAIN_SITE_8003955C,
  NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT
};

enum Nba97FrontendExitDrainAccessKind {
  NBA97_FRONTEND_EXIT_DRAIN_READ = 1,
  NBA97_FRONTEND_EXIT_DRAIN_STORE = 2
};

typedef struct Nba97FrontendExitDrainEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendExitDrainEvent;

typedef int (*Nba97FrontendExitDrainIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97FrontendExitDrainEvent *, Nba97FrontendExitDrainMachine *);

typedef struct Nba97FrontendExitDrainAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendExitDrainAccess;

typedef struct Nba97FrontendExitDrainContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendExitDrainMachine machine;
  Nba97FrontendExitDrainIo io;
  void *user;
  Nba97FrontendExitDrainAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendExitDrainContext;

typedef struct Nba97FrontendExitDrainProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t poll_attempts;
  size_t zero_poll_results;
  size_t call_attempts[NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendExitDrainWord initial_active_flag;
  Nba97FrontendExitDrainWord first_mode_flag;
  Nba97FrontendExitDrainWord second_mode_flag;
  Nba97FrontendExitDrainWord saved_return_address;
  Nba97FrontendExitDrainWord restored_return_address;
  Nba97FrontendExitDrainMachine machine;
  uint8_t completed;
} Nba97FrontendExitDrainProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x800394D4
 * Range: 0x800394D4..0x80039573 (inclusive)
 * Source size: 160 bytes / 40 instructions
 * Evidence: fresh Ghidra feonly_800394d4_continue.txt and independently hashed FEONLY.BIN range; SHA-256 5b71620fae4987715d545936b770ac78df30d0b8501120fd3ae5e3abb1a61617
 *
 * Purpose: Drain the active frontend exit operation, clear its flags, and run the conditional completion services.
 * Inputs: No formal arguments; all 32 live MIPS GPRs, HI/LO, retained guest memory, and seven typed FEONLY child services.
 * Returns: Restores ra through callback-live sp+16, adds 24 to callback-live sp, and otherwise preserves the last child-returned full machine.
 * Guest memory: Reads 0x800F84C4 before changing sp; saves ra at entry-sp-8; after a nonzero poll reads 0x8002149C, clears 0x800F43B0 then 0x800F84C4, independently reloads 0x8002149C after 0x8008C274, and finally reloads ra from callback-live sp+16.
 * Calls: 0x800393F0; repeated 0x800392A0 with 0x80038E84 after each zero result; conditional 0x80029B64(a0=0,a1=0); 0x8008C274; then conditional 0x8006CDE4(a0=the independently reloaded 0x8002149C word) and 0x8006AE60.
 * Original quirks: The initial flag load precedes frame allocation; the branch delay always saves ra; polling is unbounded in the source; 0x8002149C is read independently around flag clears and 0x8008C274; callback-live sp controls the final restore.
 * Native mapping: Guest addresses remain uint32_t values over validated retained regions with byte knownness; the operation budget explicitly bounds the source poll loop, all children remain typed full-machine callbacks, and no guest address is cast to a host pointer.
 */
int nba97_frontend_exit_drain(Nba97FrontendExitDrainContext *,
                              Nba97FrontendExitDrainProgress *);

#ifdef __cplusplus
}
#endif
#endif
