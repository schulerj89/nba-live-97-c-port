#ifndef NBA97_FRONTEND_IO_COMPLETE_H
#define NBA97_FRONTEND_IO_COMPLETE_H

#include "frontend_exit_drain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendExitDrainWord Nba97FrontendIoCompleteWord;
typedef Nba97FrontendExitDrainMachine Nba97FrontendIoCompleteMachine;

enum Nba97FrontendIoCompleteRegister {
  NBA97_FRONTEND_IO_COMPLETE_ZERO = 0,
  NBA97_FRONTEND_IO_COMPLETE_AT = 1,
  NBA97_FRONTEND_IO_COMPLETE_V0 = 2,
  NBA97_FRONTEND_IO_COMPLETE_V1 = 3,
  NBA97_FRONTEND_IO_COMPLETE_A0 = 4,
  NBA97_FRONTEND_IO_COMPLETE_RA = 31,
  NBA97_FRONTEND_IO_COMPLETE_REGISTER_COUNT = 32
};

enum Nba97FrontendIoCompleteAccessKind {
  NBA97_FRONTEND_IO_COMPLETE_READ = 1
};

typedef struct Nba97FrontendIoCompleteAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendIoCompleteAccess;

typedef struct Nba97FrontendIoCompleteContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendIoCompleteMachine machine;
  Nba97FrontendIoCompleteAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendIoCompleteContext;

typedef struct Nba97FrontendIoCompleteProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t access_events;
  size_t instruction_events;
  size_t status_reads;
  size_t slots_examined;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t instruction_count;
  Nba97FrontendIoCompleteWord active_word;
  Nba97FrontendIoCompleteWord last_status;
  Nba97FrontendIoCompleteMachine machine;
  uint8_t completed;
} Nba97FrontendIoCompleteProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x800392A0
 * Range: 0x800392A0..0x800392F7 (inclusive)
 * Source size: 88 bytes / 22 instructions
 * Evidence: fresh Ghidra feonly_800392a0_continue.txt and independently hashed FEONLY.BIN range; SHA-256 dca1d4f4bf2b7847a1175abe703ab434c4ed51efccb2af341b536de466f98d7a
 *
 * Purpose: Report whether the active frontend I/O operation and all eight retained status slots are complete.
 * Inputs: No formal arguments; all 32 live MIPS GPRs, HI/LO, ra, active word 0x800F84C4, and status words 0x800EF840 with stride 36.
 * Returns: Fully-known v0=1 when inactive or every status is zero, v0=0 on the first definitely nonzero status, plus exact live at/v1/a0 loop state; all other registers and HI/LO are preserved.
 * Guest memory: Reads 0x800F84C4 first and, only when active, up to eight little-endian words at 0x800EF840+i*36; no stores.
 * Calls: None observed.
 * Original quirks: The active branch clears a0 in its delay slot; a0 increments in the status branch delay even on exit; v1 increments in the loop-branch delay after the eighth zero slot; reads precede any JR ra fault.
 * Native mapping: Guest addresses remain uint32_t values over validated retained regions with byte knownness; no guest value is cast to a host pointer and no host-side poll result is guessed.
 */
int nba97_frontend_io_complete(Nba97FrontendIoCompleteContext *,
                               Nba97FrontendIoCompleteProgress *);

#ifdef __cplusplus
}
#endif
#endif
