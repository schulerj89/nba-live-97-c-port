#ifndef NBA97_FRONTEND_MEMORY_COPY_H
#define NBA97_FRONTEND_MEMORY_COPY_H

#include "frontend_main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendMainWord Nba97FrontendMemoryCopyWord;
typedef Nba97FrontendMainMachine Nba97FrontendMemoryCopyMachine;

enum Nba97FrontendMemoryCopyRegister {
  NBA97_FRONTEND_MEMORY_COPY_ZERO = 0,
  NBA97_FRONTEND_MEMORY_COPY_AT = 1,
  NBA97_FRONTEND_MEMORY_COPY_V0 = 2,
  NBA97_FRONTEND_MEMORY_COPY_A0 = 4,
  NBA97_FRONTEND_MEMORY_COPY_A1 = 5,
  NBA97_FRONTEND_MEMORY_COPY_A2 = 6,
  NBA97_FRONTEND_MEMORY_COPY_A3 = 7,
  NBA97_FRONTEND_MEMORY_COPY_T0 = 8,
  NBA97_FRONTEND_MEMORY_COPY_T1 = 9,
  NBA97_FRONTEND_MEMORY_COPY_T2 = 10,
  NBA97_FRONTEND_MEMORY_COPY_T3 = 11,
  NBA97_FRONTEND_MEMORY_COPY_T4 = 12,
  NBA97_FRONTEND_MEMORY_COPY_T5 = 13,
  NBA97_FRONTEND_MEMORY_COPY_T6 = 14,
  NBA97_FRONTEND_MEMORY_COPY_T7 = 15,
  NBA97_FRONTEND_MEMORY_COPY_RA = 31,
  NBA97_FRONTEND_MEMORY_COPY_REGISTER_COUNT = 32
};

enum Nba97FrontendMemoryCopyAccessKind {
  NBA97_FRONTEND_MEMORY_COPY_READ = 1,
  NBA97_FRONTEND_MEMORY_COPY_STORE = 2
};

typedef struct Nba97FrontendMemoryCopyAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t logical_address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t transfer_mask;
  uint8_t kind;
} Nba97FrontendMemoryCopyAccess;

typedef struct Nba97FrontendMemoryCopyContext {
  Nba97GameTextMemory memory;
  /* Bounds attempted guest-memory instructions, including a refused access. */
  size_t operation_budget;
  Nba97FrontendMemoryCopyMachine machine;
  Nba97FrontendMemoryCopyAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendMemoryCopyContext;

typedef struct Nba97FrontendMemoryCopyProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t bytes_read;
  size_t bytes_stored;
  size_t access_events;
  size_t instruction_events;
  uint32_t instruction_count;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t source;
  uint32_t destination;
  uint32_t requested_length;
  uint32_t working_source;
  uint32_t working_destination;
  uint32_t working_count;
  uint32_t return_v0;
  uint8_t return_v0_known_mask;
  uint8_t backward;
  uint8_t unaligned;
  uint8_t completed;
  uint8_t trapped;
  Nba97FrontendMemoryCopyMachine machine;
} Nba97FrontendMemoryCopyProgress;

enum {
  NBA97_FRONTEND_MEMORY_COPY_ARITHMETIC_TRAP = -6
};

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x800909A8
 * Range: 0x800909A8..0x80090CC7 (inclusive)
 * Source size: 800 bytes / 200 instructions
 * Evidence: fresh Ghidra feonly_800909a8_continue.txt; SHA-256 589207dc7895ba0151f714f53c02c357959170daed411e652ca281ac7216ef4b
 *
 * Purpose: Copy an arbitrary retained-memory span with overlap-safe direction and the original aligned and partial-word access schedule.
 * Inputs: A0 source, A1 destination, A2 byte length, all other live GPR words and byte-known masks, HI/LO, retained guest memory, and a native access budget.
 * Returns: V0 contains source/destination alignment bits; A0/A1/A2, AT, A3 and T0..T7 retain their exact path-dependent final values, RA and HI/LO are preserved, and all 32 GPR words/masks are published.
 * Guest memory: Reads the source and writes the destination in exact LW/SW, LWL/LWR/SWL/SWR, and LB/SB source order through validated little-endian retained regions.
 * Calls: None observed.
 * Original quirks: Signed address comparisons choose direction; the initial OR branch delay always executes; backward endpoint ADD instructions can trap; backward aligned tails still use partial-word pairs; negative lengths can wrap into a vast loop; V0 is alignment bits rather than destination.
 * Native mapping: Guest addresses stay uint32_t values over validated retained regions; byte-knownness and partial-word masks are explicit, and operation_budget reports an exact bounded prefix without a host-pointer cast.
 */
int nba97_frontend_memory_copy(Nba97FrontendMemoryCopyContext *,
                               Nba97FrontendMemoryCopyProgress *);

#ifdef __cplusplus
}
#endif
#endif
