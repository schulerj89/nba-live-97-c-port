#ifndef NBA97_FRONTEND_OVERLAY_LOAD_H
#define NBA97_FRONTEND_OVERLAY_LOAD_H

#include "frontend_main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendMainWord Nba97FrontendOverlayLoadWord;
typedef Nba97FrontendMainMachine Nba97FrontendOverlayLoadMachine;

enum Nba97FrontendOverlayLoadRegister {
  NBA97_FRONTEND_OVERLAY_LOAD_ZERO = 0,
  NBA97_FRONTEND_OVERLAY_LOAD_V0 = 2,
  NBA97_FRONTEND_OVERLAY_LOAD_A0 = 4,
  NBA97_FRONTEND_OVERLAY_LOAD_A1 = 5,
  NBA97_FRONTEND_OVERLAY_LOAD_A2 = 6,
  NBA97_FRONTEND_OVERLAY_LOAD_SP = 29,
  NBA97_FRONTEND_OVERLAY_LOAD_RA = 31,
  NBA97_FRONTEND_OVERLAY_LOAD_REGISTER_COUNT = 32
};

enum Nba97FrontendOverlayLoadSite {
  NBA97_FRONTEND_OVERLAY_LOAD_SITE_NONE = 0,
  NBA97_FRONTEND_OVERLAY_LOAD_SITE_8007B124,
  NBA97_FRONTEND_OVERLAY_LOAD_SITE_COUNT
};

enum Nba97FrontendOverlayLoadAccessKind {
  NBA97_FRONTEND_OVERLAY_LOAD_READ = 1,
  NBA97_FRONTEND_OVERLAY_LOAD_STORE = 2
};

typedef struct Nba97FrontendOverlayLoadEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendOverlayLoadEvent;

typedef int (*Nba97FrontendOverlayLoadIo)(
    void *, const Nba97GameTextMemory *, const Nba97FrontendOverlayLoadEvent *,
    Nba97FrontendOverlayLoadMachine *);

typedef struct Nba97FrontendOverlayLoadAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendOverlayLoadAccess;

typedef struct Nba97FrontendOverlayLoadContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendOverlayLoadMachine machine;
  Nba97FrontendOverlayLoadIo io;
  void *user;
  Nba97FrontendOverlayLoadAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendOverlayLoadContext;

typedef struct Nba97FrontendOverlayLoadProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t call_attempts[NBA97_FRONTEND_OVERLAY_LOAD_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_OVERLAY_LOAD_SITE_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendOverlayLoadWord forwarded_a0;
  Nba97FrontendOverlayLoadWord forwarded_a1;
  Nba97FrontendOverlayLoadWord delay_a2;
  Nba97FrontendOverlayLoadWord child_return;
  Nba97FrontendOverlayLoadWord saved_return_address;
  Nba97FrontendOverlayLoadWord restored_return_address;
  Nba97FrontendOverlayLoadMachine machine;
  uint8_t completed;
} Nba97FrontendOverlayLoadProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x8007B11C
 * Range: 0x8007B11C..0x8007B13B (inclusive)
 * Source size: 32 bytes / 8 instructions
 * Evidence: fresh Ghidra feonly_8007b11c_continue.txt and independently hashed FEONLY.BIN range; SHA-256 97d8f0e4eb51bd581d1431e5995abb4ea56b67408568f334d91a8b93e61029e2; transitive arguments confirmed by feonly_8007b15c_continue.txt and feonly_8007b1d0_continue.txt
 *
 * Purpose: Enter the frontend overlay loader with the caller's filename and flags plus source-forced load mode 1.
 * Inputs: a0 filename guest address and a1 flags, with all 32 live MIPS GPRs, HI/LO, retained guest stack memory, and typed FEONLY child 0x8007B15C.
 * Returns: Preserves the child's v0 and live CPU state except that ra is restored through callback-live sp+16 and callback-live sp is raised by 24.
 * Guest memory: Saves entry ra at entry-sp-8, then reloads ra from callback-live sp+16; no filename or loader payload bytes are read by this owner.
 * Calls: 0x8007B15C(a0=caller filename, a1=caller flags, a2=1).
 * Original quirks: The immediate child is annotated as void by Ghidra, but its transitive 0x8007B1D0 callee consumes all three live arguments; a2 is overwritten in the JAL delay; callback-live sp controls the restore.
 * Native mapping: Guest stack addresses remain validated uint32_t retained-memory values with byte knownness; the loader remains a typed full-machine callback and no guest address is cast to a host pointer.
 */
int nba97_frontend_overlay_load(Nba97FrontendOverlayLoadContext *,
                                Nba97FrontendOverlayLoadProgress *);

#ifdef __cplusplus
}
#endif
#endif
