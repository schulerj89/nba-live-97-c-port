#ifndef NBA97_FRONTEND_MEMORY_COPY_ADAPTER_H
#define NBA97_FRONTEND_MEMORY_COPY_ADAPTER_H

#include "recovered/frontend_memory_copy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendMemoryCopyBinding {
  size_t operation_budget;
  Nba97FrontendMemoryCopyAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendMainEvent event;
  Nba97FrontendMemoryCopyMachine input_machine;
  Nba97FrontendMemoryCopyProgress progress;
  int result;
} Nba97FrontendMemoryCopyBinding;

/* Compose the FEONLY 0x800909A8 owner at frontend-main's natural JAL site
 * 0x80028B54. Every other main callback is forwarded unchanged. */
int nba97_frontend_main_with_recovered_memory_copy(
    Nba97FrontendMainContext *, Nba97FrontendMemoryCopyBinding *,
    Nba97FrontendMainProgress *);

#ifdef __cplusplus
}
#endif
#endif
