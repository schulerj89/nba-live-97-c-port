#include "recovered/spu_initialize.h"
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
void check_at(bool ok,int line) { ++checks;if(!ok) { std::fprintf(stderr,"spu initialize check %u at %d\n",checks,line);std::exit(1); } }
#define check(v) check_at(bool(v),__LINE__)
constexpr uint32_t Base=0x800c7500u,Spu=0x1f801c00u,Dpcr=0x1f8010f0u;
struct Fixture {
    std::array<uint8_t,0xa00> bytes{},known{};
    Nba97VoicePatlSpan span{};Nba97SpuInitialize owner{};Nba97SpuInitializeProgress progress{};
    std::vector<Nba97SpuInitializeEvent> journal=std::vector<Nba97SpuInitializeEvent>(30000);
    std::map<uint32_t,uint32_t> device;
    std::function<int(const Nba97SpuInitializeEvent&,Nba97SpuTransferValue&)> hook;
    unsigned pio=0,events=0,controller=0,diagnostics=0;
    Fixture() {
        bytes.fill(0xcd);span={bytes.data(),known.data(),bytes.size(),Base,1,1,0};
        owner={{&span,1},io,this,100000};
        put(0x800c75c8u,Spu);put(0x800c75d8u,Dpcr);put(0x800c7a90u,0xfffe);
        put(0x800c7dc4u,0x800c7dacu);put(0x800c7db8u,0x8007f708u);
        device[Dpcr]=0x12345678;device[Spu+0x1ae]=0;
        device[Spu+0x188]=0x5678;device[Spu+0x18a]=0x1200;
    }
    void put(uint32_t at,uint32_t v,uint32_t w=4) { check(nba97_voice_patl_write(&owner.memory,at,w,v)==1); }
    uint32_t get(uint32_t at,uint32_t w=4) { uint32_t v=0;check(nba97_voice_patl_read(&owner.memory,at,w,&v)==1);return v; }
    static int io(void* p,const Nba97VoicePatlMemory* m,const Nba97SpuInitializeEvent* e,Nba97SpuTransferValue* v) {
        auto& f=*static_cast<Fixture*>(p);check(m==&f.owner.memory);
        if(f.hook) { int rc=f.hook(*e,*v);if(rc!=-99)return rc; }
        switch(e->kind) {
        case NBA97_SPU_INITIALIZE_DEVICE_READ:
            if(e->address>=Base&&e->address<Base+f.bytes.size())
                return nba97_voice_patl_read(m,e->address,e->width,&v->word)==1?(v->known=1,1):0;
            if(!f.device.count(e->address))return 0;
            *v={f.device[e->address],1};return 1;
        case NBA97_SPU_INITIALIZE_DEVICE_WRITE:
            if(e->address>=Base&&e->address<Base+f.bytes.size())return nba97_voice_patl_write(m,e->address,e->width,e->value)==1;
            f.device[e->address]=e->value;return 1;
        case NBA97_SPU_INITIALIZE_CONTROLLER:++f.controller;*v={0xabcdef01,0};return 1;
        case NBA97_SPU_INITIALIZE_EVENTS:++f.events;*v={0xabcdef01,0};return 1;
        case NBA97_SPU_INITIALIZE_PIO:
            ++f.pio;check(e->argument[0]==0x800c7604u&&e->argument[1]==16);
            *v={0xfffffffeu,1};return 1; // Scripted source timeout, intentionally ignored.
        case NBA97_SPU_INITIALIZE_DIAGNOSTIC:
            ++f.diagnostics;check(e->argument[0]==0x80027dd0u&&e->argument[1]==0x80027de0u);
            *v={0xfedcba98,0};return 1;
        default:return 0;
        }
    }
    int run(Nba97SpuInitializeOperation op=NBA97_SPU_INITIALIZE_7E6EC,uint32_t a0=0,uint32_t a1=0,uint32_t a2=0,size_t cap=30000) {
        return nba97_spu_initialize(&owner,op,a0,a1,a2,journal.data(),cap,&progress);
    }
};
void ordinary_and_modes() {
    Fixture f;check(f.run()==1&&f.progress.completed&&!f.progress.transferred&&f.progress.stopped_pc==0);
    check(f.progress.returned.known&&f.progress.returned.word==Spu+0x1a2);
    check(f.controller==1&&f.events==1&&f.pio==1&&!f.diagnostics);
    check(f.device[Dpcr]==(0x12345678u|0xb0000u)&&f.device[Spu+0x1aa]==0xc000);
    check(f.get(0x800c75ecu)==3&&f.get(0x800c75e8u)==2&&f.get(0x800c75f4u)==7);
    check(f.get(0x800c75c4u,2)==0x200&&f.device[Spu+0x1a2]==0xfffe);
    for(uint32_t i=0;i<24;++i) {
        check(f.get(0x800c7648u+i*2,2)==0xc000);
        check(f.device[Spu+i*16]==0&&f.device[Spu+i*16+2]==0);
        check(f.device[Spu+i*16+4]==0x3fff&&f.device[Spu+i*16+6]==0x200);
        check(f.device[Spu+i*16+8]==0&&f.device[Spu+i*16+10]==0);
        check(!f.device.count(Spu+i*16+12)&&!f.device.count(Spu+i*16+14));
    }
    check(f.known[0x178]==0); // Event import must own the real handle; no made-up one here.
    check(f.get(0x800c7630u)==0xfffe&&f.get(0x800c75f8u)==1);
    for(uint32_t mode:{1u,0xffffffffu,0x80000000u}) {
        Fixture g;check(g.run(NBA97_SPU_INITIALIZE_MODE_7E3FC,mode)==1);
        check(!g.pio&&g.events==1&&g.controller==1&&g.get(0x800c75c4u,2)==0);
        check(!g.device.count(Spu)&&!g.device.count(Spu+0x190)&&g.known[0x148]==0);
    }
}
void timeouts_and_mutations() {
    {Fixture f;f.device[Spu+0x1ae]=0x400;
        check(f.run()==1&&f.diagnostics==1&&f.get(0x800c75c0u)==5001&&f.pio==1);
        check(std::count_if(f.journal.begin(),f.journal.begin()+f.progress.events,[](const auto& e){return e.pc==0x8007cf44u;})==5000);}
    {Fixture f;f.hook=[&](const auto& e,auto&) {
        if(e.pc==0x8007cedcu)f.put(0x800c75c0u,9000); // Cleared AFTER this first read.
        if(e.pc==0x8007cf44u)f.put(0x800c75c0u,5000);
        return -99;
    };f.device[Spu+0x1ae]=1;check(f.run()==1&&f.diagnostics==1&&f.get(0x800c75c0u)==5001);}
    {Fixture f;f.hook=[&](const auto& e,auto&) {
        if(e.kind==NBA97_SPU_INITIALIZE_EVENTS)f.put(0x800c7a90u,0xabcd1234);
        if(e.pc==0x8007dd98u)f.put(0x800c75c8u,0x12345678);
        return -99;
    };check(f.run()==1&&f.get(0x800c7630u)==0xabcd1234&&f.device[Spu+0x1a2]==0x1234);
        check(f.progress.returned.word==Spu+0x1a2);}
    {Fixture f;f.hook=[&](const auto& e,auto& v) {
        if(e.pc==0x8007cf94u)f.put(0x800c75c8u,Spu+0x200);
        if(e.kind==NBA97_SPU_INITIALIZE_DEVICE_READ&&e.address==Spu+0x200+0x188) {v={0,1};return 1;}
        if(e.kind==NBA97_SPU_INITIALIZE_DEVICE_READ&&e.address==Spu+0x200+0x18a) {v={0,1};return 1;}
        if(e.kind==NBA97_SPU_INITIALIZE_DEVICE_READ&&e.address==Spu+0x200+0x18c) {v={0,1};return 1;}
        if(e.kind==NBA97_SPU_INITIALIZE_DEVICE_READ&&e.address==Spu+0x200+0x18e) {v={0,1};return 1;}
        return -99;
    };check(f.run()==1);check(f.device[Spu+0x184]==0&&f.device[Spu+0x1b6]==0);
        check(f.device[Spu+0x204]==0x3fff&&f.progress.returned.word==Spu+0x3a2);}
}
void register_and_dispatch() {
    for(uint32_t shift:{0u,1u,3u,31u,32u,63u,0xffffffffu}) {
        Fixture f;f.put(0x800c75ecu,shift);
        check(f.run(NBA97_SPU_INITIALIZE_REGISTER_7DD80,0xd1,0x87654321,1)==1);
        check(f.device[Spu+0x1a2]==((0x87654321u>>(shift&31u))&0xffffu));
        check(f.progress.returned.word==Spu+0x1a2);
    }
    {Fixture f;f.put(0x800c75c8u,Base);check(f.run(NBA97_SPU_INITIALIZE_REGISTER_7DD80,0x80000064u,0x1234)==1);
        check(f.get(Base+0xc8,2)==0x1234&&f.progress.returned.word==Base+0xc8);}
    {Fixture f;f.put(0x800c7db8u,0);check(f.run(NBA97_SPU_INITIALIZE_CONTROLLER_7F5D0)==1);
        check(f.journal[0].address==0&&!f.progress.returned.known&&f.progress.returned.word==0xabcdef01);}
    {Fixture f;f.hook=[](const auto& e,auto&) {return e.kind==NBA97_SPU_INITIALIZE_CONTROLLER?2:-99;};
        check(f.run()==2&&f.progress.completed&&f.progress.transferred&&f.progress.events==1&&!f.pio&&!f.events);
        check(f.progress.stopped_pc==0x8007f5e8u&&f.journal[0].transferred);}
}
void boundaries() {
    Fixture full;check(full.run()==1);
    for(size_t cap=0;cap<full.progress.events;++cap) {
        Fixture f;check(f.run(NBA97_SPU_INITIALIZE_7E6EC,0,0,0,cap)==NBA97_SPU_INITIALIZE_LIMIT);
        check(!f.progress.completed&&f.progress.events==cap);
        for(size_t j=0;j<cap;++j)check(f.journal[j].pc==full.journal[j].pc&&f.journal[j].value==full.journal[j].value);
    }
    for(size_t budget=0;budget<full.progress.accesses;++budget) {
        Fixture f;f.owner.access_budget=budget;check(f.run()==NBA97_SPU_INITIALIZE_LIMIT&&f.progress.accesses==budget);
    }
    for(auto pc:{0x8007f5e8u,0x8007cff4u,0x8007d048u,0x8007d1a4u,0x8007e43cu,0x8007dd98u}) {
        Fixture f;f.hook=[=](const auto& e,auto&){return e.pc==pc?0:-99;};
        check(f.run()==NBA97_PATL_IO_REFUSED&&f.progress.stopped_pc==pc&&!f.progress.completed);
        check(!f.journal[f.progress.events-1].completed);
    }
    for(auto kind:{NBA97_SPU_INITIALIZE_CONTROLLER,NBA97_SPU_INITIALIZE_DEVICE_READ,NBA97_SPU_INITIALIZE_PIO,NBA97_SPU_INITIALIZE_EVENTS}) {
        Fixture f;f.hook=[=](const auto& e,auto& v){if(e.kind==kind){v={0,2};return 1;}return -99;};
        check(f.run()==NBA97_PATL_METADATA&&f.journal[f.progress.events-1].completed);
    }
    {Fixture f;f.hook=[](const auto& e,auto&){return e.kind==NBA97_SPU_INITIALIZE_PIO?2:-99;};
        check(f.run()==NBA97_PATL_METADATA&&!f.progress.completed&&f.journal[f.progress.events-1].transferred);}
    {Fixture f;f.hook=[](const auto&,auto&){return 3;};check(f.run()==NBA97_PATL_IO_REFUSED&&!f.journal[0].completed);}
    {Fixture f;f.owner.io=nullptr;check(f.run()==NBA97_PATL_IO_REFUSED);}
    {Fixture f;f.known[0x8c4]=0;check(f.run()==NBA97_PATL_RESOURCE&&f.progress.events==0);}
    {Fixture f;f.put(0x800c75c8u,Spu+1);check(f.run()==NBA97_PATL_RESOURCE&&f.progress.stopped_pc==0x8007ce4cu);}
}
}
int main() { ordinary_and_modes();timeouts_and_mutations();register_and_dispatch();boundaries();std::printf("spu initialize: %u checks passed\n",checks); }
