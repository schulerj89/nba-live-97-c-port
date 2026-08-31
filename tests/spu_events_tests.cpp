#include "recovered/spu_events.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value,int line) {
    ++checks;if(!value) { std::fprintf(stderr,"spu events check %u failed at line %d\n",checks,line);std::exit(1); }
}
#define check(value) check_at((value),__LINE__)
constexpr uint32_t Base=0x800c7500u,Guard=0x800c7a80u,Handle=0x800c7678u;
constexpr uint32_t Table=0x800c7dacu,MaskPointer=0x800c7e30u,Ready=0x800c7e38u,Callbacks=0x800c7e3cu;
constexpr uint32_t DmaPointer=0x800c7e5cu,IrqMask=0x1f801074u,DmaControl=0x1f8010f4u,Data=0x80110000u;
struct Fixture {
    std::array<uint8_t,0xa00> bytes{},known{};
    std::array<uint8_t,32> data{},dataKnown{};
    std::array<Nba97VoicePatlSpan,2> spans{};
    std::array<Nba97SpuEventsEvent,256> journal{};
    std::map<uint32_t,uint32_t> device;
    Nba97SpuEvents owner{};Nba97SpuEventsProgress progress{};
    std::function<int(const Nba97SpuEventsEvent&,Nba97SpuTransferValue&)> hook;
    std::vector<Nba97SpuEventsKind> calls;
    uint32_t opened=0x12345678;
    Fixture() {
        bytes.fill(0xcd);data.fill(0xa5);
        spans[0]={bytes.data(),known.data(),bytes.size(),Base,1,1,0};
        spans[1]={data.data(),dataKnown.data(),data.size(),Data,1,1,0};
        owner={{spans.data(),spans.size()},io,this,10000};
        // Explicit earlier SDK/interrupt-controller entry; this fixture does
        // not claim that ResetCallback/hardware startup ran in the host.
        put(Guard,0);put(Handle,0xdeadbeef);put(0x800c7dc4u,Table);put(Table+4,0x8007fdb8u);
        put(Ready,1,2);put(Callbacks+16,0);put(MaskPointer,IrqMask);put(DmaPointer,DmaControl);
        put(0x800c75fcu,0x80012340);put(0x800c7600u,0x80012344);
        device[IrqMask]=0x7ff;device[DmaControl]=0x12654321;
    }
    void put(uint32_t at,uint32_t value,uint32_t width=4) { check(nba97_voice_patl_write(&owner.memory,at,width,value)==1); }
    uint32_t get(uint32_t at,uint32_t width=4) { uint32_t v=0;check(nba97_voice_patl_read(&owner.memory,at,width,&v)==1);return v; }
    static int io(void* context,const Nba97VoicePatlMemory* memory,const Nba97SpuEventsEvent* event,Nba97SpuTransferValue* value) {
        auto& f=*static_cast<Fixture*>(context);check(memory==&f.owner.memory);
        f.calls.push_back(event->kind);
        if(f.hook) { int rc=f.hook(*event,*value);if(rc!=-99)return rc; }
        switch(event->kind) {
        case NBA97_SPU_EVENTS_DEVICE_READ:
            if(event->address>=Base&&event->address<0x80200000u)
                return nba97_voice_patl_read(memory,event->address,event->width,&value->word)==1?(value->known=1,1):0;
            if(f.device.find(event->address)==f.device.end())return 0;
            *value={f.device[event->address],1};return 1;
        case NBA97_SPU_EVENTS_DEVICE_WRITE:
            if(event->address>=Base&&event->address<0x80200000u)
                return nba97_voice_patl_write(memory,event->address,event->width,event->value)==1;
            f.device[event->address]=event->value;return 1;
        case NBA97_SPU_EVENTS_OPEN_EVENT:
            check(event->argument[0]==0xf0000009u&&event->argument[1]==0x20&&event->argument[2]==0x2000&&event->argument[3]==0);
            *value={f.opened,1};return 1;
        case NBA97_SPU_EVENTS_ENTER_CRITICAL:
            check(event->argument[0]==1);*value={1,1};return 1;
        case NBA97_SPU_EVENTS_EXIT_CRITICAL:
            check(event->argument[0]==2);*value={0xabcdef01,0};return 1;
        default:*value={0,1};return 1; // Explicit scripted BIOS/dispatch result.
        }
    }
    int run(Nba97SpuEventsOperation op,uint32_t a0=0,uint32_t a1=0,uint32_t a2=0,uint32_t a3=0,size_t capacity=256) {
        return nba97_spu_events(&owner,op,a0,a1,a2,a3,journal.data(),capacity,&progress);
    }
};
void initialize_shutdown() {
    Fixture f;check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==1&&f.progress.completed&&!f.progress.returned.known);
    check(f.get(Guard)==1&&f.get(Handle)==f.opened&&f.get(0x800c7620u)==0);
    check(f.get(Callbacks+16)==0x8007d668u&&f.device[IrqMask]==0x7ff);
    check(f.device[DmaControl]==0x00f54321u); // Literal source mask/enable write.
    check(f.journal[0].pc==0x8007e4e0u&&f.journal[0].value==1);
    f.calls.clear();check(f.run(NBA97_SPU_EVENTS_SHUTDOWN_7E81C)==1);
    check(f.get(Guard)==0&&f.get(Callbacks+16)==0&&f.get(0x800c75fcu)==0&&f.get(0x800c7600u)==0);
    check(f.get(Handle)==f.opened&&f.device[IrqMask]==0x7ff&&f.device[DmaControl]==0x00e54321u);
    auto close=std::find(f.calls.begin(),f.calls.end(),NBA97_SPU_EVENTS_CLOSE_EVENT);
    check(close!=f.calls.end()&&close+1!=f.calls.end()&&*(close+1)==NBA97_SPU_EVENTS_DISABLE_EVENT);
    check(!f.progress.returned.known); // ExitCritical's raw result stays unavailable.
}
void guards_and_source_errors() {
    for(uint32_t value:{1u,2u,0xffffffffu,0x80000000u}) {
        Fixture f;f.put(Guard,value);f.owner.io=nullptr;
        check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4,0,0,0,0,0)==1);
        check(f.progress.returned.word==value&&f.progress.returned.known&&f.progress.events==0);
    }
    for(uint32_t value:{0u,2u,0xffffffffu}) {
        Fixture f;f.put(Guard,value);f.owner.io=nullptr;
        check(f.run(NBA97_SPU_EVENTS_SHUTDOWN_7E81C,0,0,0,0,0)==1);
        check(f.progress.returned.word==1&&f.progress.stores==0);
    }
    for(uint32_t handle:{0u,0xffffffffu}) {
        Fixture f;f.opened=handle;check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==1);
        check(f.get(Handle)==handle&&f.get(Guard)==1);
        auto it=std::find_if(f.journal.begin(),f.journal.begin()+f.progress.events,
            [](const auto& e) { return e.kind==NBA97_SPU_EVENTS_ENABLE_EVENT; });
        check(it!=f.journal.begin()+f.progress.events&&it->argument[0]==handle);
    }
    {Fixture f;f.put(Ready,0,2);check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==1);
        // Original still opens/enables an event when the earlier DMA callback
        // subsystem declines registration. Do not force callback readiness.
        check(f.get(Callbacks+16)==0&&f.get(Guard)==1&&f.get(Handle)==f.opened);}
    {Fixture f;f.owner.io=nullptr;
        check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==NBA97_PATL_IO_REFUSED&&f.get(Guard)==1);
        // A fresh invocation follows the published guard and skips. This is
        // the original state, not permission to resume the refused operation.
        check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==1&&f.progress.returned.word==1);}
}
void registration_aliases() {
    {Fixture f;f.put(Callbacks+16,0x8007d668u);f.known[Ready-Base]=0;f.owner.io=nullptr;
        check(f.run(NBA97_SPU_EVENTS_REGISTER_7E548,0x8007d668u)==1);
        check(f.progress.returned.word==0x8007d668u&&f.progress.events==0);}
    {Fixture f;f.put(Ready,0,2);f.owner.io=nullptr;
        check(f.run(NBA97_SPU_EVENTS_SET_CALLBACK_7FDB8,4,0x80012340)==1);
        check(f.progress.returned.word==0&&f.progress.stores==0);}
    {Fixture f;
        // Channel8's callback slot IS C7E5C. The source caches its original
        // device pointer before overwriting this retained slot.
        check(f.run(NBA97_SPU_EVENTS_SET_CALLBACK_7FDB8,8,Data)==1);
        check(f.get(DmaPointer)==Data&&f.device[DmaControl]==0x01e54321u);
        check(f.progress.returned.word==DmaControl&&f.device[IrqMask]==0x7ff);}
    {Fixture f;f.put(Data,0x4567,2);
        // Wrapped channel arithmetic reaches C7E30. Restoration reloads the
        // now-changed pointer and writes the saved mask into that RAM alias.
        check(f.run(NBA97_SPU_EVENTS_SET_CALLBACK_7FDB8,0xfffffffdu,Data)==1);
        check(f.get(MaskPointer)==Data&&f.get(Data,2)==0x7ff&&f.device[IrqMask]==0);
        check(f.progress.returned.word==IrqMask);}
    {Fixture f;f.put(Table+4,0);
        check(f.run(NBA97_SPU_EVENTS_REGISTER_7E548,0x80012340)==1);
        const auto& event=f.journal[0];
        check(event.kind==NBA97_SPU_EVENTS_OTHER_DISPATCH&&event.address==0&&event.argument[0]==4&&event.argument[1]==0x80012340);
        check(f.progress.stores==0);}
}
void live_handles_and_knownness() {
    {Fixture f;f.put(Guard,1);uint32_t seen=0;
        f.hook=[&](const auto& e,auto& value) {
            if(e.kind==NBA97_SPU_EVENTS_CLOSE_EVENT) { f.put(Handle,0x98765432);value={0,0};return 1; }
            if(e.kind==NBA97_SPU_EVENTS_DISABLE_EVENT)seen=e.argument[0];
            return -99;
        };
        check(f.run(NBA97_SPU_EVENTS_SHUTDOWN_7E81C)==1&&seen==0x98765432&&f.get(Handle)==seen);}
    {Fixture f;f.hook=[](const auto& e,auto& value) {
        if(e.kind==NBA97_SPU_EVENTS_OPEN_EVENT) { value={0x87654321,0};return 1; }return -99;
        };
        check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==NBA97_PATL_RESOURCE);
        check(f.get(Guard)==1&&f.get(Callbacks+16)==0x8007d668u&&f.get(Handle)==0xdeadbeef);
        check(f.journal[f.progress.events-1].kind==NBA97_SPU_EVENTS_OPEN_EVENT&&f.journal[f.progress.events-1].completed);}
    {Fixture f;f.hook=[](const auto&,auto& value) { value={0,2};return 1; };
        check(f.run(NBA97_SPU_EVENTS_ENTER_7F308)==NBA97_PATL_METADATA&&f.progress.callbacks_completed==1);
        check(f.journal[0].completed&&!f.progress.completed);}
    {Fixture f;f.hook=[](const auto& e,auto& value) {
        if(e.kind==NBA97_SPU_EVENTS_DEVICE_READ) { value={0,0};return 1; }return -99;
        };
        check(f.run(NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,0)==NBA97_PATL_RESOURCE);
        check(f.device[IrqMask]==0x7ff&&f.progress.callbacks_completed==1);}
}
void limits() {
    Fixture baseline;check(baseline.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==1);
    const size_t events=baseline.progress.events,accesses=baseline.progress.accesses;
    for(size_t capacity=0;capacity<events;++capacity) {
        Fixture f;check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4,0,0,0,0,capacity)==NBA97_SPU_EVENTS_LIMIT);
        check(f.progress.events==capacity&&!f.progress.completed);
        for(size_t i=0;i<capacity;++i) {
            const auto& a=f.journal[i];const auto& b=baseline.journal[i];
            check(a.completed&&a.pc==b.pc&&a.kind==b.kind&&a.address==b.address&&a.value==b.value);
        }
    }
    for(size_t budget=0;budget<accesses;++budget) {
        Fixture f;f.owner.access_budget=budget;
        check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==NBA97_SPU_EVENTS_LIMIT);
        check(f.progress.accesses==budget&&!f.progress.completed);
    }
    {Fixture f;f.put(Table+4,0x8007fdb8);f.known[Callbacks+16-Base+3]=0;
        check(f.run(NBA97_SPU_EVENTS_REGISTER_7E548,0x80012340)==NBA97_PATL_RESOURCE&&f.progress.events==0);}
    {Fixture f;check(nba97_spu_events(nullptr,NBA97_SPU_EVENTS_INITIALIZE_7E4C4,0,0,0,0,nullptr,0,&f.progress)==NBA97_PATL_ARGUMENT);}
}
}
int main() {
    initialize_shutdown();guards_and_source_errors();registration_aliases();live_handles_and_knownness();limits();
    std::printf("spu events: %u checks passed\n",checks);
}
