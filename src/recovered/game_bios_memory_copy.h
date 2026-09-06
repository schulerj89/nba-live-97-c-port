#ifndef NBA97_GAME_BIOS_MEMORY_COPY_H
#define NBA97_GAME_BIOS_MEMORY_COPY_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameBiosMemoryCopyWord;
typedef Nba97GameMatchClocksMachine Nba97GameBiosMemoryCopyMachine;

typedef struct Nba97GameBiosMemoryCopyEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t service;
  uint8_t argument_count;
} Nba97GameBiosMemoryCopyEvent;

/* The callback models only the BIOS tail-transfer boundary. It receives the
 * full post-delay machine and retained memory, may mutate both, and returns 1
 * only when BIOS service 0x2A has returned through the live ra. */
typedef int (*Nba97GameBiosMemoryCopyIo)(void *, const Nba97GameTextMemory *,
                                         const Nba97GameBiosMemoryCopyEvent *,
                                         Nba97GameBiosMemoryCopyMachine *);

typedef struct Nba97GameBiosMemoryCopyContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameBiosMemoryCopyMachine machine;
  Nba97GameBiosMemoryCopyIo io;
  void *user;
} Nba97GameBiosMemoryCopyContext;

typedef struct Nba97GameBiosMemoryCopyProgress {
  size_t operations;
  size_t callbacks_completed;
  uint32_t stopped_pc;
  uint32_t stopped_entry;
  uint8_t stopped_service;
  Nba97GameBiosMemoryCopyEvent event;
  Nba97GameBiosMemoryCopyMachine machine;
  uint8_t completed;
} Nba97GameBiosMemoryCopyProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009CB0C
 * Range: 0x8009CB0C..0x8009CB17 (inclusive)
 * Source size: 12 bytes / 3 instructions
 * Evidence: fresh Ghidra game_8009cb0c.txt; instruction SHA-256
 * ad7c5bc50bc07966feaaee9043cbb18cd00d1ff4ffbcfbe37fdad2b22ea2876d
 *
 * Purpose: Select BIOS service 0x2A and tail-transfer to vector 0x000000A0.
 * Inputs: Full 32-GPR/HI-LO machine and mapped memory; live a0 destination,
 * a1 source, a2 byte count, and ra are forwarded without validation.
 * Returns: The typed BIOS boundary supplies all returned machine and memory
 * effects; the trampoline itself only replaces t2 with 0xA0 and t1 with 0x2A.
 * Guest memory: No direct access; the typed BIOS callback receives retained
 * mapped memory and may perform the service's transfer.
 * Calls: Tail transfer to BIOS vector 0x000000A0 at 0x8009CB10, selecting
 * service 0x2A in the 0x8009CB14 delay slot.
 * Original quirks: JR T2 creates no link, leaves ra unchanged, accepts unknown
 * arguments, and executes the T1 delay-slot write before the BIOS boundary.
 * Native mapping: The low BIOS vector and all arguments remain uint32_t guest
 * values; a typed callback owns BIOS behavior without libc or host pointers.
 */
int nba97_game_bios_memory_copy(Nba97GameBiosMemoryCopyContext *,
                                Nba97GameBiosMemoryCopyProgress *);

#ifdef __cplusplus
}
#endif
#endif
