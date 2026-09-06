#ifndef NBA97_FRONTEND_CLOCK_READ_ADAPTER_H
#define NBA97_FRONTEND_CLOCK_READ_ADAPTER_H

#include "recovered/frontend_clock_read.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendClockReadParentContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint32_t return_address;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendClockReadParentContract;

typedef struct Nba97FrontendClockReadBinding {
  size_t operation_budget;
  Nba97FrontendClockReadAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendExitWaitEvent parent_event;
  Nba97FrontendClockReadMachine parent_machine;
  Nba97FrontendClockReadProgress progress;
  int result;
} Nba97FrontendClockReadBinding;

typedef struct Nba97FrontendClockReadAdapterProgress {
  size_t invocations;
  size_t completions;
  size_t initial_invocations;
  size_t loop_invocations;
  Nba97FrontendExitWaitEvent initial_event;
  Nba97FrontendClockReadMachine initial_parent_machine;
  Nba97FrontendClockReadProgress initial_progress;
  Nba97FrontendClockReadAccess initial_access;
  Nba97FrontendExitWaitEvent loop_event;
  Nba97FrontendClockReadMachine loop_parent_machine;
  Nba97FrontendClockReadProgress loop_progress;
  Nba97FrontendClockReadAccess loop_access;
  int initial_result;
  int loop_result;
} Nba97FrontendClockReadAdapterProgress;

int nba97_frontend_clock_read_parent_contract(
    uint8_t, Nba97FrontendClockReadParentContract *);

int nba97_frontend_clock_read_from_frontend_exit_wait(
    void *, const Nba97GameTextMemory *, const Nba97FrontendExitWaitEvent *,
    Nba97FrontendExitWaitMachine *);

int nba97_frontend_exit_wait_with_recovered_clock(
    Nba97FrontendExitWaitContext *, Nba97FrontendClockReadBinding *,
    Nba97FrontendExitWaitProgress *,
    Nba97FrontendClockReadAdapterProgress *);

#ifdef __cplusplus
}
#endif
#endif
