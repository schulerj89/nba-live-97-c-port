#ifndef NBA97_GAME_COURT_STARTUP_SERVICES_H
#define NBA97_GAME_COURT_STARTUP_SERVICES_H
#include "game_court_startup.h"
#include "game_heap_release.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Concrete90698 composition for the startup bridge. The unchanged full native
 * heap-release owner handles the actual retained bank/descriptor/free/lock
 * effects. LOAD and SYNC remain mandatory real services supplied below.
 * Bind startup_context.io=nba97_game_court_startup_service_io and user=this.
 * Release journal/progress do not overlap mapped bytes, bridge journal or this
 * context. Retained metadata/lifetimes are fixed, including across services.
 * Released bytes are not wiped or detached here: the host allocation owner
 * derives lifetime changes from actual descriptor effects, not merely a
 * completed90698 (its not-found branch releases nothing). A numeric source
 * pointer surviving release does not authorize further host dereferences.
 */
typedef struct Nba97GameCourtStartupServices {
    Nba97GameCourtStartupIo load_or_sync;
    void* user;
    size_t release_access_budget;
    Nba97GameHeapReleaseStore* release_journal;
    size_t release_capacity;
    Nba97GameHeapReleaseProgress release;
    int release_status; /* Last reached90698 result; no implicit success. */
} Nba97GameCourtStartupServices;
int nba97_game_court_startup_service_io(void*,const Nba97GameTextMemory*,
    const Nba97GameCourtStartupEvent*,uint32_t* returned);
#ifdef __cplusplus
}
#endif
#endif
