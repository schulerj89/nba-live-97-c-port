#ifndef NBA97_FRONTEND_IO_DRAIN_H
#define NBA97_FRONTEND_IO_DRAIN_H

#include "frontend_exit_drain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendExitDrainWord Nba97FrontendIoDrainWord;
typedef Nba97FrontendExitDrainMachine Nba97FrontendIoDrainMachine;

enum Nba97FrontendIoDrainRegister {
  NBA97_FRONTEND_IO_DRAIN_ZERO = 0,
  NBA97_FRONTEND_IO_DRAIN_AT = 1,
  NBA97_FRONTEND_IO_DRAIN_V0 = 2,
  NBA97_FRONTEND_IO_DRAIN_V1 = 3,
  NBA97_FRONTEND_IO_DRAIN_A0 = 4,
  NBA97_FRONTEND_IO_DRAIN_S0 = 16,
  NBA97_FRONTEND_IO_DRAIN_S1 = 17,
  NBA97_FRONTEND_IO_DRAIN_SP = 29,
  NBA97_FRONTEND_IO_DRAIN_RA = 31,
  NBA97_FRONTEND_IO_DRAIN_REGISTER_COUNT = 32
};

enum Nba97FrontendIoDrainSite {
  NBA97_FRONTEND_IO_DRAIN_SITE_NONE = 0,
  NBA97_FRONTEND_IO_DRAIN_SITE_80039458,
  NBA97_FRONTEND_IO_DRAIN_SITE_8003949C,
  NBA97_FRONTEND_IO_DRAIN_SITE_800394AC,
  NBA97_FRONTEND_IO_DRAIN_SITE_COUNT
};

enum Nba97FrontendIoDrainAccessKind {
  NBA97_FRONTEND_IO_DRAIN_READ = 1,
  NBA97_FRONTEND_IO_DRAIN_STORE = 2
};

typedef struct Nba97FrontendIoDrainEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendIoDrainEvent;

typedef int (*Nba97FrontendIoDrainIo)(
    void *, const Nba97GameTextMemory *, const Nba97FrontendIoDrainEvent *,
    Nba97FrontendIoDrainMachine *);

typedef struct Nba97FrontendIoDrainAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendIoDrainAccess;

typedef struct Nba97FrontendIoDrainContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendIoDrainMachine machine;
  Nba97FrontendIoDrainIo io;
  void *user;
  Nba97FrontendIoDrainAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendIoDrainContext;

typedef struct Nba97FrontendIoDrainProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t slot_iterations;
  size_t poll_attempts;
  size_t zero_poll_results;
  size_t call_attempts[NBA97_FRONTEND_IO_DRAIN_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_IO_DRAIN_SITE_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendIoDrainWord last_status;
  Nba97FrontendIoDrainWord last_slot_offset;
  Nba97FrontendIoDrainWord saved_s1;
  Nba97FrontendIoDrainWord saved_s0;
  Nba97FrontendIoDrainWord saved_return_address;
  Nba97FrontendIoDrainWord restored_s1;
  Nba97FrontendIoDrainWord restored_s0;
  Nba97FrontendIoDrainWord restored_return_address;
  Nba97FrontendIoDrainMachine machine;
  uint8_t completed;
} Nba97FrontendIoDrainProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x800393F0
 * Range: 0x800393F0..0x800394D3 (inclusive)
 * Source size: 228 bytes / 57 instructions
 * Evidence: fresh Ghidra feonly_800393f0_continue.txt and independently hashed FEONLY.BIN range; SHA-256 ddd6a228f2ddfecfebe23641b1c36c549e82172f38dfe659484b2d9e521ea50c
 *
 * Purpose: Release active frontend I/O handles across the signed eight-slot table, clear their state, and wait for the I/O queue to drain.
 * Inputs: No formal arguments; all 32 live MIPS GPRs, HI/LO, retained guest memory, and three typed FEONLY child services.
 * Returns: Restores ra, s1, and s0 through callback-live sp+24, +20, and +16, adds 32 to callback-live sp, and retains all other live machine state.
 * Guest memory: Saves s1/s0/ra in that order in a 32-byte frame; reads slot status words at 0x800EF840+s0; status 3 reads 0x800EF844+s0 before the child then clears pointer and status through callback-live s0; status 1 clears status; signed statuses 4/5 clear 0x800EF830+s0; restores the three saved words in ra/s1/s0 order.
 * Calls: Status-3 handle release 0x80077638(a0=slot pointer); repeated queue poll 0x800392A0; zero-result pump 0x80038E84.
 * Original quirks: Branch 0x80039418 latches equality against old v0=3 before its delay overwrites v0 with signed v1<4; branch 0x80039420 latches that old result before its delay overwrites v0 with 1; callbacks may change s0/s1 and therefore later clear addresses and loop bounds; s0 advances by 36 in the loop branch delay even when the loop exits; both source loops can run indefinitely.
 * Native mapping: Guest addresses remain uint32_t values over validated retained regions with byte knownness; explicit operation limits expose exact slot/poll prefixes, all children remain typed full-machine callbacks, and no guest address is cast to a host pointer.
 */
int nba97_frontend_io_drain(Nba97FrontendIoDrainContext *,
                            Nba97FrontendIoDrainProgress *);

#ifdef __cplusplus
}
#endif
#endif
