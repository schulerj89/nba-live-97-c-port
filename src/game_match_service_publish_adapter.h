#ifndef NBA97_GAME_MATCH_SERVICE_PUBLISH_ADAPTER_H
#define NBA97_GAME_MATCH_SERVICE_PUBLISH_ADAPTER_H

#include "recovered/game_match_service_publish.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMatchServicePublishBinding {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    Nba97GameMatchServicePublishMachine entry_machine;
    uint8_t entry_machine_ready;
    Nba97GameMatchServicePublishIo io;
    void* user;
    Nba97GameMatchServicePublishAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GameMatchServicePublishProgress progress;
    int result;
    size_t invocations;
} Nba97GameMatchServicePublishBinding;

/* Compose this owner only at match tick's actual 0x80068D7C event. The legacy
 * tick callback exposes no GPR/SP/HI/LO state, so the caller must independently
 * provide and mark ready the exact source-entry machine, including JAL ra. */
int nba97_game_match_service_publish_from_match_tick(
    void*, const Nba97MatchTickCall*, Nba97GamePeriodValue*);

#ifdef __cplusplus
}
#endif
#endif
