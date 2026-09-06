#ifndef NBA97_GAME_FIRST_PERIOD_STARTUP_ADAPTER_H
#define NBA97_GAME_FIRST_PERIOD_STARTUP_ADAPTER_H

#include "recovered/game_first_period_startup.h"
#include "recovered/game_period_startup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameFirstPeriodStartupBinding {
    size_t operation_budget;
    Nba97GameFirstPeriodStartupIo io;
    void* user;
    Nba97GameFirstPeriodStartupAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GameFirstPeriodStartupProgress progress;
    int result;
    size_t invocations;
} Nba97GameFirstPeriodStartupBinding;

/* Compose the complete 0x800673F0 owner only at period startup's actual
 * 0x80067494 zero-selector event. Shared retained memory and the complete live
 * GPR file flow into the child, and every completed or failed prefix flows back
 * to the parent callback. */
int nba97_game_first_period_startup_from_period_startup(void*,
    const Nba97GameTextMemory*, const Nba97GamePeriodStartupEvent*,
    Nba97GamePeriodStartupRegisters*);

#ifdef __cplusplus
}
#endif
#endif
