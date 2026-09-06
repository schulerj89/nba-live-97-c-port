#ifndef NBA97_FRONTEND_DISPATCH_H
#define NBA97_FRONTEND_DISPATCH_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendDispatchWord {
  uint32_t word;
  uint8_t known_mask;
} Nba97FrontendDispatchWord;

typedef struct Nba97FrontendDispatchRegisters {
  Nba97FrontendDispatchWord gpr[32];
} Nba97FrontendDispatchRegisters;

typedef struct Nba97FrontendDispatchMachine {
  Nba97FrontendDispatchRegisters registers;
  Nba97FrontendDispatchWord hi;
  Nba97FrontendDispatchWord lo;
} Nba97FrontendDispatchMachine;

enum Nba97FrontendDispatchSite {
  NBA97_FRONTEND_DISPATCH_SITE_NONE = 0,
  NBA97_FRONTEND_DISPATCH_SITE_8003F8C8,
  NBA97_FRONTEND_DISPATCH_SITE_8003F8DC,
  NBA97_FRONTEND_DISPATCH_SITE_8003F8F4,
  NBA97_FRONTEND_DISPATCH_SITE_8003F8FC,
  NBA97_FRONTEND_DISPATCH_SITE_8003F92C,
  NBA97_FRONTEND_DISPATCH_SITE_8003F97C,
  NBA97_FRONTEND_DISPATCH_SITE_8003FA08,
  NBA97_FRONTEND_DISPATCH_SITE_8003FA3C,
  NBA97_FRONTEND_DISPATCH_SITE_8003FBD8,
  NBA97_FRONTEND_DISPATCH_SITE_8003FC58,
  NBA97_FRONTEND_DISPATCH_SITE_8003FC68,
  NBA97_FRONTEND_DISPATCH_SITE_8003FC78,
  NBA97_FRONTEND_DISPATCH_SITE_8003FCA8,
  NBA97_FRONTEND_DISPATCH_SITE_8003FCDC,
  NBA97_FRONTEND_DISPATCH_SITE_8003FCF4,
  NBA97_FRONTEND_DISPATCH_SITE_8003FD10,
  NBA97_FRONTEND_DISPATCH_SITE_8003FD3C,
  NBA97_FRONTEND_DISPATCH_SITE_8003FD44,
  NBA97_FRONTEND_DISPATCH_SITE_8003FD4C,
  NBA97_FRONTEND_DISPATCH_SITE_8003FD74,
  NBA97_FRONTEND_DISPATCH_SITE_8003FD7C,
  NBA97_FRONTEND_DISPATCH_SITE_8003FE14,
  NBA97_FRONTEND_DISPATCH_SITE_8003FE58,
  NBA97_FRONTEND_DISPATCH_SITE_8003FE98,
  NBA97_FRONTEND_DISPATCH_SITE_8003FED8,
  NBA97_FRONTEND_DISPATCH_SITE_8003FEE0,
  NBA97_FRONTEND_DISPATCH_SITE_8003FF00,
  NBA97_FRONTEND_DISPATCH_SITE_8003FF10,
  NBA97_FRONTEND_DISPATCH_SITE_8003FF8C,
  NBA97_FRONTEND_DISPATCH_SITE_8004005C,
  NBA97_FRONTEND_DISPATCH_SITE_8004006C,
  NBA97_FRONTEND_DISPATCH_SITE_8004009C,
  NBA97_FRONTEND_DISPATCH_SITE_800400AC,
  NBA97_FRONTEND_DISPATCH_SITE_800400F4,
  NBA97_FRONTEND_DISPATCH_SITE_80040128,
  NBA97_FRONTEND_DISPATCH_SITE_80040158,
  NBA97_FRONTEND_DISPATCH_SITE_80040184,
  NBA97_FRONTEND_DISPATCH_SITE_80040194,
  NBA97_FRONTEND_DISPATCH_SITE_800401C4,
  NBA97_FRONTEND_DISPATCH_SITE_800401FC,
  NBA97_FRONTEND_DISPATCH_SITE_8004028C,
  NBA97_FRONTEND_DISPATCH_SITE_800402D8,
  NBA97_FRONTEND_DISPATCH_SITE_800402E8,
  NBA97_FRONTEND_DISPATCH_SITE_80040350,
  NBA97_FRONTEND_DISPATCH_SITE_80040360,
  NBA97_FRONTEND_DISPATCH_SITE_80040370,
  NBA97_FRONTEND_DISPATCH_SITE_80040380,
  NBA97_FRONTEND_DISPATCH_SITE_80040390,
  NBA97_FRONTEND_DISPATCH_SITE_80040398,
  NBA97_FRONTEND_DISPATCH_SITE_800403D4,
  NBA97_FRONTEND_DISPATCH_SITE_800403DC,
  NBA97_FRONTEND_DISPATCH_SITE_80040410,
  NBA97_FRONTEND_DISPATCH_SITE_80040474,
  NBA97_FRONTEND_DISPATCH_SITE_80040548,
  NBA97_FRONTEND_DISPATCH_SITE_80040558,
  NBA97_FRONTEND_DISPATCH_SITE_800405D8,
  NBA97_FRONTEND_DISPATCH_SITE_80040658,
  NBA97_FRONTEND_DISPATCH_SITE_800406BC,
  NBA97_FRONTEND_DISPATCH_SITE_800406E8,
  NBA97_FRONTEND_DISPATCH_SITE_800406FC,
  NBA97_FRONTEND_DISPATCH_SITE_8004070C,
  NBA97_FRONTEND_DISPATCH_SITE_8004071C,
  NBA97_FRONTEND_DISPATCH_SITE_8004072C,
  NBA97_FRONTEND_DISPATCH_SITE_8004073C,
  NBA97_FRONTEND_DISPATCH_SITE_8004076C,
  NBA97_FRONTEND_DISPATCH_SITE_8004077C,
  NBA97_FRONTEND_DISPATCH_SITE_800407D4,
  NBA97_FRONTEND_DISPATCH_SITE_800407E8,
  NBA97_FRONTEND_DISPATCH_SITE_800407F0,
  NBA97_FRONTEND_DISPATCH_SITE_800407F8,
  NBA97_FRONTEND_DISPATCH_SITE_80040830,
  NBA97_FRONTEND_DISPATCH_SITE_80040850,
  NBA97_FRONTEND_DISPATCH_SITE_80040868,
  NBA97_FRONTEND_DISPATCH_SITE_80040900,
  NBA97_FRONTEND_DISPATCH_SITE_80040964,
  NBA97_FRONTEND_DISPATCH_SITE_800409A8,
  NBA97_FRONTEND_DISPATCH_SITE_800409D0,
  NBA97_FRONTEND_DISPATCH_SITE_800409D8,
  NBA97_FRONTEND_DISPATCH_SITE_800409E0,
  NBA97_FRONTEND_DISPATCH_SITE_COUNT
};

typedef struct Nba97FrontendDispatchEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
} Nba97FrontendDispatchEvent;

typedef int (*Nba97FrontendDispatchIo)(void *, const Nba97GameTextMemory *,
                                       const Nba97FrontendDispatchEvent *,
                                       Nba97FrontendDispatchMachine *);

enum Nba97FrontendDispatchAccessKind {
  NBA97_FRONTEND_DISPATCH_READ = 1,
  NBA97_FRONTEND_DISPATCH_STORE = 2
};

typedef struct Nba97FrontendDispatchAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendDispatchAccess;

typedef struct Nba97FrontendDispatchContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendDispatchMachine machine;
  Nba97FrontendDispatchIo io;
  void *user;
  Nba97FrontendDispatchAccess *access_journal;
  size_t access_journal_capacity;
} Nba97FrontendDispatchContext;

typedef struct Nba97FrontendDispatchProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t callbacks_completed;
  size_t access_events;
  size_t call_attempts[NBA97_FRONTEND_DISPATCH_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_DISPATCH_SITE_COUNT];
  size_t dispatch_iterations;
  size_t backup_iterations;
  size_t restore_iterations;
  size_t roster_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendDispatchMachine machine;
  uint8_t completed;
} Nba97FrontendDispatchProgress;

// clang-format off
/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x8003F7C8
 * Range: 0x8003F7C8..0x80040A1B (inclusive)
 * Source size: 4692 bytes / 1173 instructions
 * Evidence: fresh Ghidra feonly_8003f7c8.txt; raw overlay agreement; instruction SHA-256 a42d7d2d97ab00ad7ddb214677b743dfd5d98d05119f9e6894fd092a6ccf1b9f
 *
 * Purpose: Initialize, run, and unwind the 43-state frontend menu dispatcher, then prepare the selected roster records for the original loader path.
 * Inputs: No formal arguments; full live MIPS machine, retained frontend globals and runtime jump table, callback-live stack, and typed implementations for all 79 original calls.
 * Returns: Restores ra, fp, s7..s0 through callback-live sp and returns after cleanup; caller-saved registers and HI/LO retain exact source and callback state.
 * Guest memory: Reads and writes the live 0x88-byte frame, frontend context at the pointer loaded from 0x800170C0, state stack, globals, runtime dispatch table, roster pointer table, and two 12-record output arrays in exact source order.
 * Calls: Typed targets in source-call order are 0x8003F7B0,0x800770D4,0x80030CDC,0x80030308,0x8003D2A4,0x800459C8,0x80031A88,0x8003F43C,0x8003D930,0x8003D930,0x8002F0E8,0x8004FCD8,0x8004F5F4,0x80041144,0x80037010,0x8003B194,0x80061674,0x80046D24,0x8003E7A8,0x8003F778,0x80044944,0x800435A4,0x800417D4,0x80041DF4,0x8003F778,0x80046354,0x800435A4,0x80057CE4,0x8003F7B0,0x80042288,0x80053F4C,0x8005428C,0x8005460C,0x80056AEC,0x80056CD0,0x80056F9C,0x80057508,0x800592C4,0x8005721C,0x80041A38,0x8005CF78,0x80059220,0x8005BF34,0x800431D4,0x80058A18,0x8005B500,0x8005BC8C,0x8003F778,0x800482F0,0x8004875C,0x800487E0,0x80047618,0x80049C40,0x800435A4,0x80046F80,0x80047194,0x8004DAE8,0x8004E46C,0x8004D514,0x8004E768,0x8004D514,0x8005A880,0x8005A538,0x8005CB2C,0x8005C4E0,0x8005D46C,0x8005D7D4,0x80028B8C,0x800804E8,0x80028B8C,0x800357B0,0x8005851C,0x8005851C,0x800909A8,0x800909A8,0x8004E9D8,0x8004E9D8,0x80029DD0,0x8002FC30; every event also carries its exact call PC, delay PC, argument count and per-site invocation.
 * Original quirks: Runtime JR targets are trusted; signed stack indices and negative team IDs remain unchecked; exhausted or cyclic menu flows are bounded only by the native operation budget; null roster pointers are passed to the copy boundary unchanged.
 * Native mapping: Guest addresses remain uint32_t values over validated retained regions with per-byte knownness; unresolved children are synchronous typed callbacks and no host pointer or fabricated ABI is used.
 */
int nba97_frontend_dispatch(Nba97FrontendDispatchContext *,
                            Nba97FrontendDispatchProgress *);
// clang-format on

#ifdef __cplusplus
}
#endif
#endif
