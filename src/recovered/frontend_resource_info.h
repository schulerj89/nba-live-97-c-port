#ifndef NBA97_FRONTEND_RESOURCE_INFO_H
#define NBA97_FRONTEND_RESOURCE_INFO_H

#include "frontend_resource_load.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97FrontendResourceLoadWord Nba97FrontendResourceInfoWord;
typedef Nba97FrontendResourceLoadMachine Nba97FrontendResourceInfoMachine;

enum Nba97FrontendResourceInfoRegister {
  NBA97_FRONTEND_RESOURCE_INFO_ZERO = 0,
  NBA97_FRONTEND_RESOURCE_INFO_AT = 1,
  NBA97_FRONTEND_RESOURCE_INFO_V0 = 2,
  NBA97_FRONTEND_RESOURCE_INFO_A0 = 4,
  NBA97_FRONTEND_RESOURCE_INFO_A1 = 5,
  NBA97_FRONTEND_RESOURCE_INFO_A2 = 6,
  NBA97_FRONTEND_RESOURCE_INFO_A3 = 7,
  NBA97_FRONTEND_RESOURCE_INFO_S0 = 16,
  NBA97_FRONTEND_RESOURCE_INFO_S1 = 17,
  NBA97_FRONTEND_RESOURCE_INFO_S2 = 18,
  NBA97_FRONTEND_RESOURCE_INFO_S3 = 19,
  NBA97_FRONTEND_RESOURCE_INFO_S4 = 20,
  NBA97_FRONTEND_RESOURCE_INFO_S5 = 21,
  NBA97_FRONTEND_RESOURCE_INFO_S6 = 22,
  NBA97_FRONTEND_RESOURCE_INFO_S7 = 23,
  NBA97_FRONTEND_RESOURCE_INFO_SP = 29,
  NBA97_FRONTEND_RESOURCE_INFO_RA = 31,
  NBA97_FRONTEND_RESOURCE_INFO_REGISTER_COUNT = 32
};

enum Nba97FrontendResourceInfoSite {
  NBA97_FRONTEND_RESOURCE_INFO_SITE_NONE = 0,
  NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A5E8,
  NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A610,
  NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A63C,
  NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A648,
  NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A658,
  NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A66C,
  NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A698,
  NBA97_FRONTEND_RESOURCE_INFO_SITE_COUNT
};

enum Nba97FrontendResourceInfoAccessKind {
  NBA97_FRONTEND_RESOURCE_INFO_READ = 1,
  NBA97_FRONTEND_RESOURCE_INFO_STORE = 2
};

typedef struct Nba97FrontendResourceInfoEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendResourceInfoEvent;

typedef int (*Nba97FrontendResourceInfoIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97FrontendResourceInfoEvent *, Nba97FrontendResourceInfoMachine *);

typedef struct Nba97FrontendResourceInfoAccess {
  uint32_t pc;
  uint32_t address;
  uint32_t value;
  size_t operation;
  uint8_t width;
  uint8_t known_mask;
  uint8_t kind;
} Nba97FrontendResourceInfoAccess;

typedef struct Nba97FrontendResourceInfoContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendResourceInfoMachine machine;
  Nba97FrontendResourceInfoIo io;
  void *user;
  Nba97FrontendResourceInfoAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendResourceInfoContext;

typedef struct Nba97FrontendResourceInfoProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  size_t callbacks_completed;
  size_t instruction_events;
  size_t attempts_started;
  size_t failed_attempts;
  size_t call_attempts[NBA97_FRONTEND_RESOURCE_INFO_SITE_COUNT];
  size_t call_count[NBA97_FRONTEND_RESOURCE_INFO_SITE_COUNT];
  uint32_t stopped_pc;
  uint32_t stopped_address;
  uint32_t stopped_target;
  uint32_t frame_stack_pointer;
  uint32_t instruction_count;
  Nba97FrontendResourceInfoWord input_filename;
  Nba97FrontendResourceInfoWord input_handle_pointer;
  Nba97FrontendResourceInfoWord input_other_pointer;
  Nba97FrontendResourceInfoWord input_size_pointer;
  Nba97FrontendResourceInfoWord input_fifth_argument;
  Nba97FrontendResourceInfoWord prefix_result;
  Nba97FrontendResourceInfoWord open_result;
  Nba97FrontendResourceInfoWord info_result;
  Nba97FrontendResourceInfoWord seek_result;
  Nba97FrontendResourceInfoWord published_size;
  Nba97FrontendResourceInfoWord saved_return_address;
  Nba97FrontendResourceInfoWord saved_s0;
  Nba97FrontendResourceInfoWord saved_s1;
  Nba97FrontendResourceInfoWord saved_s2;
  Nba97FrontendResourceInfoWord saved_s3;
  Nba97FrontendResourceInfoWord saved_s4;
  Nba97FrontendResourceInfoWord saved_s5;
  Nba97FrontendResourceInfoWord saved_s6;
  Nba97FrontendResourceInfoWord saved_s7;
  Nba97FrontendResourceInfoWord restored_return_address;
  Nba97FrontendResourceInfoWord restored_s0;
  Nba97FrontendResourceInfoWord restored_s1;
  Nba97FrontendResourceInfoWord restored_s2;
  Nba97FrontendResourceInfoWord restored_s3;
  Nba97FrontendResourceInfoWord restored_s4;
  Nba97FrontendResourceInfoWord restored_s5;
  Nba97FrontendResourceInfoWord restored_s6;
  Nba97FrontendResourceInfoWord restored_s7;
  Nba97FrontendResourceInfoMachine machine;
  uint8_t delegated_path;
  uint8_t direct_path_success;
  uint8_t completed;
} Nba97FrontendResourceInfoProgress;

/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x8008A594
 * Range: 0x8008A594..0x8008A6EB (inclusive)
 * Source size: 344 bytes / 86 instructions
 * Evidence: fresh Ghidra feonly_8008a594_continue.txt and independently hashed FEONLY.BIN range; SHA-256 494529aeb56f769fbc5f40e3792f83492ad9368f40e6672ce2f4359a6d0a887a; direct-child listings named in the workflow
 *
 * Purpose: Resolve a frontend resource through the delegated path or repeatedly format, open, inspect, and seek it while publishing its handle and size.
 * Inputs: a0 filename, a1 handle-output pointer, a2 secondary-output pointer, a3 size-output pointer, fifth argument at entry sp+16, all 32 live GPRs, HI/LO, retained guest memory, and seven typed FEONLY child services.
 * Returns: No defined scalar return; preserves callback-live state except source register effects, restores s7..s0 and ra through callback-live sp, and raises that sp by 352.
 * Guest memory: Saves s5/s0/s6/s4/s1/s2/s7/ra/s3 at frame offsets 332/312/336/328/316/320/340/344/324, reads the fifth argument at frame+368, uses frame+24 as a formatted path, writes delegated stack arguments at +16/+20 (the first points to +304), clears three caller outputs before every direct attempt, stores every open result, reloads the handle before seek/close, and finally stores s1 through s4.
 * Calls: 0x80084910(a0..a2), delegated 0x80074184(a0..a3 plus frame+16/+20), repeated 0x80083B70(a0..a3), 0x8007F588(a0,a1), conditional 0x8008A408(a0), conditional 0x8007F318(a0..a2), and optional 0x8008A7B0(a0).
 * Original quirks: Direct attempts are capped by mutable s2 initialized to ten; the open-result store and retry decrement are branch-delay effects; s1 and s3 can remain callback-mutated or stale when open fails; a successful nonzero info result with zero seek result publishes s1; all later frame/output accesses use callback-live registers.
 * Native mapping: Guest addresses remain validated uint32_t retained-memory values with byte knownness; every unresolved child receives the complete mutable machine and no guest address is cast to a host pointer.
 */
int nba97_frontend_resource_info(Nba97FrontendResourceInfoContext *,
                                 Nba97FrontendResourceInfoProgress *);

#ifdef __cplusplus
}
#endif
#endif
