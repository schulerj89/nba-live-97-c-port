#include "recovered/frontend_resource_cleanup.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

static void check(bool ok) { if (!ok) std::abort(); }
struct Fixture {
    Nba97FrontendResourceCleanup resource{11,{{3,21,31},{4,22,0}},41};
    Nba97CoolIndexLoad sound{51,61,71,81,91,101,111,121};
    uint32_t cool=131;
    unsigned statuses=0;
    bool finished=false;
    std::vector<std::array<uint32_t,4>> calls;
};
static uint32_t call(void* p,Nba97FrontendCleanupCall kind,uint32_t a,uint32_t b,uint32_t c) {
    auto& f=*static_cast<Fixture*>(p);f.calls.push_back({static_cast<uint32_t>(kind),a,b,c});
    if(kind==NBA97_FRONTEND_CLEANUP_VOICE_STATUS) return f.finished || ++f.statuses>=3;
    if(kind==NBA97_FRONTEND_CLEANUP_BUFFER_WAIT) {
        check(f.sound.pending==0 && !f.cool && !f.resource.portrait_index);
        check(f.sound.graphics==61 && f.resource.portrait[0].data==21);
    }
    return 0;
}
int main() {
    Fixture f;
    check(nba97_frontend_resource_cleanup(&f.resource,&f.sound,&f.cool,800,call,&f)==1);
    check(f.calls==std::vector<std::array<uint32_t,4>>{
        {NBA97_FRONTEND_CLEANUP_DRAIN,0,0,0},
        {NBA97_FRONTEND_CLEANUP_FREE_DATA,11,0,0},
        {NBA97_FRONTEND_CLEANUP_CANCEL,20,0,0},
        {NBA97_FRONTEND_CLEANUP_VOICE_STATUS,81,0,0},
        {NBA97_FRONTEND_CLEANUP_FADE,81,20,UINT32_MAX},
        {NBA97_FRONTEND_CLEANUP_VOICE_STATUS,81,0,0},
        {NBA97_FRONTEND_CLEANUP_VOICE_STATUS,81,0,0},
        {NBA97_FRONTEND_CLEANUP_UNLOAD_BANK,111,91,0},
        {NBA97_FRONTEND_CLEANUP_FREE_DATA,101,0,0},
        {NBA97_FRONTEND_CLEANUP_FREE_DATA,131,0,0},
        {NBA97_FRONTEND_CLEANUP_BUFFER_WAIT,800,480,0x8003282c},
        {NBA97_FRONTEND_CLEANUP_SYNC,0,0,0},
        {NBA97_FRONTEND_CLEANUP_FREE_DATA,21,0,0}, // No free22: original graphic0 quirk.
        {NBA97_FRONTEND_CLEANUP_FREE_DATA,41,0,0},
        {NBA97_FRONTEND_CLEANUP_SYNC,0,0,0},
        {NBA97_FRONTEND_CLEANUP_HARDWARE_WAIT,0,0,0}});
    check(f.resource.portrait[0].physical_record==3 && f.resource.portrait[1].physical_record==4);
    check(!f.resource.portrait[1].data && !f.resource.card_data && f.sound.voice==UINT32_MAX);
    check(f.sound.bank==UINT32_MAX && f.sound.loaded_data==51 && f.sound.archive_path==121);
    Fixture done;done.finished=true;
    check(nba97_frontend_announcer_stop(&done.sound,call,&done)==0);
    check(done.sound.voice==81 && done.sound.bank==UINT32_MAX && !done.sound.sample_data);
    Fixture live;check(nba97_frontend_announcer_stop(&live.sound,call,&live)==1);
    check(live.sound.voice==UINT32_MAX && live.sound.pending==71); //313C8 does not clear FDC00.
    Fixture negative;negative.sound.voice=negative.sound.bank=UINT32_MAX;negative.sound.sample_data=0;
    check(nba97_frontend_announcer_stop(&negative.sound,call,&negative)==0 && negative.calls.size()==1);
    Fixture invalid;
    check(nba97_frontend_announcer_stop(nullptr,call,&invalid)==-1);
    check(!nba97_frontend_resource_cleanup(&invalid.resource,&invalid.sound,nullptr,800,call,&invalid));
    check(invalid.calls.empty());
    std::cout<<"Original frontend cleanup ordering, conditional frees and announcer quirks passed\n";
}
