#include "recovered/spu_heap_mapping.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value,int line) {
    ++checks;if(!value) { std::fprintf(stderr,"spu heap mapping check %u failed at line %d\n",checks,line);std::exit(1); }
}
#define check(value) check_at((value),__LINE__)
constexpr uint32_t Map=0x80130000u,Body=0x80140000u,Descriptors=0x800fee50u;
struct Fixture {
    std::array<std::vector<uint8_t>,6> bytes,known;
    std::array<Nba97VoicePatlSpan,6> spans{};
    std::array<uint8_t,24> table_bytes{},table_known{};
    std::array<Nba97SpuHeapStore,256> journal{};
    std::vector<uint8_t> transfer_fixture=std::vector<uint8_t>(0x80000,0xcd);
    Nba97VoiceMappingTable table{table_bytes.data(),table_known.data(),table_bytes.size()};
    Nba97SpuHeapMapping bridge{};
    Nba97VoiceMapping mapping{};
    Nba97VoiceMappingProgress progress{};
    unsigned transfers=0,events=0;
    bool transfer_complete=false;
    Fixture(bool with_platform=true) {
        const uint32_t bases[]={0x800c7500u,Descriptors,0x800c6d2cu,0x800f9600u,Map,Body};
        const size_t sizes[]={0x600,129*8,2,4,64,64};
        for(unsigned i=0;i<spans.size();++i) {
            bytes[i].assign(sizes[i],0xcd);known[i].assign(sizes[i],0);
            spans[i]={bytes[i].data(),known[i].data(),sizes[i],bases[i],1,1,0};
        }
        bridge.platform=with_platform?platform:nullptr;bridge.platform_context=this;
        bridge.access_budget=10000;bridge.journal=journal.data();bridge.journal_capacity=journal.size();
        mapping={{spans.data(),spans.size()},nba97_spu_heap_mapping_invoke,&bridge,10000};
        put(0x800c75ecu,3);put(0x800c75f4u,7);put(0x800c762cu,0);
        put(0x800c75e8u,1);put(0x800c75f0u,8);put(0x800c75f8u,0);put(0x800c7678u,0x55);
        put(0x800c75fcu,0);put(0x800c6d2cu,0,1);put(0x800f9600u,0);
        put(Map+6,1,1);put(Map+0x1c,0);put(Map+0x20,32);put(Map+0x24,16);put(Map+0x28,16);
        put(Map+0x2c,0xffffffffu);put(Map+0x30,0xffffffffu);
        for(unsigned i=0;i<64;++i)put(Body+i,(i*3+7)&255,1);
        check(nba97_voice_mapping_table_write(&table,0,0xffffffffu)==1);
        Nba97SpuHeap heap{mapping.memory,1000};Nba97SpuHeapProgress init{};
        check(nba97_spu_heap(&heap,NBA97_SPU_HEAP_INITIALIZE_7E940,128,Descriptors,journal.data(),journal.size(),&init)==1);
    }
    void put(uint32_t at,uint32_t value,unsigned width=4) { check(nba97_voice_patl_write(&mapping.memory,at,width,value)==1); }
    uint32_t get(uint32_t at,unsigned width=4) {
        uint32_t value=0;check(nba97_voice_patl_read(&mapping.memory,at,width,&value)==1);return value;
    }
    static int platform(void* context,const Nba97VoicePatlMemory* memory,
        const Nba97VoiceMappingEvent* event,uint32_t* result) {
        auto& f=*static_cast<Fixture*>(context);check(memory==&f.mapping.memory);
        // Declared test transfer/event fixture. This proves composition and
        // exact forwarding, not an implementation of real SPU hardware.
        if(event->call==NBA97_MAPPING_TRANSFER_7DC90) {
            auto destination=f.get(0x800c75c4u,2)*8u;
            check(uint64_t(destination)+event->a1<=f.transfer_fixture.size());
            for(uint32_t i=0;i<event->a1;++i)f.transfer_fixture[destination+i]=uint8_t(f.get(event->a0+i,1));
            ++f.transfers;f.transfer_complete=true;*result=0xffffffffu;return 1;
        }
        if(event->call==NBA97_MAPPING_TEST_EVENT_7F568) {
            check(event->a0==0x55&&event->a1==0);++f.events;*result=f.transfer_complete?1u:0u;return 1;
        }
        check(false);return 0; // Heap operations must never reach platform.
    }
    Nba97VoiceApiResult upload() { return nba97_voice_mapping_upload(&mapping,Map,Body,&table,&progress); }
};
void complete_lifecycle() {
    Fixture f;auto result=f.upload();
    if(result.completion!=1)std::fprintf(stderr,"mapping result %d, stopped %08x, heap %d, transfers %u, events %u\n",
        result.completion,f.progress.stopped_address,f.bridge.progress.completion,f.transfers,f.events);
    check(result.completion==1&&result.value==0&&f.bridge.progress.operations_completed==1);
    check(f.get(Map+0x2c)==0x1010&&f.transfers==1&&f.events==1&&f.get(0x800c6d2du,1)==0);
    check(f.bridge.progress.stores==5&&f.bridge.progress.completion==1);
    uint32_t value=0;check(nba97_voice_mapping_table_read(&f.table,4,&value)==1&&value==0x1010);
    check(f.table_known[8]==0&&f.table_known[12]==0); // No appended terminator.
    for(unsigned i=0;i<16;++i)check(f.transfer_fixture[0x1010+i]==uint8_t(i*3+7));
    f.bridge.progress={};
    result=nba97_voice_mapping_unload(&f.mapping,Map,&f.progress);
    check(result.completion==1&&result.value==0&&f.bridge.progress.operations_completed==1);
    check(f.bridge.progress.last_operation==NBA97_SPU_HEAP_FREE_7E56C&&f.get(Descriptors)==0x40001010);
    check(f.get(Map+0x2c)==0x1010&&f.transfers==1); // Original retains mapping fields.
    for(unsigned i=0;i<16;++i)check(f.transfer_fixture[0x1010+i]==uint8_t(i*3+7));
}
void stopped_transfer() {
    Fixture f(false);auto result=f.upload();
    check(result.completion==NBA97_PATL_IO_REFUSED&&f.transfers==0);
    check(f.bridge.progress.operations_completed==1&&f.bridge.progress.completion==1);
    check(f.get(Descriptors)==0x1010&&f.get(0x800c7a88u)==1);
    check(f.get(Map+0x2c)==0xffffffffu&&f.get(0x800c6d2du,1)==1&&f.get(0x800f9600u)==1);
}
void stereo_and_refusal() {
    {Fixture f;f.put(Map+6,2,1);auto result=f.upload();
        check(result.completion==1&&result.value==0&&f.transfers==2&&f.bridge.progress.operations_completed==2);
        check(f.get(Map+0x2c)==0x1010&&f.get(Map+0x30)==0x1020&&f.bridge.progress.stores==10);}
    {Fixture f;f.put(Map+6,2,1);f.put(Map+0x28,24);auto result=f.upload();
        // Original right-transfer comparison uses the LEFT length. Both real
        // CPU allocations remain live after the resulting source failure.
        check(result.completion==1&&result.value==-1&&f.transfers==2);
        check(f.get(Map+0x2c)==0x1010&&f.get(Map+0x30)==0xffffffffu&&f.get(0x800c7a88u)==2);
        check(f.get(Descriptors+8)==0x1020&&f.bridge.progress.operations_completed==2);}
    {Fixture f;f.put(Map+6,2,1);f.bridge.journal_capacity=5;auto result=f.upload();
        check(result.completion==NBA97_PATL_IO_REFUSED&&f.bridge.progress.completion==NBA97_SPU_HEAP_LIMIT);
        check(f.transfers==1&&f.bridge.progress.operations_completed==1&&f.bridge.progress.stores==5);
        check(f.bridge.progress.last.stopped_pc==0x8007eddcu&&f.get(Map+0x2c)==0x1010&&f.get(0x800c7a88u)==1);}
    {Fixture f;f.bridge.access_budget=0;auto result=f.upload();
        check(result.completion==NBA97_PATL_IO_REFUSED&&f.bridge.progress.completion==NBA97_SPU_HEAP_LIMIT);
        check(f.bridge.progress.stores==0&&f.bridge.progress.accesses==0&&f.get(Descriptors)==0x40001010);}
}
}
int main() {
    complete_lifecycle();stopped_transfer();stereo_and_refusal();
    std::printf("spu_heap_mapping: %u checks passed\n",checks);
}
