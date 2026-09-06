#ifndef NBA97_FRONTEND_RESOURCE_LOAD_ADAPTER_H
#define NBA97_FRONTEND_RESOURCE_LOAD_ADAPTER_H
#include "recovered/frontend_load_payload.h"
#include "recovered/frontend_resource_load.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97FrontendResourceLoadSiteContract {
  uint32_t pc, delay_slot_pc, target;
  uint8_t argument_count, target_program;
} Nba97FrontendResourceLoadSiteContract;
typedef struct Nba97FrontendResourceLoadBinding {
  size_t operation_budget;
  Nba97FrontendResourceLoadIo io;
  void *user;
  Nba97FrontendResourceLoadAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations, completions;
  Nba97FrontendLoadPayloadEvent parent_event;
  Nba97FrontendResourceLoadMachine parent_machine;
  Nba97FrontendResourceLoadProgress progress;
  int result;
} Nba97FrontendResourceLoadBinding;
typedef struct Nba97FrontendResourceLoadAdapterProgress {
  size_t invocations, completions;
  Nba97FrontendLoadPayloadEvent parent_event;
  Nba97FrontendResourceLoadMachine parent_machine;
  Nba97FrontendResourceLoadProgress progress;
  int result;
} Nba97FrontendResourceLoadAdapterProgress;
int nba97_frontend_resource_load_site_contract(
    uint8_t, Nba97FrontendResourceLoadSiteContract *);
int nba97_frontend_resource_load_from_frontend_load_payload(
    void *, const Nba97GameTextMemory *, const Nba97FrontendLoadPayloadEvent *,
    Nba97FrontendLoadPayloadMachine *);
int nba97_frontend_load_payload_with_recovered_resource(
    Nba97FrontendLoadPayloadContext *, Nba97FrontendResourceLoadBinding *,
    Nba97FrontendLoadPayloadProgress *,
    Nba97FrontendResourceLoadAdapterProgress *);
#ifdef __cplusplus
}
#endif
#endif
