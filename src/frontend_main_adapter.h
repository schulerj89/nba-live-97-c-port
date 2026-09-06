#ifndef NBA97_FRONTEND_MAIN_ADAPTER_H
#define NBA97_FRONTEND_MAIN_ADAPTER_H

#include "frontend_dispatch_entry_adapter.h"
#include "recovered/frontend_main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendMainSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
  uint8_t target_program;
  uint8_t dynamic_target;
} Nba97FrontendMainSiteContract;

typedef struct Nba97FrontendMainAdapterProgress {
  size_t wrapper_invocations;
  size_t wrapper_completions;
  Nba97FrontendMainEvent wrapper_event;
  Nba97FrontendMainMachine wrapper_machine;
  Nba97FrontendDispatchEntryProgress wrapper_progress;
  Nba97FrontendDispatchEntryAdapterProgress wrapper_adapter;
  int wrapper_result;
} Nba97FrontendMainAdapterProgress;

typedef struct Nba97FrontendMainCallerEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendMainCallerEvent;

typedef struct Nba97FrontendMainBinding {
  size_t operation_budget;
  Nba97FrontendMainIo io;
  void *user;
  Nba97FrontendDispatchEntryBinding wrapper;
  Nba97FrontendMainAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendMainCallerEvent event;
  Nba97FrontendMainProgress progress;
  Nba97FrontendMainAdapterProgress adapter;
  int result;
} Nba97FrontendMainBinding;

int nba97_frontend_main_site_contract(uint8_t,
                                      Nba97FrontendMainSiteContract *);

int nba97_frontend_main_with_recovered_dispatch_entry(
    Nba97FrontendMainContext *, Nba97FrontendDispatchEntryBinding *,
    Nba97FrontendMainProgress *, Nba97FrontendMainAdapterProgress *);

int nba97_frontend_main_from_overlay_entry(
    void *, const Nba97GameTextMemory *, const Nba97FrontendMainCallerEvent *,
    Nba97FrontendMainMachine *);

#ifdef __cplusplus
}
#endif
#endif
