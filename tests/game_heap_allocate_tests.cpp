#include "recovered/game_heap_allocate.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks=0;
void check(bool v){++checks;if(!v){std::fprintf(stderr,"heap allocation check %u failed\n",checks);std::exit(1);}}
constexpr uint32_t Heap=0x80103d50u,Free=0x800eb688u,Serial=0x800c4a8cu,Name=0x80130000u,
    Records=0x80131000u,Head=Records,Tail=Records+0x40u,New=Records+0x80u,Arena=0x80140000u;
struct Fixture {
    std::array<std::vector<uint8_t>,6> bytes,known;
    std::array<Nba97GameTextRegion,6> regions{};
    Nba97GameHeapContext context{};Nba97GameHeapArguments args{Name,77,32,0};
    Nba97GameHeapProgress out{};std::vector<Nba97GameHeapEvent> events=std::vector<Nba97GameHeapEvent>(1000);
    unsigned mode=0,names=0,reclaims=0;
    Fixture(){const uint32_t bases[]={Heap,Free,Serial,Name,Records,Arena};const size_t sizes[]={384,16,16,64,1024,8192};
        for(unsigned i=0;i<6;++i){bytes[i].resize(sizes[i],0xcd);known[i].resize(sizes[i]);regions[i]={bases[i],bytes[i].data(),known[i].data(),sizes[i]};}
        context={{regions.data(),regions.size()},10000,io,this};
        put(Heap,Head);put(Heap+4,Tail);put(Heap+8,3);put(Heap+12,15);put(Heap+20,0);put(Free,New);put(Serial,99);
        put(Head,Arena);put(Head+16,0);put(Head+32,Tail);put(Head+36,0);
        put(Tail,Arena+4096);put(Tail+16,0);put(Tail+32,0);put(Tail+36,Head);put(New+32,0);
        const char name[]="disc:/fonts\\sample";for(size_t i=0;i<sizeof name;++i)byte(Name+static_cast<uint32_t>(i),static_cast<uint8_t>(name[i]));
    }
    std::pair<uint8_t*,uint8_t*> at(uint32_t address){for(auto& r:regions)if(address>=r.base&&address-r.base<r.size)return {r.data+address-r.base,r.known+address-r.base};check(false);return {};}
    void byte(uint32_t address,uint8_t v){auto p=at(address);*p.first=v;*p.second=1;}
    void put(uint32_t address,uint32_t v){for(unsigned i=0;i<4;++i)byte(address+i,static_cast<uint8_t>(v>>(8*i)));}
    uint32_t get(uint32_t address){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(*at(address+i).first)<<(8*i);return v;}
    static int io(void* user,const Nba97GameTextMemory*,const Nba97GameHeapEvent* event,Nba97GameHeapValue* result){
        auto& f=*static_cast<Fixture*>(user);
        if(event->kind==NBA97_HEAP_BIOS_A0_1A){++f.names;check(event->pc==0x8009d940u&&event->argument[2]==12);
            if(f.mode==1)return 0;
            // Declared BIOS fixture, not proof of the unavailable BIOS ROM.
            bool ended=false;for(unsigned i=0;i<12;++i){uint8_t v=ended?0:*f.at(event->argument[1]+i).first;if(!v)ended=true;f.byte(event->argument[0]+i,v);}
            if(f.mode==4)f.put(Heap+20,4);
            if(f.mode==5){f.put(Heap+20,4);f.put(New+20,1);f.put(New+24,0x1234);}
            if(f.mode==8)*f.at(New+3).second=2;
            if(f.mode==9)*result={123,2};
            if(f.mode==10){f.put(Heap+20,4);*f.at(New+24).second=0;}
            return 1; // Name-copy return is unused and may remain unknown.
        }
        ++f.reclaims;check(event->kind==NBA97_HEAP_RECLAIM_A3074&&event->argument[0]==f.args.flags);
        if(f.mode==2)return 0;
        if(f.mode==3){*result={0,0};return 1;}
        if(f.mode==6&&f.reclaims==1){f.put(Tail,Arena+8192);*result={1,1};return 1;}
        *result={0,1};return 1;
    }
    int run(size_t capacity=1000){return nba97_game_heap_allocate(&context,&args,events.data(),capacity,&out);}
};
void ordinary(){
    for(uint32_t flags:{0u,32u,64u,96u}){Fixture f;f.args.flags=flags;check(f.run()==1&&f.out.completed&&f.out.descriptor.word==New&&f.out.descriptor.known);
        check(f.out.aligned_size==80&&f.get(New)==((flags&32)?Arena+4096-80:Arena));
        check(f.get(New+16)==80&&f.get(New+20)==77&&f.get(New+24)==flags&&f.get(New+28)==99);
        check(f.get(Free)==0&&f.get(Serial)==100&&f.get(Head+32)==New&&f.get(Tail+36)==New);
        check(f.get(New+32)==Tail&&f.get(New+36)==Head&&f.names==1&&f.reclaims==0&&f.out.stores==11);
        check(f.get(New+4)==0x706d6173u&&*f.at(New+4+6).first==0);
        for(uint32_t i=0;i<8192;++i)check(*f.at(Arena+i).second==0); // Allocation never initializespayload.
    }
    {Fixture f;f.put(Heap+20,4);check(f.run()==1&&f.out.aligned_size==84&&f.out.stores==16);
        const auto base=f.get(New);check(f.get(New+24)==0x4020&&f.get(base+77)==0x444e4542u);
        for(unsigned i=0;i<4;++i){const auto& e=f.events[f.out.events-4+i];check(e.address==base+80-i&&e.width==1&&e.pc==0x800aa078u);}}
    {Fixture f;f.mode=4;check(f.run()==1&&f.out.aligned_size==80&&f.get(New+24)==0x4020);}
    {Fixture f;f.mode=5;check(f.run()==1&&f.get(New+24)==0x5234&&f.get(f.get(New)+1)==0x444e4542u);}
    {Fixture a,b;b.args.unused_argument3=0xffffffffu;check(a.run()==1&&b.run()==1&&a.bytes==b.bytes&&a.known==b.known);}
}
void refusal(){
    {Fixture f;f.context.io=nullptr;check(f.run()==NBA97_TEXT_IO_REFUSED&&f.out.stores==6&&f.get(Free)==0&&f.get(Serial)==100);check(!f.out.descriptor.known);}
    for(unsigned mode:{0u,2u,3u,6u})for(uint32_t direction:{0u,32u}){Fixture f;f.mode=mode;f.args.flags=direction;f.args.size=5000;
        const int rc=f.run();check(rc==(mode==2?-5:mode==3?-2:1));
        if(mode==6)check(f.out.descriptor.word==New&&f.reclaims==1&&f.names==1);
        else if(mode==0)check(f.out.completed&&f.out.descriptor.known&&f.out.descriptor.word==0&&f.out.stores==0);}
    {Fixture f;f.put(Free,0);check(f.run()==NBA97_TEXT_RESOURCE&&f.out.stopped_pc==0x80090d4cu&&f.out.stopped_address==32);}
    {Fixture f;f.put(Free,New+1);check(f.run()==NBA97_TEXT_ALIGNMENT_TRAP&&f.out.stores==0);}
    {Fixture f;*f.at(Heap+20).second=0;*f.at(Heap+23).second=2;check(f.run()==NBA97_TEXT_ARGUMENT&&f.out.stopped_pc==0x800902f0u);}
    {Fixture f;*f.at(New+24).second=0;*f.at(New+27).second=2;check(f.run()==NBA97_TEXT_ARGUMENT&&f.out.stores==1&&f.get(Free)==0);}
    {Fixture f;*f.at(Name).second=0;check(f.run()==NBA97_TEXT_UNKNOWN&&f.out.events==0);}
    {Fixture f;f.mode=8;check(f.run()==NBA97_TEXT_ARGUMENT&&f.out.stores==6&&f.out.stopped_pc==0x80090550u);check(*f.at(New).second==0);}
    {Fixture f;f.mode=9;check(f.run()==NBA97_TEXT_ARGUMENT&&f.out.stores==6&&!f.events[6].completed);}
    {Fixture f;f.mode=10;check(f.run()==NBA97_TEXT_UNKNOWN&&f.out.stores==11&&f.out.stopped_pc==0x800a54c4u);check(f.get(Head+32)==New&&f.get(Tail+36)==New);}
    for(size_t limit=0;limit<50;++limit){Fixture f;f.context.access_budget=limit;const int rc=f.run();check(rc==1||rc==NBA97_TEXT_LIMIT);check(f.out.accesses<=limit);}
    for(size_t cap=0;cap<12;++cap){Fixture f;check(f.run(cap)==NBA97_TEXT_LIMIT&&f.out.events==cap);}
}
void raw_quirks(){
    {Fixture f;f.args.size=0x80000000u;check(f.run()==1&&f.get(New)==Arena+4096-0x80000000u); // Signedgap test admits wrappedsize; no repair.
        check(f.out.aligned_size==0x80000000u);}
    {Fixture f;f.put(Serial,0xffffffffu);check(f.run()==1&&f.get(Serial)==0&&f.get(New+28)==0xffffffffu);}
    {Fixture f;f.put(Heap,0);check(f.run()==1&&f.out.descriptor.word==0&&f.out.events==0);}
    {Fixture f;f.put(Heap+8,5);f.args.size=1;check(f.run()==1&&f.out.aligned_size==2); // Uncheckednoncanonicalmask retained.
    }
}
}
int main(){ordinary();refusal();raw_quirks();std::printf("game_heap_allocate: %u checks passed\n",checks);}
