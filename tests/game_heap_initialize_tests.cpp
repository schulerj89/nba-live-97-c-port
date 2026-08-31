#include "recovered/game_heap_initialize.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool pass){++checks;if(!pass){std::fprintf(stderr,"heap initialize check %u failed\n",checks);std::exit(1);}}
constexpr uint32_t Arena=0x8010b61cu,End=0x801fd800u,Heap=0x80103d50u,Lock=0x800d7c3cu;
struct Fixture {
    std::array<std::vector<uint8_t>,7> bytes,known;
    std::array<Nba97GameTextRegion,7> regions{};
    Nba97GameHeapInitializeArguments args{220,Arena,End-Arena,0x800d79c8u};
    Nba97GameHeapInitializeContext context{};
    Nba97GameHeapInitializeProgress progress{};
    std::vector<Nba97GameHeapInitializeEvent> journal=std::vector<Nba97GameHeapInitializeEvent>(1000);
    unsigned calls=0,refuse=0,mutation=0;
    Fixture(){
        const uint32_t bases[]={0x800c4a80u,Lock,0x800eb688u,0x801029c0u,Heap,0x80109b8cu,Arena};
        const size_t sizes[]={16,8,4,4,384,4,End-Arena};
        for(unsigned i=0;i<regions.size();++i){bytes[i].resize(sizes[i],0xcd);known[i].resize(sizes[i],0);
            regions[i]={bases[i],bytes[i].data(),known[i].data(),sizes[i]};}
        context={{regions.data(),regions.size()},10000,format,this};
    }
    void put(uint32_t address,uint32_t value,unsigned size=4){
        for(auto& r:regions)if(address>=r.base&&uint64_t(address-r.base)+size<=r.size){
            auto offset=address-r.base;for(unsigned i=0;i<size;++i){r.data[offset+i]=uint8_t(value>>(8*i));r.known[offset+i]=1;}return;}
        check(false);
    }
    uint32_t get(uint32_t address){
        for(auto& r:regions)if(address>=r.base&&uint64_t(address-r.base)+4<=r.size){
            uint32_t v=0;auto offset=address-r.base;for(unsigned i=0;i<4;++i)v|=uint32_t(r.data[offset+i])<<(8*i);return v;}
        check(false);return 0;
    }
    static int format(void* user,const Nba97GameTextMemory*,const Nba97GameHeapInitializeEvent* e){
        auto& f=*static_cast<Fixture*>(user);++f.calls;
        check(e->kind==NBA97_HEAP_INITIALIZE_FORMAT_9CB7C);
        if(f.refuse==f.calls)return 0;
        // Declared mutable formatting fixture, not a recovered9CB7C.
        f.put(e->argument[0],f.calls==1?0x474542u:0x444e45u);
        if(f.mutation==1&&f.calls==2){f.put(0x800c4a80u,0x12345678);f.put(0x800c4a84u,0x12340000);f.put(0x801029c0u,Lock+4);}
        if(f.mutation==2&&f.calls==2)f.known[0][4]=0;
        if(f.mutation==3&&f.calls==2)f.put(0x801029c0u,0);
        return 1;
    }
    int run(size_t capacity=1000){return nba97_game_heap_initialize(&context,&args,journal.data(),capacity,&progress);}
};
void ordinary(){
    Fixture f;check(f.run()==1&&f.progress.completed&&f.calls==2);
    check(f.get(Heap)==Arena&&f.get(Heap+4)==Arena+40);
    check(f.get(Arena)==Arena+220*40&&f.get(Arena+40)==End);
    check(f.get(0x800eb688u)==Arena+80&&f.get(Arena+32)==Arena+40&&f.get(Arena+40+36)==Arena);
    check(f.get(Heap+8)==15&&f.get(Heap+12)==15&&f.get(Heap+16)==0&&f.get(Heap+20)==0);
    check(f.get(Lock)==0&&f.get(0x801029c0u)==Lock&&f.progress.return_v0==End-Arena);
    check(f.get(0x80109b8cu)==End-Arena);
    check(f.known[6][28]==0&&f.known[6][68]==0); // Sentinel serials were never set.
    for(size_t i=220*40;i<f.bytes[6].size();++i)
        if(f.bytes[6][i]!=0xcd||f.known[6][i]){check(false);break;}
    check(f.bytes[6][220*40]==0xcd&&f.known[6][220*40]==0);
    check(f.progress.stores+2==f.progress.events&&f.progress.callbacks_completed==2);
}
void prefixes(){
    {Fixture f;f.refuse=1;check(f.run()==NBA97_TEXT_IO_REFUSED);check(f.get(0x800eb688u)==Arena+40&&f.get(Lock)==1);
        check(f.known[4][0]==0&&!f.progress.completed&&f.progress.callbacks_completed==0);}
    {Fixture f;f.refuse=2;check(f.run()==NBA97_TEXT_IO_REFUSED);check(f.get(Heap)==Arena&&f.get(0x800eb688u)==Arena+80);
        check(f.known[4][4]==0&&f.get(Lock)==1&&f.progress.callbacks_completed==1);}
    for(uint32_t count:{0u,1u,0xffffffffu}){Fixture f;f.args.descriptor_count=count;check(f.run()==NBA97_TEXT_RESOURCE);
        check(f.progress.stopped_pc==0x80090d4cu&&f.progress.stopped_address==0x20&&f.get(Lock)==1);}
    {Fixture f;f.args.arena++;check(f.run()==NBA97_TEXT_ALIGNMENT_TRAP);check(f.progress.stopped_pc==0x80090d00u&&f.get(Lock)==1);}
    {Fixture f;f.known[1][0]=0;f.known[1][3]=2;check(f.run()==NBA97_TEXT_ARGUMENT);check(!f.progress.stores&&f.bytes[1][0]==0xcd);}
    {Fixture f;f.context.access_budget=3;check(f.run()==NBA97_TEXT_LIMIT);check(f.progress.accesses==3&&f.progress.stores==3&&f.get(Lock)==1);}
    {Fixture f;check(f.run(2)==NBA97_TEXT_LIMIT);check(f.progress.events==2&&f.get(Lock)==0&&f.get(0x801029c0u)==Lock);}
}
void live_values(){
    {Fixture f;f.mutation=1;check(f.run()==1);check(f.progress.return_v0==0x5678&&f.get(0x80109b8cu)==0x5678);
        check(f.get(Lock)==1&&f.get(Lock+4)==0);}
    {Fixture f;f.mutation=2;check(f.run()==NBA97_TEXT_UNKNOWN);check(f.progress.stopped_pc==0x8008fb10u&&f.get(Lock)==1);}
    {Fixture f;f.mutation=3;check(f.run()==NBA97_TEXT_RESOURCE);check(f.progress.stopped_pc==0x800a408cu&&f.progress.stopped_address==0);
        check(f.get(0x80109b8cu)==End-Arena);}
    {Fixture f;f.args.gp+=4;check(f.run()==1);check(f.get(Lock+4)==0&&f.get(Lock)==0);}
}
void bank(){
    Fixture f;f.put(0x800eb688u,Arena);f.put(Arena+32,Arena+40);f.put(Arena+72,Arena+80);
    Nba97GameHeapBankArguments args{0x12345678,0xf20,Arena+1000,End,0,32,0xabcdef,1};
    check(nba97_game_heap_initialize_bank(&f.context,&args,f.journal.data(),f.journal.size(),&f.progress)==1);
    const auto heap=Heap+15*24;check(f.progress.return_v0==heap&&f.get(heap+8)==0xffffffffu&&f.get(heap+12)==31);
    check(f.get(heap+16)==0xabcdef&&f.get(heap+20)==4&&f.get(Arena+24)==0x8f20);
    check(f.get(heap)==Arena&&f.get(heap+4)==Arena+40&&f.known[1][0]==0);
}
}
int main(){ordinary();prefixes();live_values();bank();std::printf("game_heap_initialize: %u checks passed\n",checks);}
