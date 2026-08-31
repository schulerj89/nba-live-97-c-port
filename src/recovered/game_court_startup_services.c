#include "game_court_startup_services.h"
int nba97_game_court_startup_service_io(void* user,const Nba97GameTextMemory* memory,
    const Nba97GameCourtStartupEvent* event,uint32_t* returned){
    Nba97GameCourtStartupServices* services=(Nba97GameCourtStartupServices*)user;
    Nba97GameHeapReleaseContext release;
    Nba97GameHeapReleaseValue unused_incoming;
    if(!services||!memory||!event||!returned)return 0;
    if(event->kind==NBA97_COURT_STARTUP_LOAD_29BFC||event->kind==NBA97_COURT_STARTUP_SYNC_994F4){
        if(!services->load_or_sync)return 0;
        return services->load_or_sync(services->user,memory,event,returned)==1;
    }
    if(event->kind!=NBA97_COURT_STARTUP_FREE_90698)return 0;
    release.memory=*memory;release.access_budget=services->release_access_budget;
    /* This caller never consumes90698's v0. The source NULL arm preserves its
     * incoming value: carry unknownness rather than inventing a known zero.
     * The bridge's required prior29BFC ordinarily supplies a nonzero payload. */
    unused_incoming.word=0;unused_incoming.known=0;
    services->release_status=nba97_game_heap_release(&release,NBA97_HEAP_RELEASE_PAYLOAD_90698,
        event->argument[0],unused_incoming,services->release_journal,
        services->release_capacity,&services->release);
    return services->release_status==NBA97_TEXT_COMPLETE;
}
