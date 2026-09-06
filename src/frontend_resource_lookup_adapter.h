#ifndef NBA97_FRONTEND_RESOURCE_LOOKUP_ADAPTER_H
#define NBA97_FRONTEND_RESOURCE_LOOKUP_ADAPTER_H

#include "recovered/frontend_memory_copy.h"
#include "recovered/frontend_resource_lookup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendResourceLookupSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendResourceLookupSiteContract;

typedef struct Nba97FrontendResourceLookupCopyBinding {
  size_t operation_budget;
  Nba97FrontendMemoryCopyAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendResourceLookupEvent parent_event;
  Nba97FrontendMemoryCopyMachine parent_machine;
  Nba97FrontendMemoryCopyProgress progress;
  int result;
} Nba97FrontendResourceLookupCopyBinding;

typedef struct Nba97FrontendResourceLookupBinding {
  size_t operation_budget;
  Nba97FrontendResourceLookupIo io;
  void *user;
  Nba97FrontendResourceLookupAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  Nba97FrontendResourceLookupCopyBinding copy[2];
  size_t invocations;
  size_t completions;
  Nba97FrontendResourceLoadEvent parent_event;
  Nba97FrontendResourceLookupMachine parent_machine;
  Nba97FrontendResourceLookupProgress progress;
  int result;
} Nba97FrontendResourceLookupBinding;

typedef struct Nba97FrontendResourceLookupAdapterProgress {
  size_t invocations;
  size_t completions;
  Nba97FrontendResourceLoadEvent parent_event;
  Nba97FrontendResourceLookupMachine parent_machine;
  Nba97FrontendResourceLookupProgress progress;
  int result;
} Nba97FrontendResourceLookupAdapterProgress;

int nba97_frontend_resource_lookup_site_contract(
    uint8_t site, Nba97FrontendResourceLookupSiteContract *contract);
int nba97_frontend_resource_lookup_from_resource_load(
    void *binding, const Nba97GameTextMemory *memory,
    const Nba97FrontendResourceLoadEvent *event,
    Nba97FrontendResourceLoadMachine *machine);
int nba97_frontend_resource_load_with_recovered_lookup(
    Nba97FrontendResourceLoadContext *context,
    Nba97FrontendResourceLookupBinding *binding,
    Nba97FrontendResourceLoadProgress *progress,
    Nba97FrontendResourceLookupAdapterProgress *adapter_progress);

#ifdef __cplusplus
}
#endif
#endif
