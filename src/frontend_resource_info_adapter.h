#ifndef NBA97_FRONTEND_RESOURCE_INFO_ADAPTER_H
#define NBA97_FRONTEND_RESOURCE_INFO_ADAPTER_H

#include "recovered/frontend_resource_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendResourceInfoSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendResourceInfoSiteContract;

typedef struct Nba97FrontendResourceInfoBinding {
  size_t operation_budget;
  Nba97FrontendResourceInfoIo io;
  void *user;
  Nba97FrontendResourceInfoAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendResourceLoadEvent parent_event;
  Nba97FrontendResourceInfoMachine parent_machine;
  Nba97FrontendResourceInfoProgress progress;
  int result;
} Nba97FrontendResourceInfoBinding;

typedef struct Nba97FrontendResourceInfoAdapterProgress {
  size_t invocations;
  size_t completions;
  Nba97FrontendResourceLoadEvent parent_event;
  Nba97FrontendResourceInfoMachine parent_machine;
  Nba97FrontendResourceInfoProgress progress;
  int result;
} Nba97FrontendResourceInfoAdapterProgress;

int nba97_frontend_resource_info_site_contract(
    uint8_t, Nba97FrontendResourceInfoSiteContract *);

int nba97_frontend_resource_info_from_frontend_resource_load(
    void *, const Nba97GameTextMemory *, const Nba97FrontendResourceLoadEvent *,
    Nba97FrontendResourceLoadMachine *);

int nba97_frontend_resource_load_with_recovered_info(
    Nba97FrontendResourceLoadContext *, Nba97FrontendResourceInfoBinding *,
    Nba97FrontendResourceLoadProgress *,
    Nba97FrontendResourceInfoAdapterProgress *);

#ifdef __cplusplus
}
#endif
#endif
