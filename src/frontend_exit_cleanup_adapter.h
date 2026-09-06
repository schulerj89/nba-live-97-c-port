#ifndef NBA97_FRONTEND_EXIT_CLEANUP_ADAPTER_H
#define NBA97_FRONTEND_EXIT_CLEANUP_ADAPTER_H

#include "recovered/frontend_exit_cleanup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendExitCleanupSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendExitCleanupSiteContract;

typedef struct Nba97FrontendExitCleanupBinding {
  size_t operation_budget;
  Nba97FrontendExitCleanupIo io;
  void *user;
  Nba97FrontendExitCleanupAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendMainEvent event;
  Nba97FrontendExitCleanupProgress progress;
  int result;
} Nba97FrontendExitCleanupBinding;

int nba97_frontend_exit_cleanup_site_contract(
    uint8_t, Nba97FrontendExitCleanupSiteContract *);

int nba97_frontend_exit_cleanup_from_frontend_main(
    void *, const Nba97GameTextMemory *, const Nba97FrontendMainEvent *,
    Nba97FrontendMainMachine *, Nba97FrontendMainCalleeOutcome *);

#ifdef __cplusplus
}
#endif
#endif
