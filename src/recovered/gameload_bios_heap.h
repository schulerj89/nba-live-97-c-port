#ifndef NBA97_GAMELOAD_BIOS_HEAP_H
#define NBA97_GAMELOAD_BIOS_HEAP_H

#include "gameload_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameloadEntryWord Nba97GameloadBiosHeapWord;
typedef Nba97GameloadEntryMachine Nba97GameloadBiosHeapMachine;

enum Nba97GameloadBiosHeapRegister {
  NBA97_GAMELOAD_BIOS_HEAP_ZERO = 0,
  NBA97_GAMELOAD_BIOS_HEAP_A0 = 4,
  NBA97_GAMELOAD_BIOS_HEAP_A1 = 5,
  NBA97_GAMELOAD_BIOS_HEAP_T1 = 9,
  NBA97_GAMELOAD_BIOS_HEAP_T2 = 10,
  NBA97_GAMELOAD_BIOS_HEAP_RA = 31,
  NBA97_GAMELOAD_BIOS_HEAP_REGISTER_COUNT = 32
};

enum Nba97GameloadBiosHeapSite {
  NBA97_GAMELOAD_BIOS_HEAP_SITE_NONE = 0,
  NBA97_GAMELOAD_BIOS_HEAP_SITE_A0_SERVICE_39,
  NBA97_GAMELOAD_BIOS_HEAP_SITE_COUNT
};

typedef struct Nba97GameloadBiosHeapEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t service;
  uint8_t argument_count;
} Nba97GameloadBiosHeapEvent;

/* This callback owns the unresolved BIOS A0 service 0x39 boundary. It receives
 * the complete post-delay CPU state and retained memory, may mutate both, and
 * returns 1 only after the BIOS service has returned through the live ra. */
typedef int (*Nba97GameloadBiosHeapIo)(
    void *, const Nba97GameTextMemory *, const Nba97GameloadBiosHeapEvent *,
    Nba97GameloadBiosHeapMachine *);

typedef struct Nba97GameloadBiosHeapContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameloadBiosHeapMachine machine;
  Nba97GameloadBiosHeapIo io;
  void *user;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
} Nba97GameloadBiosHeapContext;

typedef struct Nba97GameloadBiosHeapProgress {
  size_t operations;
  size_t callbacks_completed;
  size_t instruction_events;
  uint32_t instruction_count;
  uint32_t stopped_pc;
  uint32_t stopped_entry;
  uint8_t stopped_service;
  Nba97GameloadBiosHeapEvent event;
  Nba97GameloadBiosHeapMachine machine;
  uint8_t completed;
} Nba97GameloadBiosHeapProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMELOAD
 * Address: 0x801E1590
 * Range: 0x801E1590..0x801E159B (inclusive)
 * Source size: 12 bytes / 3 instructions
 * Evidence: fresh Ghidra gameload_801e1590_continue.txt; instruction SHA-256
 * 4487ee3019aae533a71d191483e6876aa40c2530923670ec0e012a78204fb863
 *
 * Purpose: Select BIOS A0 service 0x39 (InitHeap) and tail-transfer to vector
 * 0x000000A0.
 * Inputs: Full 32-GPR/HI-LO machine and retained GAMELOAD memory; live a0 heap
 * base, a1 heap size, and ra are forwarded without knownness validation.
 * Returns: The typed BIOS boundary supplies every returned CPU and memory
 * effect; the trampoline itself only replaces t2 with 0xA0 and t1 with 0x39.
 * Guest memory: No direct accesses; the typed BIOS callback receives retained
 * mapped memory and may mutate it while implementing the service.
 * Calls: Tail transfer to BIOS vector 0x000000A0 at 0x801E1594, selecting
 * service 0x39 in the 0x801E1598 delay slot.
 * Original quirks: JR T2 creates no link, leaves ra unchanged, accepts unknown
 * a0/a1/ra bytes, and executes the T1 delay-slot write before refusal or a
 * depleted host operation budget is reported.
 * Native mapping: Guest values remain uint32_t words with per-byte knownness;
 * a bounded typed callback owns BIOS behavior without host-pointer casts.
 */
int nba97_gameload_bios_heap(Nba97GameloadBiosHeapContext *,
                             Nba97GameloadBiosHeapProgress *);

#ifdef __cplusplus
}
#endif
#endif
