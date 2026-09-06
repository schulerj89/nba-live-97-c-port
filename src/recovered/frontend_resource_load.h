#ifndef NBA97_FRONTEND_RESOURCE_LOAD_H
#define NBA97_FRONTEND_RESOURCE_LOAD_H

#include "frontend_overlay_load.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendOverlayLoadWord Nba97FrontendResourceLoadWord;
typedef Nba97FrontendOverlayLoadMachine Nba97FrontendResourceLoadMachine;

enum Nba97FrontendResourceLoadRegister {
  NBA97_FRONTEND_RESOURCE_LOAD_ZERO = 0,
  NBA97_FRONTEND_RESOURCE_LOAD_AT = 1,
  NBA97_FRONTEND_RESOURCE_LOAD_V0 = 2,
  NBA97_FRONTEND_RESOURCE_LOAD_A0 = 4,
  NBA97_FRONTEND_RESOURCE_LOAD_A1 = 5,
  NBA97_FRONTEND_RESOURCE_LOAD_A2 = 6,
  NBA97_FRONTEND_RESOURCE_LOAD_A3 = 7,
  NBA97_FRONTEND_RESOURCE_LOAD_S0 = 16,
  NBA97_FRONTEND_RESOURCE_LOAD_S1 = 17,
  NBA97_FRONTEND_RESOURCE_LOAD_S2 = 18,
  NBA97_FRONTEND_RESOURCE_LOAD_S3 = 19,
  NBA97_FRONTEND_RESOURCE_LOAD_SP = 29,
  NBA97_FRONTEND_RESOURCE_LOAD_RA = 31,
  NBA97_FRONTEND_RESOURCE_LOAD_REGISTER_COUNT = 32
};

enum Nba97FrontendResourceLoadSite {
  NBA97_FRONTEND_RESOURCE_LOAD_SITE_NONE = 0,
  NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B1F0,
  NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B214,
  NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B230,
  NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B250,
  NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B268,
  NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B28C,
  NBA97_FRONTEND_RESOURCE_LOAD_SITE_COUNT
};

enum Nba97FrontendResourceLoadAccessKind {
  NBA97_FRONTEND_RESOURCE_LOAD_READ = 1,
  NBA97_FRONTEND_RESOURCE_LOAD_STORE = 2
};

typedef struct Nba97FrontendResourceLoadEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendResourceLoadEvent;

typedef int (*Nba97FrontendResourceLoadIo)(
    void *, const Nba97GameTextMemory *, const Nba97FrontendResourceLoadEvent *,
    Nba97FrontendResourceLoadMachine *);

typedef struct Nba97FrontendResourceLoadAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendResourceLoadAccess;

typedef struct Nba97FrontendResourceLoadContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendResourceLoadMachine machine;
  Nba97FrontendResourceLoadIo io;
  void *user;
  Nba97FrontendResourceLoadAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendResourceLoadContext;

typedef struct Nba97FrontendResourceLoadProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t call_attempts[NBA97_FRONTEND_RESOURCE_LOAD_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_RESOURCE_LOAD_SITE_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendResourceLoadWord input_filename;
  Nba97FrontendResourceLoadWord input_flags;
  Nba97FrontendResourceLoadWord input_mode;
  Nba97FrontendResourceLoadWord cached_lookup_result;
  Nba97FrontendResourceLoadWord file_size;
  Nba97FrontendResourceLoadWord allocation_result;
  Nba97FrontendResourceLoadWord descriptor_word;
  Nba97FrontendResourceLoadWord callback_pointer;
  Nba97FrontendResourceLoadWord dynamic_return;
  Nba97FrontendResourceLoadWord saved_return_address;
  Nba97FrontendResourceLoadWord saved_s0;
  Nba97FrontendResourceLoadWord saved_s1;
  Nba97FrontendResourceLoadWord saved_s2;
  Nba97FrontendResourceLoadWord saved_s3;
  Nba97FrontendResourceLoadWord restored_return_address;
  Nba97FrontendResourceLoadWord restored_s0;
  Nba97FrontendResourceLoadWord restored_s1;
  Nba97FrontendResourceLoadWord restored_s2;
  Nba97FrontendResourceLoadWord restored_s3;
  Nba97FrontendResourceLoadMachine machine;
  uint8_t cached_result_discarded;
  uint8_t completed;
} Nba97FrontendResourceLoadProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x8007B1D0
 * Range: 0x8007B1D0..0x8007B2BB (inclusive)
 * Source size: 236 bytes / 59 instructions
 * Evidence: fresh Ghidra feonly_8007b1d0_continue.txt and independently hashed FEONLY.BIN range; SHA-256 16756cd9554b869085b0f84eb6b2f1b9fe0931e7bb07f40c9f08ce90a3677c26; five direct-callee listings named in the workflow
 *
 * Purpose: Build and load a frontend resource descriptor, publish its size, close it, and optionally notify a dynamic callback.
 * Inputs: a0 filename guest pointer, a1 flags, a2 mode, all 32 live MIPS GPRs, HI/LO, retained guest stack/global/descriptor memory, and six typed FEONLY children.
 * Returns: v0 is the allocated descriptor or optional dynamic callback result; restores ra/s3/s2/s1/s0 through callback-live sp and raises that sp by 64.
 * Guest memory: Uses callback-live sp+16/+24/+28/+32 and saved-register slots +40..+56, reads an allocated descriptor word, stores the reloaded size to 0x800D9AE8, and reloads callback pointer 0x800D9B50.
 * Calls: 0x8008A2C8(a0), 0x8008A594(a0,a1,a2,a3,stack arg), optional 0x80077160(a0..a3), optional 0x8008A810(a0..a2), 0x8008A7B0(a0), and optional dynamic callback(a0..a3).
 * Original quirks: Any nonzero cached lookup result is deliberately cleared; every later stack/global/descriptor read uses callback-live state; the dynamic JALR target is latched before its delay slot.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory values with byte knownness; unresolved services are full-machine callbacks and no guest address is cast to a host pointer.
 */
int nba97_frontend_resource_load(Nba97FrontendResourceLoadContext *,
                                 Nba97FrontendResourceLoadProgress *);

#ifdef __cplusplus
}
#endif
#endif
