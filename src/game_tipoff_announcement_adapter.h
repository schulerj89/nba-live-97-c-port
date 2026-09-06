#ifndef NBA97_GAME_TIPOFF_ANNOUNCEMENT_ADAPTER_H
#define NBA97_GAME_TIPOFF_ANNOUNCEMENT_ADAPTER_H

#include "recovered/game_first_period_startup.h"
#include "recovered/game_tipoff_announcement.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameTipoffAnnouncementBinding {
    size_t operation_budget;
    Nba97GameTipoffAnnouncementIo io;
    void* user;
    Nba97GameTipoffAnnouncementAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GameTipoffAnnouncementProgress progress;
    int result;
    size_t invocations;
} Nba97GameTipoffAnnouncementBinding;

/* Compose the complete 0x8007EF4C owner only at first-period startup's
 * actual 0x80067450 event. Shared retained memory and the full live GPR file
 * flow through every completed or failed child prefix. */
int nba97_game_tipoff_announcement_from_first_period_startup(void*,
    const Nba97GameTextMemory*, const Nba97GameFirstPeriodStartupEvent*,
    Nba97GameFirstPeriodStartupRegisters*);

#ifdef __cplusplus
}
#endif
#endif
