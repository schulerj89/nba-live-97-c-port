#include "recovered/spu_transfer_mapping.h"
#include "recovered/spu_heap_mapping.h"
#include "spu_sample_backend.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value,int line) {
    ++checks;if(!value) { std::fprintf(stderr,"spu transfer mapping check %u failed at line %d\n",checks,line);std::exit(1); }
}
#define check(value) check_at(bool(value),__LINE__)
constexpr uint32_t Map=0x80130000u,Body=0x80140000u,Descriptors=0x800fee50u;
using Backend=nba97::SpuSampleBackend;
using Phase=nba97::SpuDmaPhase;
struct Fixture {
    std::array<std::vector<uint8_t>,6> bytes,known;
    std::array<Nba97VoicePatlSpan,6> spans{};
    std::array<uint8_t,24> tableBytes{},tableKnown{};
    std::array<Nba97SpuHeapStore,256> heapJournal{};
    std::array<Nba97SpuTransferEvent,500> transferJournal{},isrJournal{};
    Nba97VoiceMappingTable table{tableBytes.data(),tableKnown.data(),tableBytes.size()};
    Nba97SpuHeapMapping heapBridge{};
    Nba97SpuTransferMapping transferBridge{};
    Nba97VoiceMapping mapping{};
    Nba97VoiceMappingProgress progress{};
    Backend device;
    nba97::SpuSampleIoContext binding{&device,1,nullptr,nullptr};
    uint32_t handle=0;
    bool service=true;
    unsigned copied=0,interrupts=0;
    Fixture() {
        const uint32_t bases[]={0x800c7500u,Descriptors,0x800c6d2cu,0x800f9600u,Map,Body};
        const size_t sizes[]={0x600,129*8,2,4,64,128};
        for(size_t i=0;i<spans.size();++i) {
            bytes[i].assign(sizes[i],0xcd);known[i].assign(sizes[i],0);
            spans[i]={bytes[i].data(),known[i].data(),sizes[i],bases[i],1,1,0};
        }
        heapBridge.platform=nba97_spu_transfer_mapping_invoke;heapBridge.platform_context=&transferBridge;
        heapBridge.access_budget=10000;heapBridge.journal=heapJournal.data();heapBridge.journal_capacity=heapJournal.size();
        transferBridge.io=io;transferBridge.io_context=this;transferBridge.access_budget=10000;
        transferBridge.journal=transferJournal.data();transferBridge.journal_capacity=transferJournal.size();
        mapping={{spans.data(),spans.size()},nba97_spu_heap_mapping_invoke,&heapBridge,10000};
        // Declared retained entry state and owned padding for this composition.
        // No claim that original hardware/interrupt startup already executed.
        put(0x800c75ecu,3);put(0x800c75f4u,7);put(0x800c762cu,0);
        put(0x800c75e8u,1);put(0x800c75f0u,8);put(0x800c75f8u,0);
        put(0x800c75fcu,0);put(0x800c6d2cu,0,1);put(0x800f9600u,0);
        put(0x800c75c8u,0x1f801c00);put(0x800c75ccu,Backend::Madr);
        put(0x800c75d0u,Backend::Bcr);put(0x800c75d4u,Backend::Chcr);put(0x800c75dcu,Backend::BusDelay);
        put(Map+6,1,1);put(Map+0x1c,0);put(Map+0x20,32);put(Map+0x24,16);put(Map+0x28,16);
        put(Map+0x2c,0xffffffffu);put(Map+0x30,0xffffffffu);
        for(uint32_t i=0;i<128;++i)put(Body+i,(i*3+7)&255,1);
        check(nba97_voice_mapping_table_write(&table,0,0xffffffffu)==1);
        Nba97SpuHeap heap{mapping.memory,1000};Nba97SpuHeapProgress init{};
        check(nba97_spu_heap(&heap,NBA97_SPU_HEAP_INITIALIZE_7E940,128,Descriptors,heapJournal.data(),heapJournal.size(),&init)==1);
        check(device.importRegister(Backend::Control,2,{0,1}));
        check(device.importRegister(Backend::TransferControl,2,{4,1}));
        check(device.importRegister(Backend::Dpcr,4,{0x80000,1}));
        check(device.importRegister(Backend::BusDelay,4,{0,1}));
        check(device.openEvent(0xf0000009,0x20,0x2000,0,handle));
        check(handle!=0&&device.enableEvent(handle));put(0x800c7678u,handle);
    }
    void put(uint32_t at,uint32_t value,uint32_t width=4) { check(nba97_voice_patl_write(&mapping.memory,at,width,value)==1); }
    uint32_t get(uint32_t at,uint32_t width=4) { uint32_t value=0;check(nba97_voice_patl_read(&mapping.memory,at,width,&value)==1);return value; }
    static int io(void* opaque,const Nba97VoicePatlMemory* memory,const Nba97SpuTransferEvent* event,Nba97SpuTransferValue* result) {
        auto& f=*static_cast<Fixture*>(opaque);
        check(memory->spans==f.mapping.memory.spans&&memory->count==f.mapping.memory.count);
        if(event->kind==NBA97_SPU_TRANSFER_TEST_EVENT_B0_0B&&f.service&&f.device.request().phase==Phase::Requested) {
            // Explicit scheduling boundary in this test: service the owned
            // request, then execute the recovered ISR before the original poll.
            // No fabricated completion and no claim of hardware IRQ cadence.
            if(!f.device.servicePendingDma(*memory,f.binding.memoryGeneration))return 0;
            ++f.copied;const auto ticket=f.device.request().ticket;
            if(!f.device.beginIsr(ticket))return 0;
            Nba97SpuTransfer cpu{*memory,io,&f,10000};Nba97SpuTransferProgress p{};
            int rc=nba97_spu_transfer(&cpu,NBA97_SPU_ISR_7D668,0,0,0,f.isrJournal.data(),f.isrJournal.size(),&p);
            const bool finished=bool(f.device.finishIsr(ticket,rc==1));
            if(rc!=1||!finished)return 0;
            ++f.interrupts;
        }
        return Backend::io(&f.binding,memory,event,result);
    }
    Nba97VoiceApiResult upload() { return nba97_voice_mapping_upload(&mapping,Map,Body,&table,&progress); }
};
void actual_upload_and_unload() {
    Fixture f;auto result=f.upload();
    if(result.completion!=1)std::fprintf(stderr,"mapping %d stopped %08x transfer %d device %d\n",result.completion,
        f.progress.stopped_address,f.transferBridge.progress.completion,int(f.device.lastResult().status));
    check(result.completion==1&&result.value==0);
    check(f.copied==1&&f.interrupts==1&&f.device.request().phase==Phase::IsrComplete);
    check(f.get(Map+0x2c)==0x1010&&f.get(0x800c6d2du,1)==0);
    check(f.heapBridge.progress.operations_completed==1&&f.transferBridge.progress.operations_completed==2);
    check(std::count(f.device.known().begin(),f.device.known().end(),uint8_t(1))==64);
    for(size_t i=0;i<64;++i)check(f.device.samples()[0x1010+i]==f.bytes[5][i]&&f.device.known()[0x1010+i]==1);
    uint32_t event=2;check(f.device.testEvent(f.handle,event)&&event==0);
    check(f.tableKnown[8]==0&&f.tableKnown[12]==0); // Original leaves no new sentinel.
    const auto samples=f.device.samples(),known=f.device.known();
    f.heapBridge.progress={};result=nba97_voice_mapping_unload(&f.mapping,Map,&f.progress);
    check(result.completion==1&&result.value==0&&f.get(Descriptors)==0x40001010);
    check(f.device.samples()==samples&&f.device.known()==known&&f.get(Map+0x2c)==0x1010);
}
void stereo_rounding_and_source_failure() {
    {Fixture f;f.put(Map+6,2,1);auto result=f.upload();
        check(result.completion==1&&result.value==0&&f.copied==2&&f.interrupts==2);
        check(f.get(Map+0x2c)==0x1010&&f.get(Map+0x30)==0x1020);
        // Two 16-byte allocations still each transfer64bytes. The original
        // second transfer overwrites part of the first transfer's padding.
        for(size_t i=0;i<16;++i)check(f.device.samples()[0x1010+i]==f.bytes[5][i]);
        for(size_t i=0;i<64;++i)check(f.device.samples()[0x1020+i]==f.bytes[5][32+i]);
        check(std::count(f.device.known().begin(),f.device.known().end(),uint8_t(1))==80);}
    {Fixture f;f.put(Map+6,2,1);f.put(Map+0x28,24);auto result=f.upload();
        // Original compares the right result with the LEFT length, after
        // actually transferring the right samples. Both allocations remain.
        check(result.completion==1&&result.value==-1&&f.copied==2);
        check(f.get(Map+0x2c)==0x1010&&f.get(Map+0x30)==0xffffffffu&&f.get(0x800c7a88u)==2);}
}
void ownership_and_events() {
    {Fixture f;f.known[5][16]=0;auto result=f.upload();
        check(result.completion==NBA97_PATL_IO_REFUSED&&f.device.lastResult().status==nba97::SpuSampleStatus::Unknown);
        check(f.device.request().phase==Phase::Requested&&f.copied==0&&f.interrupts==0);
        check(std::count(f.device.known().begin(),f.device.known().end(),uint8_t(1))==0);
        check(f.get(Descriptors)==0x1010&&f.get(Map+0x2c)==0xffffffffu&&f.get(0x800c6d2du,1)==1);}
    {Fixture f;f.service=false;f.mapping.step_budget=200;auto result=f.upload();
        check(result.completion==NBA97_MAPPING_LIMIT&&f.copied==0&&f.device.request().phase==Phase::Requested);}
    {Fixture f;f.service=false;check(f.device.deliverEvent(0xf0000009,0x20));auto result=f.upload();
        // A stale completion can satisfy the original wait before this new
        // copy. Do not clear the event at start to hide this original behavior.
        check(result.completion==1&&result.value==0&&f.copied==0&&f.device.request().phase==Phase::Requested);
        check(std::count(f.device.known().begin(),f.device.known().end(),uint8_t(1))==0);}
}
void bridge_limits_and_returns() {
    {Fixture f;f.transferBridge.access_budget=0;auto result=f.upload();
        check(result.completion==NBA97_PATL_IO_REFUSED&&f.transferBridge.progress.completion==NBA97_SPU_TRANSFER_LIMIT);
        check(f.heapBridge.progress.operations_completed==1&&f.transferBridge.progress.accesses==0&&f.device.request().phase==Phase::Idle);}
    {Fixture f;f.transferBridge.journal_capacity=1;auto result=f.upload();
        check(result.completion==NBA97_PATL_IO_REFUSED&&f.transferBridge.progress.events==1);
        check(f.transferBridge.progress.completion==NBA97_SPU_TRANSFER_LIMIT&&f.device.request().phase==Phase::Idle);}
    {Fixture f;f.transferBridge.io=[](void*,const Nba97VoicePatlMemory*,const Nba97SpuTransferEvent*,Nba97SpuTransferValue* out) {
            *out={0xabcdef01,0};return 1;
        };
        Nba97VoiceMappingEvent event{NBA97_MAPPING_TEST_EVENT_7F568,f.handle,0};uint32_t result=0x12345678;
        check(nba97_spu_transfer_mapping_invoke(&f.transferBridge,&f.mapping.memory,&event,&result)==0);
        check(result==0x12345678&&f.transferBridge.progress.completion==NBA97_PATL_RESOURCE);
        check(f.transferBridge.progress.last.completed&&f.transferBridge.progress.last.callbacks_completed==1);}
    {Fixture f;Nba97VoiceMappingEvent event{NBA97_MAPPING_STREAM_RESET_7390C,1,2};uint32_t result=99;
        check(nba97_spu_transfer_mapping_invoke(&f.transferBridge,&f.mapping.memory,&event,&result)==0&&result==99);}
}
}
int main() {
    actual_upload_and_unload();stereo_rounding_and_source_failure();ownership_and_events();bridge_limits_and_returns();
    std::printf("spu transfer mapping: %u checks passed\n",checks);
}
