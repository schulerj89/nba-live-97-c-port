#ifndef NBA97_GAMELOAD_MAIN_ADAPTER_H
#define NBA97_GAMELOAD_MAIN_ADAPTER_H

#include "gameload_entry_adapter.h"
#include "recovered/gameload_main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameloadMainSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
  uint8_t target_program;
  uint8_t dynamic_target;
} Nba97GameloadMainSiteContract;

typedef struct Nba97GameloadMainParentContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint32_t return_address;
  size_t operation;
  size_t invocation;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97GameloadMainParentContract;

typedef struct Nba97GameloadMainBinding {
  size_t operation_budget;
  Nba97GameloadMainIo io;
  void *user;
  Nba97GameloadMainAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97GameloadEntryEvent parent_event;
  Nba97GameloadMainMachine parent_machine;
  Nba97GameloadMainProgress progress;
  int result;
} Nba97GameloadMainBinding;

typedef struct Nba97GameloadMainAdapterProgress {
  size_t invocations;
  size_t completions;
  Nba97GameloadEntryEvent parent_event;
  Nba97GameloadMainMachine parent_machine;
  Nba97GameloadMainProgress progress;
  int result;
} Nba97GameloadMainAdapterProgress;

int nba97_gameload_main_site_contract(uint8_t,
                                      Nba97GameloadMainSiteContract *);
int nba97_gameload_main_parent_contract(Nba97GameloadMainParentContract *);

int nba97_gameload_main_from_entry(
    void *, const Nba97GameTextMemory *, const Nba97GameloadEntryEvent *,
    Nba97GameloadEntryMachine *, Nba97GameloadEntryCalleeOutcome *);

int nba97_gameload_entry_with_recovered_main(
    Nba97GameloadEntryContext *, Nba97GameloadMainBinding *,
    Nba97GameloadEntryProgress *, Nba97GameloadMainAdapterProgress *);

#ifdef __cplusplus
}
#endif
#endif
