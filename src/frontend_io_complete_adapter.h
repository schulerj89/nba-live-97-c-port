#ifndef NBA97_FRONTEND_IO_COMPLETE_ADAPTER_H
#define NBA97_FRONTEND_IO_COMPLETE_ADAPTER_H

#include "recovered/frontend_io_complete.h"
#include "recovered/frontend_io_drain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendIoCompleteParentContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint32_t return_address;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendIoCompleteParentContract;

enum Nba97FrontendIoCompleteRecordCapacity {
  NBA97_FRONTEND_IO_COMPLETE_RECORD_ACCESSES = 9,
  NBA97_FRONTEND_IO_COMPLETE_RECORD_INSTRUCTIONS = 81
};

typedef struct Nba97FrontendIoCompleteParentRecord {
  Nba97FrontendExitDrainEvent event;
  Nba97FrontendIoCompleteMachine parent_machine;
  Nba97FrontendIoCompleteProgress progress;
  Nba97FrontendIoCompleteAccess first_access;
  Nba97FrontendIoCompleteAccess
      access_journal[NBA97_FRONTEND_IO_COMPLETE_RECORD_ACCESSES];
  uint32_t
      instruction_journal[NBA97_FRONTEND_IO_COMPLETE_RECORD_INSTRUCTIONS];
  size_t access_events;
  size_t instruction_events;
  int result;
  uint8_t completed;
} Nba97FrontendIoCompleteParentRecord;

typedef struct Nba97FrontendIoCompleteBinding {
  size_t operation_budget;
  Nba97FrontendIoCompleteAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  Nba97FrontendIoCompleteParentRecord *parent_journal;
  size_t parent_journal_capacity;
  size_t parent_events;
  size_t invocations;
  size_t completions;
  Nba97FrontendExitDrainEvent parent_event;
  Nba97FrontendIoCompleteMachine parent_machine;
  Nba97FrontendIoCompleteProgress progress;
  int result;
} Nba97FrontendIoCompleteBinding;

typedef struct Nba97FrontendIoCompleteAdapterProgress {
  size_t invocations;
  size_t completions;
  Nba97FrontendExitDrainEvent first_event;
  Nba97FrontendIoCompleteMachine first_parent_machine;
  Nba97FrontendIoCompleteProgress first_progress;
  Nba97FrontendIoCompleteAccess first_access;
  Nba97FrontendExitDrainEvent latest_event;
  Nba97FrontendIoCompleteMachine latest_parent_machine;
  Nba97FrontendIoCompleteProgress latest_progress;
  Nba97FrontendIoCompleteAccess latest_access;
  int first_result;
  int latest_result;
} Nba97FrontendIoCompleteAdapterProgress;

int nba97_frontend_io_complete_parent_contract(
    Nba97FrontendIoCompleteParentContract *);

int nba97_frontend_io_complete_from_frontend_exit_drain(
    void *, const Nba97GameTextMemory *, const Nba97FrontendExitDrainEvent *,
    Nba97FrontendExitDrainMachine *);

/* Same poll owner at the other recovered natural caller; journals retain the
 * actual I/O-drain PC/site rather than inventing an exit-drain event. */
int nba97_frontend_io_complete_from_frontend_io_drain(
    void *, const Nba97GameTextMemory *, const Nba97FrontendIoDrainEvent *,
    Nba97FrontendIoDrainMachine *);

int nba97_frontend_exit_drain_with_recovered_io_complete(
    Nba97FrontendExitDrainContext *, Nba97FrontendIoCompleteBinding *,
    Nba97FrontendExitDrainProgress *,
    Nba97FrontendIoCompleteAdapterProgress *);

#ifdef __cplusplus
}
#endif
#endif
