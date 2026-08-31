#include "recovered/voice_handles.h"
#include <array>
#include <cstdlib>
#include <iostream>
static unsigned checks;
static void check(bool ok){++checks;if(!ok)std::abort();}
struct Fixture {
    Nba97MusicVoiceClock clock{};
    std::array<Nba97MusicVoice,24> voice{};
    Nba97VoiceStopState stop{};
    Nba97VoiceHandles api{&clock,voice.data(),&stop,call,this,1};
    unsigned services=0,applies=0;
    bool retire=false;
    Fixture(){clock.rate=clock.cached_rate=120;clock.master_gain=127;
        stop.excluded_voice=UINT32_MAX;stop.tracked_stream=255;
        voice[3].handle=35;voice[3].active=1;voice[3].ramp_current=100u<<16;
        voice[3].envelope_current=127u<<16;voice[3].envelope_ticks=UINT32_MAX;voice[3].authored_gain=127;}
    static uint32_t call(void* p,Nba97MusicVoiceCall kind,uint32_t a,uint32_t,uint32_t){
        auto& f=*static_cast<Fixture*>(p);
        if(kind==NBA97_VOICE_HARDWARE_SERVICE){++f.services;if(f.retire)f.voice[3].active=0;}
        if(kind==NBA97_VOICE_APPLY)++f.applies;
        if(kind==NBA97_VOICE_STOP){auto r=nba97_voice_handle_stop(&f.api,a);check(r.completion==1);return static_cast<uint32_t>(r.value);}
        return 0;
    }
};
int main(){
    Fixture f;
    check(nba97_voice_handle_resolve(&f.api,35).value==3);
    check(nba97_voice_handle_status(&f.api,35).value==0);
    check(nba97_voice_handle_status(&f.api,67).value==1); // Same slot, different generation.
    f.api.enabled=0;check(nba97_voice_handle_status(&f.api,35).value==-10);
    check(nba97_voice_handle_fade(&f.api,35,20,128).value==-10); // Enabled gate comes first.
    f.api.enabled=255;check(nba97_voice_handle_fade(&f.api,35,20,128).value==-8);
    check(nba97_voice_handle_fade(&f.api,35,20,UINT32_MAX).value==0);
    check(f.voice[3].ramp_target==0xffff0000u&&f.voice[3].ramp_step!=0);
    check(nba97_voice_handle_gain(&f.api,35,17).value==0);
    check(f.voice[3].ramp_current==(17u<<16)&&!f.voice[3].ramp_step&&f.applies==1);
    check(f.voice[3].ramp_target==0xffff0000u); // Gain does not repair target.
    f.stop.channel[3]={2,37};
    check(nba97_voice_handle_stop(&f.api,35).value==0);
    check(f.stop.keyoff_mask==((1u<<3)|(1u<<5))&&f.voice[3].active==1);
    f.stop.tracked_stream=3;f.stop.keyoff_mask=0;
    check(nba97_voice_handle_stop(&f.api,35).value==0&&f.stop.stream_stop==1&&!f.stop.keyoff_mask);
    f.stop.excluded_voice=3;f.stop.stream_stop=0;f.stop.changing=7;
    check(nba97_voice_stop_request(&f.stop,3)==1&&!f.stop.stream_stop&&f.stop.changing==7);
    check(nba97_voice_stop_request(&f.stop,24)==0);
    Fixture pending;pending.clock.pending=1;pending.retire=true;
    check(nba97_voice_handle_status(&pending.api,35).value==0);
    check(!pending.voice[3].active&&pending.services==1&&!pending.clock.pending);
    check(nba97_voice_handle_status(&pending.api,35).value==1);
    Fixture bad;check(nba97_voice_handle_fade(&bad.api,24,20,0).completion==NBA97_VOICE_API_UNOWNED_SLOT);
    check(bad.clock.lock_depth==2); // No fake invalid-handle return/unlock after unowned read.
    Fixture underflow;check(nba97_voice_handle_unlock(&underflow.api).completion==1);
    check(underflow.clock.lock_depth==UINT32_MAX);
    Fixture trap;trap.clock.lock_depth=1;trap.clock.pending=1;trap.clock.rate=0;
    check(nba97_voice_handle_unlock(&trap.api).completion==NBA97_VOICE_API_TIMER_TRAP);
    check(trap.clock.in_service==1&&!trap.clock.pending&&!trap.clock.lock_depth);
    check(nba97_voice_handle_status(nullptr,35).completion==0);
    std::cout<<checks<<" voice handle and deferred service checks passed\n";
}
