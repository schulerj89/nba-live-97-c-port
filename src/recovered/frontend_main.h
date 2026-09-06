#ifndef NBA97_FRONTEND_MAIN_H
#define NBA97_FRONTEND_MAIN_H

#include "frontend_dispatch_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendDispatchEntryWord Nba97FrontendMainWord;
typedef Nba97FrontendDispatchEntryMachine Nba97FrontendMainMachine;

enum Nba97FrontendMainRegister {
  NBA97_FRONTEND_MAIN_ZERO = 0,
  NBA97_FRONTEND_MAIN_AT = 1,
  NBA97_FRONTEND_MAIN_V0 = 2,
  NBA97_FRONTEND_MAIN_V1 = 3,
  NBA97_FRONTEND_MAIN_A0 = 4,
  NBA97_FRONTEND_MAIN_A1 = 5,
  NBA97_FRONTEND_MAIN_A2 = 6,
  NBA97_FRONTEND_MAIN_A3 = 7,
  NBA97_FRONTEND_MAIN_S0 = 16,
  NBA97_FRONTEND_MAIN_S1 = 17,
  NBA97_FRONTEND_MAIN_S2 = 18,
  NBA97_FRONTEND_MAIN_SP = 29,
  NBA97_FRONTEND_MAIN_RA = 31,
  NBA97_FRONTEND_MAIN_REGISTER_COUNT = 32
};

enum Nba97FrontendMainProgram {
  NBA97_FRONTEND_MAIN_PROGRAM_FEONLY = 1,
  NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD = 2
};

enum Nba97FrontendMainSite {
  NBA97_FRONTEND_MAIN_SITE_NONE = 0,
  NBA97_FRONTEND_MAIN_SITE_80028810,
  NBA97_FRONTEND_MAIN_SITE_80028818,
  NBA97_FRONTEND_MAIN_SITE_80028834,
  NBA97_FRONTEND_MAIN_SITE_80028858,
  NBA97_FRONTEND_MAIN_SITE_80028880,
  NBA97_FRONTEND_MAIN_SITE_80028898,
  NBA97_FRONTEND_MAIN_SITE_800288A8,
  NBA97_FRONTEND_MAIN_SITE_800288B8,
  NBA97_FRONTEND_MAIN_SITE_800288C0,
  NBA97_FRONTEND_MAIN_SITE_800288C8,
  NBA97_FRONTEND_MAIN_SITE_800288D0,
  NBA97_FRONTEND_MAIN_SITE_800288D8,
  NBA97_FRONTEND_MAIN_SITE_800288EC,
  NBA97_FRONTEND_MAIN_SITE_800288F4,
  NBA97_FRONTEND_MAIN_SITE_800288FC,
  NBA97_FRONTEND_MAIN_SITE_80028904,
  NBA97_FRONTEND_MAIN_SITE_8002890C,
  NBA97_FRONTEND_MAIN_SITE_80028934,
  NBA97_FRONTEND_MAIN_SITE_8002893C,
  NBA97_FRONTEND_MAIN_SITE_8002894C,
  NBA97_FRONTEND_MAIN_SITE_80028954,
  NBA97_FRONTEND_MAIN_SITE_8002895C,
  NBA97_FRONTEND_MAIN_SITE_80028974,
  NBA97_FRONTEND_MAIN_SITE_800289F4,
  NBA97_FRONTEND_MAIN_SITE_800289FC,
  NBA97_FRONTEND_MAIN_SITE_80028A04,
  NBA97_FRONTEND_MAIN_SITE_80028A0C,
  NBA97_FRONTEND_MAIN_SITE_80028A14,
  NBA97_FRONTEND_MAIN_SITE_80028A48,
  NBA97_FRONTEND_MAIN_SITE_80028A50,
  NBA97_FRONTEND_MAIN_SITE_80028A58,
  NBA97_FRONTEND_MAIN_SITE_80028A60,
  NBA97_FRONTEND_MAIN_SITE_80028A7C,
  NBA97_FRONTEND_MAIN_SITE_80028A90,
  NBA97_FRONTEND_MAIN_SITE_80028AA0,
  NBA97_FRONTEND_MAIN_SITE_80028AA8,
  NBA97_FRONTEND_MAIN_SITE_80028AB0,
  NBA97_FRONTEND_MAIN_SITE_80028ACC,
  NBA97_FRONTEND_MAIN_SITE_80028AD8,
  NBA97_FRONTEND_MAIN_SITE_80028AF0,
  NBA97_FRONTEND_MAIN_SITE_80028AF8,
  NBA97_FRONTEND_MAIN_SITE_80028B00,
  NBA97_FRONTEND_MAIN_SITE_80028B08,
  NBA97_FRONTEND_MAIN_SITE_80028B1C,
  NBA97_FRONTEND_MAIN_SITE_80028B24,
  NBA97_FRONTEND_MAIN_SITE_80028B2C,
  NBA97_FRONTEND_MAIN_SITE_80028B34,
  NBA97_FRONTEND_MAIN_SITE_80028B44,
  NBA97_FRONTEND_MAIN_SITE_80028B54,
  NBA97_FRONTEND_MAIN_SITE_80028B68,
  NBA97_FRONTEND_MAIN_SITE_COUNT
};

typedef enum Nba97FrontendMainCalleeOutcome {
  NBA97_FRONTEND_MAIN_CALLEE_RETURNED = 1,
  NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED = 2
} Nba97FrontendMainCalleeOutcome;

typedef struct Nba97FrontendMainEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendMainEvent;

typedef int (*Nba97FrontendMainIo)(
    void *, const Nba97GameTextMemory *, const Nba97FrontendMainEvent *,
    Nba97FrontendMainMachine *, Nba97FrontendMainCalleeOutcome *);

enum Nba97FrontendMainAccessKind {
  NBA97_FRONTEND_MAIN_READ = 1,
  NBA97_FRONTEND_MAIN_STORE = 2
};

typedef struct Nba97FrontendMainAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendMainAccess;

typedef struct Nba97FrontendMainContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendMainMachine machine;
  Nba97FrontendMainIo io;
  void *user;
  Nba97FrontendMainAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendMainContext;

typedef struct Nba97FrontendMainProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t call_attempts[NBA97_FRONTEND_MAIN_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_MAIN_SITE_COUNT];
  size_t intro_iterations;
  size_t wait_iterations;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendMainWord loaded_initial_frontend_flag;
  Nba97FrontendMainWord loaded_menu_frontend_flag;
  Nba97FrontendMainWord loaded_intro_flag;
  Nba97FrontendMainWord loaded_context_selector;
  Nba97FrontendMainWord gameload_handle;
  Nba97FrontendMainWord gameload_size;
  Nba97FrontendMainWord dynamic_entry;
  Nba97FrontendMainWord saved_return_address;
  Nba97FrontendMainWord restored_return_address;
  Nba97FrontendMainWord restored_s0;
  Nba97FrontendMainWord restored_s1;
  Nba97FrontendMainWord restored_s2;
  Nba97FrontendMainMachine machine;
  uint8_t completed;
  uint8_t transferred;
} Nba97FrontendMainProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x80028800
 * Range: 0x80028800..0x80028B8B (inclusive)
 * Source size: 908 bytes / 227 instructions
 * Evidence: fresh Ghidra feonly_80028800_resume.txt and independently hashed FEONLY.BIN range; SHA-256 a9325ac1de6cf8da7bd5a43d95da2f2e61bfc586cd3f265824d3e519cb42b208
 *
 * Purpose: Run the complete frontend initialization, menu lifetime, teardown, GAMELOAD load/copy, and dynamic transfer sequence.
 * Inputs: No formal arguments; all 32 live MIPS GPRs, HI/LO, retained guest memory, and typed FEONLY/GAMELOAD child services.
 * Returns: A nonreturning GAMELOAD transfer completes at 0x80028B68; if it returns, ra/s2/s1/s0 are restored through callback-live sp, sp is raised by 40, and the routine returns through live ra.
 * Guest memory: Uses the 40-byte live stack frame; reads frontend/context/count globals and GAMELOAD entry; writes stack geometry, frontend pointers, flags, and teardown state in exact source order.
 * Calls: 49 static FEONLY call sites from 0x80028810 through 0x80028B54, including recovered 0x800360D4 at 0x80028AA0, then one GAMELOAD dynamic JALR at 0x80028B68.
 * Original quirks: Frontend and intro flags are independently reloaded; intro and teardown loops trust callback-live signed counters; GAMELOAD identity comes from the loaded 0x801E0000 word; partial failures retain the exact completed prefix.
 * Native mapping: Guest addresses remain uint32_t values over validated retained regions with per-byte knownness; the recovered initialized-dispatch wrapper is composed through a typed adapter and other services remain typed callbacks.
 */
int nba97_frontend_main(Nba97FrontendMainContext *,
                        Nba97FrontendMainProgress *);

#ifdef __cplusplus
}
#endif
#endif
