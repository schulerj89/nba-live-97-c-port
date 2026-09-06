#ifndef NBA97_GAME_PERIOD_EXPIRY_ADAPTER_H
#define NBA97_GAME_PERIOD_EXPIRY_ADAPTER_H

#include "recovered/game_match_tick.h"
#include "recovered/game_period_expiry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GamePeriodExpiryBinding {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    Nba97GamePeriodExpiryMachine entry_machine;
    uint8_t entry_machine_ready;
    Nba97GamePeriodExpiryIo io;
    void* user;
    Nba97GamePeriodExpiryAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GamePeriodExpiryProgress progress;
    int result;
    size_t invocations;
} Nba97GamePeriodExpiryBinding;

/* Bind only match tick's natural 0x80068D6C call. The legacy tick service
 * does not carry live GPR/SP/HI/LO state, so the caller supplies an
 * independently proven entry machine whose JAL-produced ra is verified. */
int nba97_game_period_expiry_from_match_tick(
    void*, const Nba97MatchTickCall*, Nba97GamePeriodValue*);

#ifdef __cplusplus
}
#endif
#endif
