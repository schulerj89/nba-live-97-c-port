#ifndef NBA97_FRONTEND_RESOURCE_CLEANUP_H
#define NBA97_FRONTEND_RESOURCE_CLEANUP_H
#include "frontend_resource.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97PortraitCacheResource {
    uint32_t physical_record; /*F95E8+i*12: retained by2FB00 */
    uint32_t data;            /*F95EC+i*12 */
    uint32_t graphic;         /*F95F0+i*12 */
} Nba97PortraitCacheResource;
typedef struct Nba97FrontendResourceCleanup {
    uint32_t portrait_index;  /*F9418: distinct from Cool Facts F84C8 */
    Nba97PortraitCacheResource portrait[2];
    uint32_t card_data;       /*F7E7C: random frontend player-card allocation */
} Nba97FrontendResourceCleanup;
typedef enum Nba97FrontendCleanupCall {
    NBA97_FRONTEND_CLEANUP_DRAIN,        /*393F0 */
    NBA97_FRONTEND_CLEANUP_CANCEL,       /*39308(identity20) */
    NBA97_FRONTEND_CLEANUP_VOICE_STATUS, /*92BFC(announcer handle) */
    NBA97_FRONTEND_CLEANUP_FADE,         /*7B2BC(handle,20,-1) */
    NBA97_FRONTEND_CLEANUP_UNLOAD_BANK,  /*91B28(bank_context,program) */
    NBA97_FRONTEND_CLEANUP_FREE_DATA,    /*7760C(data), not handle free */
    NBA97_FRONTEND_CLEANUP_BUFFER_WAIT,  /*38AE0(target,480,8003282C) */
    NBA97_FRONTEND_CLEANUP_SYNC,         /*804E8(0) */
    NBA97_FRONTEND_CLEANUP_HARDWARE_WAIT /*28BF0 */
} Nba97FrontendCleanupCall;
typedef uint32_t (*Nba97FrontendCleanupInvoke)(void*, Nba97FrontendCleanupCall,
    uint32_t a0, uint32_t a1, uint32_t a2);

/* Complete313C8 scalar/call owner. Returns original0/1 (1 only when a live
 * announcer required a fade/wait), or-1 for invalid native arguments before
 * effects. Callbacks operate on live state and must ensure eventual status
 * progress: the original loop has no timeout or frontend pump. */
int nba97_frontend_announcer_stop(Nba97CoolIndexLoad*, Nba97FrontendCleanupInvoke, void*);
/* Complete2FB00, including313C8. cool_index_data must point to the SAME F84C8
 * field used by music_transition and cool_index_load. Resource tokens must
 * represent owned native resources, never truncated host pointers. Structs
 * and the F84C8 field must have disjoint storage. Callback side effects remain
 * explicit; no recursive call into this owner. Returns1 after completion,
 * 0 for missing arguments before effects (source return value was unused).
 * Graphics F1478 stays live for the old-screen callback during BUFFER_WAIT;
 * its later release belongs to31A88, not this function. */
int nba97_frontend_resource_cleanup(Nba97FrontendResourceCleanup*,
    Nba97CoolIndexLoad*, uint32_t* cool_index_data, uint32_t buffer_target,
    Nba97FrontendCleanupInvoke, void*);

#ifdef __cplusplus
}
#endif
#endif
