#ifndef NBA97_FRONTEND_OVERLAY_LOAD_ADAPTER_H
#define NBA97_FRONTEND_OVERLAY_LOAD_ADAPTER_H

#include "recovered/frontend_overlay_load.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97FrontendOverlayLoadSiteContract {
  uint32_t pc;
  uint32_t delay_slot_pc;
  uint32_t target;
  uint8_t argument_count;
  uint8_t target_program;
} Nba97FrontendOverlayLoadSiteContract;

typedef struct Nba97FrontendOverlayLoadBinding {
  size_t operation_budget;
  Nba97FrontendOverlayLoadIo io;
  void *user;
  Nba97FrontendOverlayLoadAccess *access_journal;
  size_t access_journal_capacity;
  uint32_t *instruction_journal;
  size_t instruction_journal_capacity;
  size_t invocations;
  size_t completions;
  Nba97FrontendMainEvent event;
  Nba97FrontendOverlayLoadProgress progress;
  int result;
} Nba97FrontendOverlayLoadBinding;

int nba97_frontend_overlay_load_site_contract(
    uint8_t, Nba97FrontendOverlayLoadSiteContract *);

int nba97_frontend_overlay_load_from_frontend_main(
    void *, const Nba97GameTextMemory *, const Nba97FrontendMainEvent *,
    Nba97FrontendMainMachine *, Nba97FrontendMainCalleeOutcome *);

#ifdef __cplusplus
}
#endif
#endif
