#ifndef NBA97_FRONTEND_IO_DRAIN_ADAPTER_H
#define NBA97_FRONTEND_IO_DRAIN_ADAPTER_H

#include "recovered/frontend_io_drain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendIoDrainSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendIoDrainSiteContract;

typedef struct Nba97FrontendIoDrainBinding {
  size_t operation_budget;
  Nba97FrontendIoDrainIo io;
  void *user;
  Nba97FrontendIoDrainAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendExitDrainEvent event;
  Nba97FrontendIoDrainProgress progress;
  int result;
} Nba97FrontendIoDrainBinding;

int nba97_frontend_io_drain_site_contract(
    uint8_t, Nba97FrontendIoDrainSiteContract *);

int nba97_frontend_io_drain_from_frontend_exit_drain(
    void *, const Nba97GameTextMemory *, const Nba97FrontendExitDrainEvent *,
    Nba97FrontendExitDrainMachine *);

#ifdef __cplusplus
}
#endif
#endif
