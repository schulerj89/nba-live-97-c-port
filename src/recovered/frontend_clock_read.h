#ifndef NBA97_FRONTEND_CLOCK_READ_H
#define NBA97_FRONTEND_CLOCK_READ_H

#include "frontend_exit_wait.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendExitWaitWord Nba97FrontendClockReadWord;
typedef Nba97FrontendExitWaitMachine Nba97FrontendClockReadMachine;

enum Nba97FrontendClockReadRegister {
  NBA97_FRONTEND_CLOCK_READ_ZERO = 0,
  NBA97_FRONTEND_CLOCK_READ_V0 = 2,
  NBA97_FRONTEND_CLOCK_READ_RA = 31,
  NBA97_FRONTEND_CLOCK_READ_REGISTER_COUNT = 32
};

enum Nba97FrontendClockReadAccessKind {
  NBA97_FRONTEND_CLOCK_READ_READ = 1
};

typedef struct Nba97FrontendClockReadAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendClockReadAccess;

typedef struct Nba97FrontendClockReadContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendClockReadMachine machine;
  Nba97FrontendClockReadAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendClockReadContext;

typedef struct Nba97FrontendClockReadProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t access_events;
  size_t instruction_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t instruction_count;
  Nba97FrontendClockReadWord loaded_clock;
  Nba97FrontendClockReadMachine machine;
  uint8_t completed;
} Nba97FrontendClockReadProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x8008DA5C
 * Range: 0x8008DA5C..0x8008DA6B (inclusive)
 * Source size: 16 bytes / 4 instructions
 * Evidence: fresh Ghidra feonly_8008da5c_continue.txt and independently hashed FEONLY.BIN range; SHA-256 9bf283cf0c65c4bd13e3e94df28927dc756088764e78bf2e59298f9faeef85c0
 *
 * Purpose: Return the live frontend clock word from guest address 0x800D9AB8.
 * Inputs: No formal arguments; all 32 live MIPS GPRs, HI/LO, ra, and retained guest memory containing 0x800D9AB8.
 * Returns: Replaces v0 with the loaded word and its knownness, preserves every other GPR and HI/LO, and returns through live ra.
 * Guest memory: Reads exactly one little-endian word at 0x800D9AB8; no stores.
 * Calls: None observed.
 * Original quirks: The read is live on every call; partially known v0 may return when ra is known and aligned; read effects precede any JR knownness or alignment fault.
 * Native mapping: The guest address remains a uint32_t value over validated retained regions with per-byte knownness; no guest integer is cast to a host pointer and no host clock is substituted.
 */
int nba97_frontend_clock_read(Nba97FrontendClockReadContext *,
                              Nba97FrontendClockReadProgress *);

#ifdef __cplusplus
}
#endif
#endif
