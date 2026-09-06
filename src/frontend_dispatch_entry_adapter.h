#ifndef NBA97_FRONTEND_DISPATCH_ENTRY_ADAPTER_H
#define NBA97_FRONTEND_DISPATCH_ENTRY_ADAPTER_H

#include "frontend_dispatch_adapter.h"
#include "recovered/frontend_dispatch_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendDispatchEntryAdapterProgress {
  size_t dispatcher_invocations;
  size_t dispatcher_completions;
  Nba97FrontendDispatchEntryEvent dispatcher_event;
  Nba97FrontendDispatchProgress dispatcher_progress;
  int dispatcher_result;
} Nba97FrontendDispatchEntryAdapterProgress;

typedef struct Nba97FrontendDispatchEntryCallerEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t argument_count;
} Nba97FrontendDispatchEntryCallerEvent;

typedef struct Nba97FrontendDispatchEntryBinding {
  size_t operation_budget;
  Nba97FrontendDispatchBinding dispatcher;
  Nba97FrontendDispatchEntryAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendDispatchEntryCallerEvent event;
  Nba97FrontendDispatchEntryProgress progress;
  Nba97FrontendDispatchEntryAdapterProgress adapter;
  int result;
} Nba97FrontendDispatchEntryBinding;

int nba97_frontend_dispatch_entry_with_recovered_dispatch(
    Nba97FrontendDispatchEntryContext *, Nba97FrontendDispatchBinding *,
    Nba97FrontendDispatchEntryProgress *,
    Nba97FrontendDispatchEntryAdapterProgress *);

int nba97_frontend_dispatch_entry_from_frontend_main(
    void *, const Nba97GameTextMemory *,
    const Nba97FrontendDispatchEntryCallerEvent *,
    Nba97FrontendDispatchEntryMachine *);

#ifdef __cplusplus
}
#endif
#endif
