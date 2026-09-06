#ifndef NBA97_FRONTEND_DISPATCH_ADAPTER_H
#define NBA97_FRONTEND_DISPATCH_ADAPTER_H

#include "recovered/frontend_dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendDispatchCallerEvent {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t entry;
  size_t operation;
  size_t invocation;
  uint8_t argument_count;
} Nba97FrontendDispatchCallerEvent;

typedef struct Nba97FrontendDispatchSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
} Nba97FrontendDispatchSiteContract;

typedef struct Nba97FrontendDispatchBinding {
  size_t operation_budget;
  Nba97FrontendDispatchIo io;
  void *user;
  Nba97FrontendDispatchAccess *access_journal;
  size_t access_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendDispatchCallerEvent event;
  Nba97FrontendDispatchProgress progress;
  int result;
} Nba97FrontendDispatchBinding;

int nba97_frontend_dispatch_from_800360d4(
    void *, const Nba97GameTextMemory *,
    const Nba97FrontendDispatchCallerEvent *, Nba97FrontendDispatchMachine *);

int nba97_frontend_dispatch_site_contract(uint8_t,
                                          Nba97FrontendDispatchSiteContract *);

#ifdef __cplusplus
}
#endif
#endif
