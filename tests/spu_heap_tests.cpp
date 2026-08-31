#include "recovered/spu_heap.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;if(!value) { std::fprintf(stderr,"spu heap check %u failed\n",checks);std::exit(1); }
}
constexpr uint32_t Globals=0x800c7500u,Descriptors=0x800fee50u;
constexpr uint32_t Last=0x800c7a88u,Table=0x800c7a8cu,Capacity=0x800c7a84u;
struct Fixture {
    std::array<std::vector<uint8_t>,2> bytes,known;
    std::array<Nba97VoicePatlSpan,2> spans{};
    Nba97SpuHeap heap{};
    Nba97SpuHeapProgress progress{};
    std::array<Nba97SpuHeapStore,300> journal{};
    Fixture() {
        const uint32_t bases[]={Globals,Descriptors};const size_t sizes[]={0x600,129*8};
        for(unsigned i=0;i<2;++i) {
            bytes[i].assign(sizes[i],0xcd);known[i].assign(sizes[i],0);
            spans[i]={bytes[i].data(),known[i].data(),sizes[i],bases[i],1,1,0};
        }
        heap={{spans.data(),spans.size()},10000};
        // Explicit retained-state fixture, not a claim that hardware startup
        // ran. The actual caller uses128/FEE50 after the required SPU setup.
        put(0x800c75ecu,3);put(0x800c75f4u,7);put(0x800c762cu,0);put(0x800c7630u,0xfffe);
    }
    void put(uint32_t at,uint32_t value) { check(nba97_voice_patl_write(&heap.memory,at,4,value)==1); }
    uint32_t get(uint32_t at) {
        uint32_t value=0;check(nba97_voice_patl_read(&heap.memory,at,4,&value)==1);return value;
    }
    int run(Nba97SpuHeapOperation op,uint32_t a0=0,uint32_t a1=0,size_t capacity=300) {
        return nba97_spu_heap(&heap,op,a0,a1,journal.data(),capacity,&progress);
    }
    void init(uint32_t count=128) { check(run(NBA97_SPU_HEAP_INITIALIZE_7E940,count,Descriptors)==1); }
    uint32_t allocate(uint32_t size) {
        check(run(NBA97_SPU_HEAP_ALLOCATE_7EC2C,size)==1&&progress.completed);return progress.return_v0;
    }
    void free(uint32_t address) { check(run(NBA97_SPU_HEAP_FREE_7E56C,address)==1&&progress.completed); }
};
void initialize() {
    Fixture f;f.init();
    check(f.progress.return_v0==128&&f.get(Capacity)==128&&f.get(Last)==0&&f.get(Table)==Descriptors);
    check(f.get(Descriptors)==0x40001010&&f.get(Descriptors+4)==0x7eff0);
    bool untouched=true;for(size_t i=8;i<f.bytes[1].size();++i)untouched&=f.bytes[1][i]==0xcd&&f.known[1][i]==0;
    check(untouched&&f.progress.stores==5);
    for(uint32_t count:{0u,0xffffffffu,0x80000000u}) {
        Fixture zero;zero.heap.memory={nullptr,0};
        check(zero.run(NBA97_SPU_HEAP_INITIALIZE_7E940,count,0,0)==1);
        check(zero.progress.return_v0==0&&zero.progress.accesses==0&&zero.progress.stores==0);
    }
}
void lifecycle() {
    Fixture f;f.init(8);
    auto a=f.allocate(16),b=f.allocate(32),c=f.allocate(48);
    check(a==0x1010&&b==0x1020&&c==0x1040&&f.get(Last)==3);
    f.free(b);check(f.get(Descriptors+8)==(b|0x80000000u)&&f.get(Last)==3);
    auto reused=f.allocate(24);check(reused==b);
    // Split the freed block, preserving its remainder and the tail descriptor.
    check(f.get(Descriptors+12)==24&&f.get(Descriptors+16)==0x80001038u&&f.get(Descriptors+20)==8);
    f.free(reused);f.free(c);f.free(a);
    check(f.get(Last)==0&&f.get(Descriptors)==0x40001010&&f.get(Descriptors+4)==0x7eff0);
    // Compaction does not clear the old descriptor storage beyond the live tail.
    bool retained=false;for(size_t i=8;i<40;++i)retained|=f.known[1][i]!=0;
    check(retained&&f.allocate(64)==0x1010);
}
void source_quirks() {
    for(uint32_t size=0;size<8;++size) {
        Fixture f;f.init();auto address=f.allocate(size);
        // Original tests size&~mask. These requests round DOWN to zero; after
        // maintenance the returned descriptor word contains the tail flag.
        check(address==0x40001010&&f.get(Last)==0&&f.get(Descriptors+4)==0x7eff0);
    }
    {Fixture f;f.init();f.put(0x800c762cu,1);f.put(0x800c7630u,0);
        // Reserved space exceeds the tail's size. Unsigned subtraction wraps
        // and the original still allocates; this is deliberately not clamped.
        check(f.allocate(16)==0x1010);}
    {Fixture f;f.init(1);f.spans[1].size=8;
        check(f.run(NBA97_SPU_HEAP_ALLOCATE_7EC2C,16)==NBA97_PATL_RESOURCE);
        check(f.progress.stopped_pc==0x8007eddcu&&f.progress.stopped_address==Descriptors+8&&f.progress.stores==0);}
    {Fixture f;f.init(1);check(f.allocate(16)==0x1010&&f.get(Last)==1);
        check(f.allocate(16)==0xffffffffu);}
    {Fixture f;f.put(Last,0xffffffffu);
        check(f.run(NBA97_SPU_HEAP_MAINTAIN_7EF44)==1&&f.progress.return_v0==0xfffffff0u);
        f.put(Capacity,0);f.free(0);check(f.progress.return_v0==0xfffffff0u&&f.progress.stores==0);}
    {Fixture f;f.init();auto address=f.allocate(16);f.free(0xdeadbeef);
        check(f.get(Descriptors)==address&&f.get(Last)==1);}
    {Fixture f;f.init(3);
        f.put(Descriptors,0x1010);f.put(Descriptors+4,16);
        f.put(Descriptors+8,0x80001020);f.put(Descriptors+12,32);
        f.put(Descriptors+16,0x1040);f.put(Descriptors+20,16);
        f.put(Descriptors+24,0x40001050);f.put(Descriptors+28,0x7efb0);f.put(Last,3);
        // No spare descriptor: the original shrinks the free block without
        // recording its unused remainder. The gap1030..103f stays unrecorded.
        check(f.allocate(16)==0x1020&&f.get(Descriptors+12)==16);
        check(f.get(Descriptors+16)==0x1040&&f.get(Last)==3);}
    {Fixture f;f.init(1);f.put(Descriptors,0x80001010);
        f.put(Descriptors+8,0x2fffffff);f.put(Descriptors+16,0x2fffffff);f.spans[1].size=24;
        check(f.run(NBA97_SPU_HEAP_MAINTAIN_7EF44)==NBA97_PATL_RESOURCE);
        check(f.progress.stopped_pc==0x8007ef94u&&f.progress.stopped_address==Descriptors+24&&f.progress.stores==0);}
}
void aliases() {
    {Fixture f;
        // Init caches the shift before its first descriptor store aliases it.
        check(f.run(NBA97_SPU_HEAP_INITIALIZE_7E940,2,0x800c75ecu)==1);
        check(f.get(0x800c75ecu)==0x40001010&&f.get(0x800c75f0u)==0x7eff0);}
    {Fixture f;
        // Live table/global aliases preserve the literal order of all stores.
        check(f.run(NBA97_SPU_HEAP_INITIALIZE_7E940,2,Last)==1);
        check(f.get(Last)==0&&f.get(Table)==0x7eff0&&f.get(Capacity)==2);}
}
void prefixes() {
    Fixture complete;complete.init();check(complete.allocate(16)==0x1010);
    const size_t stores=complete.progress.stores,accesses=complete.progress.accesses;
    check(stores==5);
    for(size_t capacity=0;capacity<stores;++capacity) {
        Fixture f;f.init();check(f.run(NBA97_SPU_HEAP_ALLOCATE_7EC2C,16,0,capacity)==NBA97_SPU_HEAP_LIMIT);
        check(f.progress.stores==capacity&&!f.progress.completed&&f.progress.stopped_pc==complete.journal[capacity].pc);
        for(size_t i=0;i<capacity;++i)check(f.journal[i].address==complete.journal[i].address&&f.journal[i].value==complete.journal[i].value);
    }
    for(size_t budget=0;budget<accesses;++budget) {
        Fixture f;f.init();f.heap.access_budget=budget;
        check(f.run(NBA97_SPU_HEAP_ALLOCATE_7EC2C,16)==NBA97_SPU_HEAP_LIMIT&&f.progress.accesses==budget);
        for(size_t i=0;i<f.progress.stores;++i)check(f.journal[i].pc==complete.journal[i].pc&&f.journal[i].value==complete.journal[i].value);
    }
    {Fixture f;f.init();f.known[1][0]=0;f.known[1][3]=2;
        check(f.run(NBA97_SPU_HEAP_ALLOCATE_7EC2C,16)==NBA97_PATL_METADATA&&f.progress.stores==0);}
    {Fixture f;f.init();f.known[1][0]=0;
        check(f.run(NBA97_SPU_HEAP_ALLOCATE_7EC2C,16)==NBA97_PATL_RESOURCE&&f.progress.stores==0);}
    {Fixture f;f.init();f.spans[1].writable=0;
        check(f.run(NBA97_SPU_HEAP_ALLOCATE_7EC2C,16)==NBA97_PATL_RESOURCE&&f.progress.stores==0);}
    {Fixture f;check(f.run(NBA97_SPU_HEAP_INITIALIZE_7E940,8,Descriptors+1)==NBA97_PATL_RESOURCE);
        check(f.progress.stopped_pc==0x8007e960u&&f.progress.stores==0);}
}
}
int main() {
    initialize();lifecycle();source_quirks();aliases();prefixes();
    std::printf("spu_heap: %u checks passed\n",checks);
}
