#ifndef NBA97_FRONTEND_EXIT_WAIT_ADAPTER_H
#define NBA97_FRONTEND_EXIT_WAIT_ADAPTER_H

#include "recovered/frontend_exit_wait.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendExitWaitSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendExitWaitSiteContract;

typedef struct Nba97FrontendExitWaitBinding {
  size_t operation_budget;
  Nba97FrontendExitWaitIo io;
  void *user;
  Nba97FrontendExitWaitAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendExitCleanupEvent event;
  Nba97FrontendExitWaitProgress progress;
  int result;
} Nba97FrontendExitWaitBinding;

int nba97_frontend_exit_wait_site_contract(
    uint8_t, Nba97FrontendExitWaitSiteContract *);

int nba97_frontend_exit_wait_from_frontend_exit_cleanup(
    void *, const Nba97GameTextMemory *,
    const Nba97FrontendExitCleanupEvent *, Nba97FrontendExitCleanupMachine *);

#ifdef __cplusplus
}
#endif
#endif
