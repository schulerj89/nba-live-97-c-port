#ifndef NBA97_FRONTEND_EXIT_DRAIN_ADAPTER_H
#define NBA97_FRONTEND_EXIT_DRAIN_ADAPTER_H

#include "recovered/frontend_exit_drain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendExitDrainSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendExitDrainSiteContract;

typedef struct Nba97FrontendExitDrainBinding {
  size_t operation_budget;
  Nba97FrontendExitDrainIo io;
  void *user;
  Nba97FrontendExitDrainAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendExitCleanupEvent event;
  Nba97FrontendExitDrainProgress progress;
  int result;
} Nba97FrontendExitDrainBinding;

int nba97_frontend_exit_drain_site_contract(
    uint8_t, Nba97FrontendExitDrainSiteContract *);

int nba97_frontend_exit_drain_from_frontend_exit_cleanup(
    void *, const Nba97GameTextMemory *, const Nba97FrontendExitCleanupEvent *,
    Nba97FrontendExitCleanupMachine *);

#ifdef __cplusplus
}
#endif
#endif
