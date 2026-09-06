#ifndef NBA97_FRONTEND_EXIT_WAIT_H
#define NBA97_FRONTEND_EXIT_WAIT_H

#include "frontend_exit_cleanup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendExitCleanupWord Nba97FrontendExitWaitWord;
typedef Nba97FrontendExitCleanupMachine Nba97FrontendExitWaitMachine;

enum Nba97FrontendExitWaitRegister {
  NBA97_FRONTEND_EXIT_WAIT_ZERO = 0,
  NBA97_FRONTEND_EXIT_WAIT_AT = 1,
  NBA97_FRONTEND_EXIT_WAIT_V0 = 2,
  NBA97_FRONTEND_EXIT_WAIT_A0 = 4,
  NBA97_FRONTEND_EXIT_WAIT_A1 = 5,
  NBA97_FRONTEND_EXIT_WAIT_A2 = 6,
  NBA97_FRONTEND_EXIT_WAIT_S0 = 16,
  NBA97_FRONTEND_EXIT_WAIT_SP = 29,
  NBA97_FRONTEND_EXIT_WAIT_RA = 31,
  NBA97_FRONTEND_EXIT_WAIT_REGISTER_COUNT = 32
};

enum Nba97FrontendExitWaitSite {
  NBA97_FRONTEND_EXIT_WAIT_SITE_NONE = 0,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFDC,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFE4,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFF0,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002F000,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002F010,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002F018,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002F034,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002F048,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002F050,
  NBA97_FRONTEND_EXIT_WAIT_SITE_8002F060,
  NBA97_FRONTEND_EXIT_WAIT_SITE_COUNT
};

enum Nba97FrontendExitWaitExitPath {
  NBA97_FRONTEND_EXIT_WAIT_EXIT_NONE = 0,
  NBA97_FRONTEND_EXIT_WAIT_EXIT_SENTINEL = 1,
  NBA97_FRONTEND_EXIT_WAIT_EXIT_POLL_NEGATIVE = 2,
  NBA97_FRONTEND_EXIT_WAIT_EXIT_POLL_NONZERO = 3,
  NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE = 4
};

enum Nba97FrontendExitWaitAccessKind {
  NBA97_FRONTEND_EXIT_WAIT_READ = 1,
  NBA97_FRONTEND_EXIT_WAIT_STORE = 2
};

typedef struct Nba97FrontendExitWaitEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendExitWaitEvent;

typedef int (*Nba97FrontendExitWaitIo)(
    void *, const Nba97GameTextMemory *, const Nba97FrontendExitWaitEvent *,
    Nba97FrontendExitWaitMachine *);

typedef struct Nba97FrontendExitWaitAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendExitWaitAccess;

typedef struct Nba97FrontendExitWaitContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendExitWaitMachine machine;
  Nba97FrontendExitWaitIo io;
  void *user;
  Nba97FrontendExitWaitAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendExitWaitContext;

typedef struct Nba97FrontendExitWaitProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t call_attempts[NBA97_FRONTEND_EXIT_WAIT_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_EXIT_WAIT_SITE_COUNT];
  size_t loop_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendExitWaitWord initial_handle;
  Nba97FrontendExitWaitWord deadline;
  Nba97FrontendExitWaitWord first_poll_result;
  Nba97FrontendExitWaitWord second_poll_result;
  Nba97FrontendExitWaitWord clock_result;
  Nba97FrontendExitWaitWord reloaded_handle;
  Nba97FrontendExitWaitWord secondary_word;
  Nba97FrontendExitWaitWord saved_return_address;
  Nba97FrontendExitWaitWord saved_s0;
  Nba97FrontendExitWaitWord restored_return_address;
  Nba97FrontendExitWaitWord restored_s0;
  Nba97FrontendExitWaitMachine machine;
  uint8_t exit_path;
  uint8_t completed;
} Nba97FrontendExitWaitProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x8002EFBC
 * Range: 0x8002EFBC..0x8002F083 (inclusive)
 * Source size: 200 bytes / 50 instructions
 * Evidence: fresh Ghidra feonly_8002efbc_continue.txt and independently hashed FEONLY.BIN range; SHA-256 45eed4157e3ece4487b1c0c8ea03ed780461937168a8d38e05853200ebf6ad53
 *
 * Purpose: Stop a live frontend handle, poll until failure, completion, or a signed clock deadline, then release associated frontend state.
 * Inputs: No formal arguments; all 32 live MIPS GPRs, HI/LO, retained guest memory, and ten typed FEONLY child services.
 * Returns: Restores ra and s0 through callback-live sp, adds 24 to callback-live sp, and otherwise preserves the final callback/source machine.
 * Guest memory: Loads 0x80017268 before changing sp; saves ra/s0 at frame+20/+16; on non-sentinel paths reloads and stores 0x80017268, loads 0x8002149C, clears 0x8002149C after its callback, then restores ra/s0.
 * Calls: 0x8007B2BC(handle,100,-1), 0x8008DA5C, loop calls 0x8006B6A0/0x8006FCF0/0x80039260/0x8008DA5C, optional 0x80092C34(handle), then 0x80028C28/0x8006FAA0/0x80028CF4(secondary).
 * Original quirks: Both poll branches latch the old v0 before their delay overwrites v0 with -1; deadline arithmetic wraps; signed comparisons use callback-live values; the sentinel branch delay still saves s0.
 * Native mapping: Guest addresses remain uint32_t values over validated retained regions with per-byte knownness; unresolved children remain typed callbacks and no guest integer is cast to a host pointer.
 */
int nba97_frontend_exit_wait(Nba97FrontendExitWaitContext *,
                             Nba97FrontendExitWaitProgress *);

#ifdef __cplusplus
}
#endif
#endif
