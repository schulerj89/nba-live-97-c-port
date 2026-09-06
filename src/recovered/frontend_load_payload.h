#ifndef NBA97_FRONTEND_LOAD_PAYLOAD_H
#define NBA97_FRONTEND_LOAD_PAYLOAD_H

#include "frontend_overlay_load.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendOverlayLoadWord Nba97FrontendLoadPayloadWord;
typedef Nba97FrontendOverlayLoadMachine Nba97FrontendLoadPayloadMachine;

enum Nba97FrontendLoadPayloadRegister {
  NBA97_FRONTEND_LOAD_PAYLOAD_ZERO = 0,
  NBA97_FRONTEND_LOAD_PAYLOAD_V0 = 2,
  NBA97_FRONTEND_LOAD_PAYLOAD_A0 = 4,
  NBA97_FRONTEND_LOAD_PAYLOAD_A1 = 5,
  NBA97_FRONTEND_LOAD_PAYLOAD_A2 = 6,
  NBA97_FRONTEND_LOAD_PAYLOAD_SP = 29,
  NBA97_FRONTEND_LOAD_PAYLOAD_RA = 31,
  NBA97_FRONTEND_LOAD_PAYLOAD_REGISTER_COUNT = 32
};

enum Nba97FrontendLoadPayloadSite {
  NBA97_FRONTEND_LOAD_PAYLOAD_SITE_NONE = 0,
  NBA97_FRONTEND_LOAD_PAYLOAD_SITE_8007B164,
  NBA97_FRONTEND_LOAD_PAYLOAD_SITE_COUNT
};

enum Nba97FrontendLoadPayloadAccessKind {
  NBA97_FRONTEND_LOAD_PAYLOAD_READ = 1,
  NBA97_FRONTEND_LOAD_PAYLOAD_STORE = 2
};

typedef struct Nba97FrontendLoadPayloadEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendLoadPayloadEvent;

typedef int (*Nba97FrontendLoadPayloadIo)(
    void *, const Nba97GameTextMemory *, const Nba97FrontendLoadPayloadEvent *,
    Nba97FrontendLoadPayloadMachine *);

typedef struct Nba97FrontendLoadPayloadAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendLoadPayloadAccess;

typedef struct Nba97FrontendLoadPayloadContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendLoadPayloadMachine machine;
  Nba97FrontendLoadPayloadIo io;
  void *user;
  Nba97FrontendLoadPayloadAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendLoadPayloadContext;

typedef struct Nba97FrontendLoadPayloadProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t call_attempts[NBA97_FRONTEND_LOAD_PAYLOAD_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_LOAD_PAYLOAD_SITE_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendLoadPayloadWord forwarded_a0;
  Nba97FrontendLoadPayloadWord forwarded_a1;
  Nba97FrontendLoadPayloadWord forwarded_a2;
  Nba97FrontendLoadPayloadWord child_return;
  Nba97FrontendLoadPayloadWord payload_result;
  Nba97FrontendLoadPayloadWord saved_return_address;
  Nba97FrontendLoadPayloadWord restored_return_address;
  Nba97FrontendLoadPayloadMachine machine;
  uint8_t completed;
} Nba97FrontendLoadPayloadProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x8007B15C
 * Range: 0x8007B15C..0x8007B18F (inclusive)
 * Source size: 52 bytes / 13 instructions
 * Evidence: fresh Ghidra feonly_8007b15c_continue.txt and independently hashed FEONLY.BIN range; SHA-256 aaf6935467d7d6bad48e084fafaf71528d7b8e6ebb23deca4bef4e2f2f9b3ebf; child argument count confirmed by feonly_8007b1d0_continue.txt
 *
 * Purpose: Invoke the frontend payload loader and return the first word of its descriptor, or zero when no descriptor was produced.
 * Inputs: Live a0/a1/a2 forwarded to typed FEONLY child 0x8007B1D0, all 32 live MIPS GPRs, HI/LO, ra, and retained guest stack/descriptor memory.
 * Returns: Fully-known v0=0 for a null child result, otherwise the little-endian word and byte knownness loaded from the returned guest descriptor; callback-live CPU state is retained except restored ra and adjusted sp.
 * Guest memory: Saves entry ra at entry-sp-8, conditionally reads four bytes at child-returned v0, reloads ra from callback-live sp+16, and performs no other accesses.
 * Calls: 0x8007B1D0(a0,a1,a2).
 * Original quirks: The branch latches child v0 before its NOP delay; a partially known nonzero pointer can establish the branch but still fail address resolution; callback-live sp selects the restore frame.
 * Native mapping: Guest stack and descriptor addresses remain validated uint32_t retained-memory values with byte knownness; the unresolved loader is a typed full-machine callback and no descriptor contents are fabricated.
 */
int nba97_frontend_load_payload(Nba97FrontendLoadPayloadContext *,
                                Nba97FrontendLoadPayloadProgress *);

#ifdef __cplusplus
}
#endif
#endif
