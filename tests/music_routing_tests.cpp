#include "recovered/music_routing.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
struct Host {
    std::vector<std::array<uint32_t,6>> calls;
    uint32_t clock=1000, ready=1, finished=0, busy=1;
    static uint32_t invoke(void* opaque,Nba97MusicCall call,uint32_t a,uint32_t b,
                           uint32_t c,uint32_t d,uint32_t e) {
        auto& h=*static_cast<Host*>(opaque);
        h.calls.push_back({static_cast<uint32_t>(call),a,b,c,d,e});
        if(call==NBA97_MUSIC_CLOCK) return h.clock++;
        if(call==NBA97_MUSIC_READY) return h.ready;
        if(call==NBA97_MUSIC_FINISHED) return h.finished;
        if(call==NBA97_MUSIC_BUSY) return h.busy;
        if(call==NBA97_MUSIC_ALLOCATE) return 301;
        if(call==NBA97_MUSIC_VOICE) return 707;
        return 0;
    }
    unsigned count(Nba97MusicCall call) const {
        unsigned n=0; for(const auto& c:calls) if(c[0]==static_cast<uint32_t>(call))++n; return n;
    }
};
}
int main() {
    try {
        Nba97MusicRouting s{}; Nba97MusicInputs input{}; Nba97MusicResources resources{};
        resources.initial=100; resources.pause=200;
        for(unsigned i=0;i<16;++i) resources.slots[i]=1000+i;
        Host host; uint16_t rng=0;
        auto step=[&] { require(nba97_music_routing_step(&s,&input,&resources,&rng,Host::invoke,&host)!=0,"step refused"); };
        s.deadline=17; s.updates=51; s.stop_clock=800;
        require(nba97_music_routing_init(&s,&resources,255,42,Host::invoke,&host)!=0,"init refused");
        require(s.phase==0 && s.current==100 && s.generation==1 && s.deadline==17 && s.updates==51 && s.stop_clock==800,"init scope");
        require(host.calls.back()[1]==42 && host.calls.back()[2]==127 && rng==0,"init gain/no RNG");
        step(); require(s.phase==2 && s.stream==301 && s.updates==0,"allocate/load");
        input.volume=8; host.calls.clear(); step();
        require(s.phase==3 && s.voice==707 && s.deadline==27501,"start clock follows pumps");
        require(host.count(NBA97_MUSIC_PUMP)==30 && host.count(NBA97_MUSIC_REFILL)==30,"thirty ordered warmup pumps");
        for(unsigned i=0;i<30;++i) require(host.calls[5+i*2][0]==NBA97_MUSIC_PUMP && host.calls[6+i*2][0]==NBA97_MUSIC_REFILL,"warmup order");
        require(host.calls[4][0]==NBA97_MUSIC_GAIN && host.calls[4][2]==120,"source gain");
        host.finished=1; step(); require(s.phase==4,"finish phase");
        step(); require(s.phase==1 && s.retire_deadline==0,"retirement");
        step(); require(s.phase==2 && rng==0x4b4a && s.current==1010,"zero RNG fallback/index10");
        s.phase=1; input.pause=1; const auto prior_rng=rng; step();
        require(s.current==200 && rng==prior_rng,"pause no draw");
        s.phase=1; s.override=4567; step(); require(s.current==4567 && s.override==0 && rng==prior_rng,"override precedence/consume");
        s.phase=1; input.selection_blocked=1; host.calls.clear(); step();
        require(host.calls.empty() && s.phase==1 && rng==prior_rng,"selection guard before readiness");
        input.selection_blocked=0; s.inhibited=1; const auto before=s; step();
        require(std::memcmp(&before,&s,sizeof(s))==0,"inhibition preserves updates");
        s.inhibited=0; input.pause=0; host.finished=0;
        s.phase=3; s.stopping=0; host.clock=500; s.deadline=500; host.calls.clear(); step();
        require(s.phase==3 && host.count(NBA97_MUSIC_FADE)==0,"strict deadline equality");
        s.phase=3; host.clock=0x80000000u; s.deadline=0x7fffffffu; host.calls.clear(); step();
        require(s.phase==3 && host.count(NBA97_MUSIC_FADE)==0,"preserve source signed wrap quirk");
        s.phase=10; host.clock=2000; step(); require(s.phase==3 && s.stopping==1 && s.stop_clock==2000,"stop request");
        host.busy=0; s.deadline=9000; step(); require(s.phase==11 && s.inhibited==1 && s.stopping==0,"stop completion");
        host.calls.clear(); step(); require(s.phase==11 && host.calls.empty(),"source inhibited case11 remains blocked");
        s.inhibited=0; step(); require(s.phase==12 && host.count(NBA97_MUSIC_FREE)==1,"caller permits free");
        const auto saved=s; const auto saved_calls=host.calls.size();
        require(!nba97_music_routing_step(&s,nullptr,&resources,&rng,Host::invoke,&host),"null refusal");
        require(std::memcmp(&s,&saved,sizeof(s))==0 && host.calls.size()==saved_calls,"null no side effects");
        std::cout<<"Music routing lifecycle, guards, source quirks and callback order passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
