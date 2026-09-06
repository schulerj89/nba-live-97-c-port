#ifndef NBA97_GAMELOAD_BIOS_HEAP_ADAPTER_H
#define NBA97_GAMELOAD_BIOS_HEAP_ADAPTER_H

#include "recovered/gameload_bios_heap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameloadBiosHeapParentContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint32_t return_address;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97GameloadBiosHeapParentContract;

typedef struct Nba97GameloadBiosHeapBinding {
  size_t operation_budget;
  Nba97GameloadBiosHeapIo bios_io;
  void *bios_user;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameloadEntryEvent parent_event;
  Nba97GameloadBiosHeapMachine parent_machine;
  Nba97GameloadBiosHeapProgress progress;
  int result;
} Nba97GameloadBiosHeapBinding;

typedef struct Nba97GameloadBiosHeapAdapterProgress {
  size_t invocations;
  size_t completions;
  Nba97GameloadEntryEvent parent_event;
  Nba97GameloadBiosHeapMachine parent_machine;
  Nba97GameloadBiosHeapProgress progress;
  int result;
} Nba97GameloadBiosHeapAdapterProgress;

void nba97_gameload_bios_heap_binding_init(
    Nba97GameloadBiosHeapBinding *, size_t, Nba97GameloadBiosHeapIo, void *,
    uint32_t *, size_t);

int nba97_gameload_bios_heap_parent_contract(
    Nba97GameloadBiosHeapParentContract *);

int nba97_gameload_bios_heap_from_gameload_entry(
    void *, const Nba97GameTextMemory *, const Nba97GameloadEntryEvent *,
    Nba97GameloadEntryMachine *, Nba97GameloadEntryCalleeOutcome *);

/* Execute the committed GAMELOAD entry owner with this BIOS InitHeap
 * trampoline composed at its natural first-child boundary. The caller's
 * explicit callback remains responsible for the later GAMELOAD main child. */
int nba97_gameload_entry_with_recovered_bios_heap(
    const Nba97GameloadEntryContext *, Nba97GameloadBiosHeapBinding *,
    Nba97GameloadEntryProgress *, Nba97GameloadBiosHeapAdapterProgress *);

#ifdef __cplusplus
}
#endif
#endif
