#include "recovered/voice_channels.h"
#include "recovered/frontend_resource_cleanup.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
static unsigned checks;
static void check(bool ok){++checks;if(!ok)std::abort();}
struct Fixture {
    Nba97VoiceStopState stop{};
    std::array<Nba97MusicVoice,24> voices{};
    std::uint8_t finished=0;
    Nba97VoiceChannels channels{};
    std::array<std::uint32_t,24> status{};
    std::vector<std::array<std::uint32_t,3>> events;
    unsigned refuse=0;
    bool clear_mask=false;
    Fixture(){channels.stop=&stop;channels.voices=voices.data();channels.finished=&finished;
        stop.excluded_voice=UINT32_MAX;stop.tracked_stream=255;
        for(unsigned i=0;i<24;++i){voices[i].handle=32+i;voices[i].active=1;}}
    static int call(void* p,Nba97VoiceChannelCall kind,uint32_t a,uint32_t b,uint32_t* result){
        auto& f=*static_cast<Fixture*>(p);f.events.push_back({static_cast<uint32_t>(kind),a,b});
        if(f.refuse==f.events.size())return 0;
        *result=0;
        if(kind==NBA97_CHANNEL_STATUS_7BFA0){unsigned i=0;while((a>>i)!=1)++i;*result=f.status[i];}
        if(kind==NBA97_CHANNEL_KEY_6F858&&f.clear_mask){if(a)f.channels.keyon_mask=0;else f.stop.keyoff_mask=0;}
        return 1;
    }
    int run(){return nba97_voice_channels_service(&channels,call,this);}
};
struct Announcer {
    Fixture device;
    Nba97MusicVoiceClock clock{};
    Nba97VoiceHandles handles{};
    Nba97CoolIndexLoad resource{};
    unsigned polls=0,keyoffs=0;
    bool released=false;
    Announcer(){clock.rate=clock.cached_rate=120;clock.master_gain=127;
        for(auto& v:device.voices){v.active=0;v.envelope_ticks=UINT32_MAX;}
        auto& v=device.voices[3];v.active=1;v.handle=35;v.authored_gain=127;
        v.ramp_current=127u<<16;v.envelope_current=127u<<16;
        device.channels.state[3]=4;resource.voice=35;resource.bank=UINT32_MAX;
        handles={&clock,device.voices.data(),&device.stop,voice,this,1};}
    static int channel(void* p,Nba97VoiceChannelCall kind,uint32_t a,uint32_t b,uint32_t* out){
        auto& s=*static_cast<Announcer*>(p);*out=0;
        if(kind==NBA97_CHANNEL_STATUS_7BFA0)*out=s.released?0:1;
        if(kind==NBA97_CHANNEL_KEY_6F858&&!a&&(b&8)){
            // Test backend reports a real buffer return only after KEYOFF.
            check(s.device.voices[3].active==1);s.released=true;++s.keyoffs;
        }
        return 1;
    }
    static uint32_t voice(void* p,Nba97MusicVoiceCall kind,uint32_t a,uint32_t,uint32_t){
        auto& s=*static_cast<Announcer*>(p);
        if(kind==NBA97_VOICE_HARDWARE_SERVICE)
            check(nba97_voice_channels_service(&s.device.channels,channel,&s)==1);
        if(kind==NBA97_VOICE_STOP){auto r=nba97_voice_handle_stop(&s.handles,a);check(r.completion==1);return static_cast<uint32_t>(r.value);}
        return 0;
    }
    static uint32_t caller(void* p,Nba97FrontendCleanupCall kind,uint32_t a,uint32_t b,uint32_t c){
        auto& s=*static_cast<Announcer*>(p);Nba97VoiceApiResult r{1,0};
        if(kind==NBA97_FRONTEND_CLEANUP_VOICE_STATUS){
            // Deterministic interrupt fixture: one real pending timer callback
            // per poll. This is not a claim about native/original wall time.
            check(++s.polls<200);++s.clock.pending;r=nba97_voice_handle_status(&s.handles,a);
        }else if(kind==NBA97_FRONTEND_CLEANUP_FADE)r=nba97_voice_handle_fade(&s.handles,a,b,c);
        check(r.completion==1);return static_cast<uint32_t>(r.value);
    }
};
int main(){
    Fixture f;f.channels.keyon_mask=1u<<3;f.stop.channel[3]={2,4};
    check(f.run()==1&&f.channels.state[3]==2&&f.channels.state[4]==2);
    check(!f.channels.keyon_mask&&f.voices[3].active==1);
    f.status[3]=f.status[4]=1;check(f.run()==1&&f.channels.state[3]==4);
    f.status[3]=f.status[4]=3;check(f.run()==1&&f.channels.state[3]==1&&f.voices[3].active==1);
    check(!f.stop.keyoff_mask); // KEY batch has run, release is still pending.
    f.status[3]=f.status[4]=0;f.stop.tracked_stream=3;f.channels.transient[3]=7;
    check(f.run()==1&&!f.channels.state[3]&&!f.voices[3].active&&f.voices[3].handle==35);
    check(f.stop.tracked_stream==255&&f.finished==1&&!f.channels.transient[3]);
    Fixture changing;changing.stop.changing=1;
    check(changing.run()==1&&changing.channels.busy==1&&changing.events.size()==1);
    Fixture masked;masked.channels.keyon_mask=8;masked.clear_mask=true;
    check(masked.run()==1&&masked.channels.state[3]==0); // Mask reread after real callback.
    Fixture unknown;unknown.channels.state[3]=4;unknown.stop.channel[3]={3,24};
    check(unknown.run()==NBA97_CHANNEL_UNOWNED_PAIR);
    check(unknown.channels.busy==1&&unknown.channels.state[3]==1&&unknown.stop.keyoff_mask==8);
    Fixture fail;fail.channels.keyon_mask=8;fail.refuse=2;
    check(fail.run()==NBA97_CHANNEL_IO_REFUSED&&fail.channels.keyon_mask==8&&fail.channels.state[3]==0);
    Fixture excluded;excluded.channels.state[3]=4;excluded.stop.excluded_voice=3;
    check(excluded.run()==1&&excluded.channels.state[3]==4&&excluded.events.size()==1);
    Fixture stop;check(nba97_voice_stop_request(&stop.stop,3)==1);
    check(stop.run()==1&&stop.channels.state[3]==1&&stop.voices[3].active==1);
    check(stop.run()==1&&!stop.voices[3].active); // Actual reported hardware0, not STOP itself.
    check(nba97_voice_channels_service(nullptr,Fixture::call,&f)==0);
    Announcer chain;
    check(nba97_frontend_announcer_stop(&chain.resource,Announcer::caller,&chain)==1);
    check(chain.resource.voice==UINT32_MAX&&!chain.device.voices[3].active);
    check(chain.device.voices[3].handle==35&&chain.keyoffs==1&&chain.polls>1);
    std::cout<<checks<<" voice channel lifecycle and refusal checks passed\n";
}
