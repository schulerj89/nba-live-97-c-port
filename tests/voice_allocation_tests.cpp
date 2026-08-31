#include "recovered/voice_allocation.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
unsigned checks;
void check(bool b){++checks;if(!b)throw std::runtime_error("allocation check "+std::to_string(checks));}
struct Fixture {
    Nba97MusicVoiceClock clock{};Nba97MusicVoice voices[24]{};Nba97VoiceStopState stop{};
    Nba97VoiceHandles handles{};Nba97VoiceAllocationRecord records[24]{};
    uint32_t generation=0,membership=8,out=0xabcdef01;
    uint8_t selected[8]{};Nba97VoiceAllocation state{};
    unsigned services=0;bool clear_on_service=false;
    static uint32_t callback(void* p,Nba97MusicVoiceCall call,uint32_t,uint32_t,uint32_t){
        auto& f=*static_cast<Fixture*>(p);
        if(call==NBA97_VOICE_HARDWARE_SERVICE){++f.services;if(f.clear_on_service)f.voices[0].active=0;}
        return 0;
    }
    Fixture(){
        clock.rate=clock.cached_rate=120;clock.services=91;clock.master_gain=127;
        stop.excluded_voice=0xffffffff;stop.tracked_stream=255;
        handles={&clock,voices,&stop,callback,this,1};
        state={&handles,records,&generation,&membership,selected};
        std::memset(selected,255,sizeof selected);
        for(unsigned i=0;i<24;++i){voices[i].handle=0x400+i;voices[i].envelope_ticks=0xffffffff;voices[i].envelope_current=127u<<16;records[i].priority=50;records[i].age=100;}
    }
    Nba97VoiceApiResult allocate(uint32_t mask,uint32_t n,uint32_t priority){return nba97_voice_allocate(&state,mask,n,priority,&out);}
};
void freeAndLinked(){
    Fixture f;auto r=f.allocate(0x28,2,71);check(r.completion==1&&r.value==3&&f.out==35&&f.generation==32);
    check(f.selected[0]==3&&f.selected[1]==5&&f.records[3].linked[0]==5);
    check(f.voices[3].handle==35&&f.voices[5].handle==0xffffffff&&f.voices[3].active==1&&f.voices[5].active==1);
    check(f.records[3].priority==71&&f.records[5].age==91&&f.clock.lock_depth==0);
    f.generation=0x7fffffe0;r=f.allocate(1,1,2);check(r.value==0&&f.generation==0&&f.out==0);
    check(f.voices[0].active==1&&f.voices[0].handle==0); // Handle0 is possible in the source.
}
void priorityAndSteal(){
    Fixture f;for(auto& v:f.voices)v.active=1;
    f.records[7].priority=f.records[9].priority=20;f.records[7].age=100;f.records[9].age=0xffffffff;
    auto r=f.allocate(0xffffff,1,20);check(r.completion==1&&r.value==-9&&f.selected[0]==9);
    check(f.out==41&&f.voices[9].handle==41&&f.stop.keyoff_mask==(1u<<9)&&f.voices[9].active==1);
    check(f.records[9].age==0xffffffff&&f.generation==32&&f.clock.lock_depth==0);
    Fixture low;for(auto& v:low.voices)v.active=1;
    r=low.allocate(1,1,49);check(r.value==-9&&low.out==0xabcdef01&&low.generation==32&&!low.stop.keyoff_mask);
    // Sentinel bug: priority102 accepts defaultvoice0 despite an empty mask.
    Fixture sentinel;r=sentinel.allocate(0,1,102);check(r.value==0&&sentinel.out==32&&sentinel.voices[0].active==1);
    Fixture state2;state2.voices[4].active=2;r=state2.allocate(1u<<4,1,50);
    check(r.value==4&&state2.voices[4].active==1&&!state2.stop.keyoff_mask);
}
void partialAndCount(){
    Fixture f;f.voices[2].active=1;f.records[2].priority=10;
    auto r=f.allocate(5,2,50);check(r.value==-9&&f.selected[0]==0&&f.selected[1]==2);
    check(f.voices[0].active==1&&f.voices[0].handle==32&&f.voices[2].handle==0x402);
    check(f.stop.keyoff_mask==4&&f.records[0].age==91&&f.out==32);
    Fixture duplicate;duplicate.membership=0;
    r=duplicate.allocate(1,2,102);check(r.value==-9&&duplicate.selected[0]==0&&duplicate.selected[1]==0);
    check(duplicate.voices[0].active==1&&duplicate.stop.keyoff_mask==1&&duplicate.membership==0);
    Fixture zero;zero.selected[0]=7;r=zero.allocate(0,0,0);
    check(r.value==7&&zero.generation==32&&zero.out==39&&zero.voices[7].handle==39&&!zero.voices[7].active);
    Fixture negative;r=negative.allocate(0,0xffffffff,0);check(r.value==-9&&negative.generation==32&&negative.out==0xabcdef01);
}
void refusalAndService(){
    Fixture tooMany;auto r=tooMany.allocate(0xffffff,9,50);
    check(r.completion==NBA97_ALLOCATION_UNOWNED_SCRATCH&&tooMany.clock.lock_depth==1&&tooMany.generation==0);
    check(tooMany.out==0xabcdef01&&tooMany.selected[7]==255);
    Fixture count;count.membership=9;r=count.allocate(1,1,50);
    check(r.completion==NBA97_ALLOCATION_UNOWNED_SCRATCH&&count.generation==32&&count.clock.lock_depth==1);
    Fixture stale;stale.selected[0]=255;r=stale.allocate(0,0,0);
    check(r.completion==NBA97_ALLOCATION_UNOWNED_SLOT&&stale.out==0xffffffff&&stale.clock.lock_depth==1);
    Fixture disabled;disabled.handles.enabled=0;r=disabled.allocate(1,1,2);
    check(r.value==-9&&disabled.out==32&&disabled.voices[0].handle==32&&!disabled.voices[0].active);
    Fixture pending;pending.clock.services=0;pending.clock.pending=1;pending.clear_on_service=true;r=pending.allocate(1,1,2);
    check(r.value==0&&r.completion==1&&pending.services==1&&!pending.voices[0].active);
    check(pending.out==32&&!pending.clock.lock_depth&&!pending.clock.pending);
    r=nba97_voice_allocate(nullptr,1,1,1,nullptr);check(r.completion==NBA97_ALLOCATION_ARGUMENT);
}
}
int main(){try{freeAndLinked();priorityAndSteal();partialAndCount();refusalAndService();std::cout<<checks<<" source allocation/generation checks passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
