#ifndef NBA97_GAMELOAD_ENTRY_ADAPTER_H
#define NBA97_GAMELOAD_ENTRY_ADAPTER_H

#include "frontend_memory_copy_adapter.h"
#include "recovered/gameload_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameloadEntryParentContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint32_t return_address;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97GameloadEntryParentContract;

typedef struct Nba97GameloadEntryBinding {
  size_t operation_budget;
  Nba97GameloadEntryIo io;
  void *user;
  Nba97GameloadEntryAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendMainEvent parent_event;
  Nba97GameloadEntryMachine parent_machine;
  Nba97GameloadEntryProgress progress;
  int result;
} Nba97GameloadEntryBinding;

typedef struct Nba97GameloadEntryAdapterProgress {
  size_t invocations;
  size_t completions;
  Nba97FrontendMainEvent parent_event;
  Nba97GameloadEntryMachine parent_machine;
  Nba97GameloadEntryProgress progress;
  int result;
} Nba97GameloadEntryAdapterProgress;

int nba97_gameload_entry_parent_contract(
    Nba97GameloadEntryParentContract *);

int nba97_gameload_entry_from_frontend_main(
    void *, const Nba97GameTextMemory *, const Nba97FrontendMainEvent *,
    Nba97FrontendMainMachine *, Nba97FrontendMainCalleeOutcome *);

/* Execute recovered frontend main with its committed memory-copy owner and
 * this GAMELOAD entry owner composed at their natural 0x80028B54 and
 * 0x80028B68 boundaries. All other frontend-main callbacks are forwarded. */
int nba97_frontend_main_with_recovered_memory_copy_and_gameload(
    Nba97FrontendMainContext *, Nba97FrontendMemoryCopyBinding *,
    Nba97GameloadEntryBinding *, Nba97FrontendMainProgress *,
    Nba97GameloadEntryAdapterProgress *);

#ifdef __cplusplus
}
#endif
#endif
