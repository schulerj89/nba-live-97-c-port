#include "recovered/game_heap_release.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool pass) {
    ++checks;
    if (!pass) { std::fprintf(stderr,"heap release check %u failed\n",checks); std::exit(1); }
}
constexpr uint32_t Base=0x80110000u, Low=Base, Descriptor=Base+40,
    High=Base+80, Free=Base+120, Payload=Base+256, Lock=Base+384,
    Heap=0x80103d50u, FreeHead=0x800eb688u, LockPointer=0x801029c0u;
struct Fixture {
    std::array<std::vector<uint8_t>,4> data,known;
    std::array<Nba97GameTextRegion,4> regions{};
    Nba97GameHeapReleaseContext context{};
    Nba97GameHeapReleaseProgress progress{};
    std::array<Nba97GameHeapReleaseStore,8> journal{};
    Fixture() {
        const uint32_t bases[]={Base,Heap,FreeHead,LockPointer};
        const size_t sizes[]={512,384,4,4};
        for (unsigned i=0;i<4;++i) {
            data[i].assign(sizes[i],0xcd);known[i].assign(sizes[i],0);
            regions[i]={bases[i],data[i].data(),known[i].data(),sizes[i]};
        }
        context={{regions.data(),regions.size()},1000};
        for (unsigned i=0;i<16;++i) put(Heap+24*i+4,0);
        put(Heap,Low);put(Heap+4,High);
        put(Low+32,Descriptor);put(Descriptor,Payload);put(Descriptor+24,5);
        put(Descriptor+32,High);put(Descriptor+36,Low);
        put(High,Base+512);put(High+24,0x8005);put(High+36,Descriptor);
        put(FreeHead,Free);put(Free+32,0);put(LockPointer,Lock);put(Lock,0);
    }
    uint8_t* byte(uint32_t address,bool mask=false) {
        for (auto& r:regions) if (address>=r.base&&uint64_t(address-r.base)<r.size)
            return (mask?r.known:r.data)+(address-r.base);
        check(false);return nullptr;
    }
    void put(uint32_t address,uint32_t value) {
        for (unsigned i=0;i<4;++i) { *byte(address+i)=uint8_t(value>>(8*i));*byte(address+i,true)=1; }
    }
    uint32_t get(uint32_t address) {
        uint32_t value=0;for (unsigned i=0;i<4;++i)value|=uint32_t(*byte(address+i))<<(8*i);return value;
    }
    int run(Nba97GameHeapReleaseOperation operation=NBA97_HEAP_RELEASE_PAYLOAD_90698,
            uint32_t address=Payload,size_t capacity=8,Nba97GameHeapReleaseValue incoming={0x87654321,0}) {
        return nba97_game_heap_release(&context,operation,address,incoming,journal.data(),capacity,&progress);
    }
};
void ordinary() {
    Fixture f;const auto before=f.data;const auto known=f.known;
    check(f.run()==1&&f.progress.completed&&f.progress.descriptor==Descriptor);
    check(f.progress.returned.known==1&&f.progress.returned.word==Free);
    check(f.get(Low+32)==High&&f.get(High+36)==Low);
    check(f.get(Descriptor)==0&&f.get(FreeHead)==Descriptor&&f.get(Descriptor+32)==Free);
    check(f.get(Descriptor+36)==Low&&f.get(Descriptor+24)==5&&f.get(Lock)==0);
    const uint32_t pcs[]={0x800a4064u,0x80090728u,0x80090738u,0x80090740u,0x80090d34u,0x80090d3cu,0x800a408cu};
    check(f.progress.stores==7);
    for(unsigned i=0;i<7;++i)check(f.journal[i].pc==pcs[i]);
    // Every byte outside the seven actual store spans retains its incoming
    // value and knownness, including the released payload and descriptor name.
    bool unchanged=true;
    for(unsigned r=0;r<4;++r)for(size_t i=0;i<f.data[r].size();++i) {
        const auto at=f.regions[r].base+uint32_t(i);bool touched=false;
        for(size_t j=0;j<f.progress.stores;++j)
            touched|=at>=f.journal[j].address&&uint64_t(at-f.journal[j].address)<4;
        if(!touched)unchanged&=f.data[r][i]==before[r][i]&&f.known[r][i]==known[r][i];
    }
    check(unchanged);
    // A repeated release searches the live list and does not enqueue twice.
    check(f.run()==1&&f.progress.returned.known&&f.progress.returned.word==0&&f.progress.stores==0);
    check(f.get(FreeHead)==Descriptor);
}
void search() {
    for(unsigned bank=0;bank<16;++bank) {
        Fixture f;f.put(Heap+4,0);f.put(Heap+24*bank,Low);f.put(Heap+24*bank+4,High);
        check(f.run(NBA97_HEAP_FIND_90618)==1&&f.progress.returned.word==Descriptor&&f.progress.stores==0);
    }
    {Fixture f;check(f.run(NBA97_HEAP_FIND_90618,Payload+4)==1&&f.progress.returned.word==0);}
    {Fixture f;f.put(High+24,5);
        // Original quirk: reaching a malformed high sentinel can return it
        // despite the payload mismatch. No sentinel repair is introduced.
        check(f.run(NBA97_HEAP_FIND_90618,Payload+4)==1&&f.progress.returned.word==High);}
    {Fixture f;f.put(Descriptor+24,0x8005);
        check(f.run(NBA97_HEAP_FIND_90618)==1&&f.progress.returned.word==0);}
    {Fixture f;f.put(Heap,0);check(f.run(NBA97_HEAP_FIND_90618)==NBA97_TEXT_RESOURCE);
        check(f.progress.stopped_pc==0x80090648u&&f.progress.stopped_address==32);}
    {Fixture f;f.put(Descriptor+32,Descriptor);f.context.access_budget=20;
        check(f.run(NBA97_HEAP_FIND_90618,Payload+4)==NBA97_TEXT_LIMIT&&f.progress.accesses==20);}
}
void null_and_direct() {
    for(auto op:{NBA97_HEAP_RELEASE_PAYLOAD_90698,NBA97_HEAP_RELEASE_DESCRIPTOR_906C4})
        for(uint8_t known:{uint8_t(0),uint8_t(1)}) {
            Fixture f;f.context.memory={nullptr,0};
            check(f.run(op,0,0,{0x12345678,known})==1&&f.progress.completed);
            check(f.progress.returned.word==0x12345678&&f.progress.returned.known==known&&f.progress.accesses==0);
        }
    {Fixture f;check(f.run(NBA97_HEAP_UNLINK_90714,0)==NBA97_TEXT_RESOURCE);
        check(f.progress.stopped_address==36&&f.progress.stopped_pc==0x8009071cu);}
    {Fixture f;f.put(LockPointer,0);check(f.run(NBA97_HEAP_RELEASE_DESCRIPTOR_906C4,Descriptor)==NBA97_TEXT_RESOURCE);
        check(f.progress.stopped_pc==0x800a4064u&&f.progress.stopped_address==0&&f.progress.stores==0);}
    {Fixture f;f.put(LockPointer,0);check(f.run(NBA97_HEAP_UNLINK_90714,Descriptor)==1&&f.progress.stores==5);}
}
void aliases() {
    {Fixture f;f.put(Descriptor+36,Descriptor+4);
        check(f.run(NBA97_HEAP_UNLINK_90714,Descriptor)==1);
        check(f.get(Descriptor+36)==High&&f.get(High+36)==High);}
    {Fixture f;f.put(Descriptor+36,LockPointer-32);
        check(f.run(NBA97_HEAP_RELEASE_DESCRIPTOR_906C4,Descriptor)==1);
        check(f.get(LockPointer)==High&&f.get(Lock)==1&&f.get(High)==0);
        check(f.journal[6].address==High);}
    {Fixture f;f.put(Descriptor+32,FreeHead-36);f.put(Descriptor+36,Low);
        check(f.run(NBA97_HEAP_UNLINK_90714,Descriptor)==1);
        check(f.progress.returned.word==Low&&f.get(Descriptor+32)==Low);}
}
void refused_prefixes() {
    Fixture complete;check(complete.run()==1);
    for(size_t capacity=0;capacity<7;++capacity) {
        Fixture f;check(f.run(NBA97_HEAP_RELEASE_PAYLOAD_90698,Payload,capacity)==NBA97_TEXT_LIMIT);
        check(!f.progress.completed&&f.progress.stores==capacity);
        check(f.progress.stopped_pc==complete.journal[capacity].pc);
        for(size_t i=0;i<capacity;++i)check(f.journal[i].address==complete.journal[i].address&&f.journal[i].value==complete.journal[i].value);
        check(f.get(Lock)==(capacity?1u:0u));
    }
    for(size_t budget=0;budget<complete.progress.accesses;++budget) {
        Fixture f;f.context.access_budget=budget;
        check(f.run()==NBA97_TEXT_LIMIT&&f.progress.accesses==budget&&!f.progress.completed);
        for(size_t i=0;i<f.progress.stores;++i)check(f.journal[i].pc==complete.journal[i].pc&&f.journal[i].value==complete.journal[i].value);
    }
    {Fixture f;*f.byte(Descriptor+36,true)=0;
        check(f.run()==NBA97_TEXT_UNKNOWN&&f.progress.stores==1&&f.get(Lock)==1);}
    {Fixture f;*f.byte(Descriptor+36,true)=0;*f.byte(Descriptor+39,true)=2;
        check(f.run()==NBA97_TEXT_ARGUMENT&&f.progress.stores==1);}
    {Fixture f;*f.byte(Low+32,true)=0;*f.byte(Low+35,true)=2;
        check(f.run(NBA97_HEAP_UNLINK_90714,Descriptor)==NBA97_TEXT_ARGUMENT&&f.progress.stores==0);}
    {Fixture f;f.put(Descriptor+36,Low+1);
        check(f.run()==NBA97_TEXT_ALIGNMENT_TRAP&&f.progress.stores==1&&f.progress.stopped_address==Low+33);}
    {Fixture f;f.regions[0].size=Descriptor-Base+38;
        check(f.run(NBA97_HEAP_UNLINK_90714,Descriptor)==NBA97_TEXT_RESOURCE&&f.progress.stores==0);}
    {Fixture f;f.regions[1].base=Base;
        check(f.run()==NBA97_TEXT_ARGUMENT);}
}
}
int main() {
    ordinary();search();null_and_direct();aliases();refused_prefixes();
    std::printf("game_heap_release: %u checks passed\n",checks);
}
