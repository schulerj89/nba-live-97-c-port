#ifndef NBA97_FRONTEND_RESOURCE_LOOKUP_H
#define NBA97_FRONTEND_RESOURCE_LOOKUP_H
#include "frontend_resource_load.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef Nba97FrontendResourceLoadWord Nba97FrontendResourceLookupWord;
typedef Nba97FrontendResourceLoadMachine Nba97FrontendResourceLookupMachine;
enum Nba97FrontendResourceLookupRegister {
  NBA97_FRONTEND_RESOURCE_LOOKUP_ZERO = 0,
  NBA97_FRONTEND_RESOURCE_LOOKUP_V0 = 2,
  NBA97_FRONTEND_RESOURCE_LOOKUP_V1 = 3,
  NBA97_FRONTEND_RESOURCE_LOOKUP_A0 = 4,
  NBA97_FRONTEND_RESOURCE_LOOKUP_A1 = 5,
  NBA97_FRONTEND_RESOURCE_LOOKUP_A2 = 6,
  NBA97_FRONTEND_RESOURCE_LOOKUP_A3 = 7,
  NBA97_FRONTEND_RESOURCE_LOOKUP_S0 = 16,
  NBA97_FRONTEND_RESOURCE_LOOKUP_S1 = 17,
  NBA97_FRONTEND_RESOURCE_LOOKUP_S2 = 18,
  NBA97_FRONTEND_RESOURCE_LOOKUP_S3 = 19,
  NBA97_FRONTEND_RESOURCE_LOOKUP_SP = 29,
  NBA97_FRONTEND_RESOURCE_LOOKUP_RA = 31,
  NBA97_FRONTEND_RESOURCE_LOOKUP_REGISTER_COUNT = 32
};
enum Nba97FrontendResourceLookupSite {
  NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_NONE = 0,
  NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A2E0,
  NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A314,
  NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A33C,
  NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A344,
  NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A36C,
  NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A3C4,
  NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A3DC,
  NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_COUNT
};
enum Nba97FrontendResourceLookupAccessKind {
  NBA97_FRONTEND_RESOURCE_LOOKUP_READ = 1,
  NBA97_FRONTEND_RESOURCE_LOOKUP_STORE = 2
};
typedef struct Nba97FrontendResourceLookupEvent {
  uint32_t pc, delay_slot_pc, entry;
  size_t operation, invocation;
  uint8_t site, argument_count, target_program;
} Nba97FrontendResourceLookupEvent;
typedef int (*Nba97FrontendResourceLookupIo)(
    void *, const Nba97GameTextMemory *,
    const Nba97FrontendResourceLookupEvent *,
    Nba97FrontendResourceLookupMachine *);
typedef struct Nba97FrontendResourceLookupAccess {
  uint32_t pc, address, value;
  size_t operation;
  uint8_t width, known_mask, kind;
} Nba97FrontendResourceLookupAccess;
typedef struct Nba97FrontendResourceLookupContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97FrontendResourceLookupMachine machine;
  Nba97FrontendResourceLookupIo io;
  void *user;
  Nba97FrontendResourceLookupAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97FrontendResourceLookupContext;
typedef struct Nba97FrontendResourceLookupProgress {
  size_t operations, accesses, reads, stores, access_events,
      callbacks_completed, instruction_events, chain_attempts;
  size_t call_attempts[NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_COUNT],
      call_count[NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_COUNT];
  uint32_t stopped_pc, stopped_address, stopped_target, frame_stack_pointer,
      instruction_count;
  Nba97FrontendResourceLookupWord input_filename, initial_lookup, initial_flags,
      cleared_flags, allocation_result, chain_root, chain_key, chain_result,
      descriptor_source, descriptor_destination, descriptor_length,
      saved_return_address, saved_s0, saved_s1, saved_s2, saved_s3,
      restored_return_address, restored_s0, restored_s1, restored_s2,
      restored_s3;
  Nba97FrontendResourceLookupMachine machine;
  uint8_t copied, freed, secondary_path, completed;
} Nba97FrontendResourceLookupProgress;
/*
 * PS1 SUBROUTINE
 * Program: FEONLY
 * Address: 0x8008A2C8
 * Range: 0x8008A2C8..0x8008A407 (inclusive)
 * Source size: 320 bytes / 80 instructions
 * Evidence: fresh Ghidra feonly_8008a2c8_continue.txt and raw range SHA-256
 * 2bc268004a25001f37dc4a8df569c9a94b5dea9e5253ab3533dbc18e08df00d1
 *
 * Purpose: Find a cached frontend resource or search its cache chain, allocate
 * a copy when required, and copy descriptor payload bytes. Inputs: a0 filename
 * guest pointer, all 32 live GPRs, HI/LO, retained guest memory, seven typed
 * children, and a bounded operation budget. Returns: v0 is the original cached
 * descriptor, newly allocated descriptor, or zero; restores ra/s3/s2/s1/s0
 * through callback-live sp and raises sp by 40. Guest memory: Saves five frame
 * words; reads and clears descriptor flags, follows cache roots/links, reads
 * descriptor fields, and delegates payload copying at both source call sites.
 * Calls: 0x8008A0A8, 0x800771F0 twice, recovered 0x800909A8 twice, 0x80077638,
 * and repeated 0x80089FFC. Original quirks: Old bit 0x10 returns the original
 * descriptor; cache-chain misses loop; the secondary allocation result is
 * dereferenced without a null guard. Native mapping: Guest addresses use
 * validated retained memory with byte knownness; unresolved services are
 * full-machine callbacks and both copy sites compose the recovered memory-copy
 * owner.
 */
int nba97_frontend_resource_lookup(Nba97FrontendResourceLookupContext *,
                                   Nba97FrontendResourceLookupProgress *);
#ifdef __cplusplus
}
#endif
#endif
