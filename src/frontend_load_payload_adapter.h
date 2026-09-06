#ifndef NBA97_FRONTEND_LOAD_PAYLOAD_ADAPTER_H
#define NBA97_FRONTEND_LOAD_PAYLOAD_ADAPTER_H

#include "recovered/frontend_load_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendLoadPayloadParentContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint32_t return_address;
  uint8_t site;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendLoadPayloadParentContract;

typedef struct Nba97FrontendLoadPayloadBinding {
  size_t operation_budget;
  Nba97FrontendLoadPayloadIo io;
  void *user;
  Nba97FrontendLoadPayloadAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendOverlayLoadEvent parent_event;
  Nba97FrontendLoadPayloadMachine parent_machine;
  Nba97FrontendLoadPayloadProgress progress;
  int result;
} Nba97FrontendLoadPayloadBinding;

typedef struct Nba97FrontendLoadPayloadAdapterProgress {
  size_t invocations;
  size_t completions;
  Nba97FrontendOverlayLoadEvent parent_event;
  Nba97FrontendLoadPayloadMachine parent_machine;
  Nba97FrontendLoadPayloadProgress progress;
  int result;
} Nba97FrontendLoadPayloadAdapterProgress;

int nba97_frontend_load_payload_parent_contract(
    Nba97FrontendLoadPayloadParentContract *);

int nba97_frontend_load_payload_from_frontend_overlay_load(
    void *, const Nba97GameTextMemory *, const Nba97FrontendOverlayLoadEvent *,
    Nba97FrontendOverlayLoadMachine *);

int nba97_frontend_overlay_load_with_recovered_payload(
    Nba97FrontendOverlayLoadContext *, Nba97FrontendLoadPayloadBinding *,
    Nba97FrontendOverlayLoadProgress *,
    Nba97FrontendLoadPayloadAdapterProgress *);

#ifdef __cplusplus
}
#endif
#endif
