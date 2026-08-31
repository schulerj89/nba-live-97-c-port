#include "recovered/spu_transfer_mapping.h"
#include "recovered/spu_heap_mapping.h"
#include "spu_sample_backend.hpp"
#include "spu_event_backend.hpp"
#include "interrupt_controller_backend.hpp"
#include "spu_initialize_backend.hpp"
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
    nba97::SpuEventBackend events;
    nba97::InterruptControllerBackend controller;
    nba97::SpuSampleIoContext binding{&device,1,nullptr,nullptr};
    nba97::SpuEventIoContext eventBinding{&events,&device,1,nullptr,nullptr};
    std::array<Nba97SpuEventsEvent,128> eventJournal{};
    Nba97SpuEventsProgress eventProgress{};
    nba97::InterruptControllerIoContext controllerBinding{};
    std::array<Nba97InterruptEvent,512> controllerJournal{};
    Nba97InterruptProgress controllerProgress{};
    std::array<Nba97SpuInitializeEvent,512> initializeJournal{};
    std::array<Nba97SpuTransferEvent,128> pioJournal{};
    Nba97SpuInitializeProgress initializeProgress{};
    nba97::SpuInitializeIoContext initializeBinding{};
    uint32_t irqStatus=0,dicr=0,timerMode=0;
    std::array<uint32_t,4> keyCommands{};
    unsigned pioCopies=0;
    std::array<uint32_t,4> counterPolicy{};
    uint32_t handle=0;
    bool service=true;
    unsigned copied=0,interrupts=0;
    explicit Fixture(bool initializeNow=true) {
        const uint32_t bases[]={0x800c7500u,Descriptors,0x800c6d2cu,0x800f9600u,Map,Body};
        const size_t sizes[]={0xa00,129*8,2,4,64,128};
        for(size_t i=0;i<spans.size();++i) {
            bytes[i].assign(sizes[i],0xcd);known[i].assign(sizes[i],0);
            spans[i]={bytes[i].data(),known[i].data(),sizes[i],bases[i],1,1,0};
        }
        heapBridge.platform=nba97_spu_transfer_mapping_invoke;heapBridge.platform_context=&transferBridge;
        heapBridge.access_budget=10000;heapBridge.journal=heapJournal.data();heapBridge.journal_capacity=heapJournal.size();
        transferBridge.io=io;transferBridge.io_context=this;transferBridge.access_budget=10000;
        transferBridge.journal=transferJournal.data();transferBridge.journal_capacity=transferJournal.size();
        mapping={{spans.data(),spans.size()},nba97_spu_heap_mapping_invoke,&heapBridge,10000};
        // Original FE data pointers and seed bytes, plus explicit incoming
        // platform DPCR/bus state. The actual sound/controller/event startup
        // runs below; no prepared software mode, voice flags or event handle.
        put(0x800c6d2cu,0,1);put(0x800f9600u,0);
        put(0x800c75d8u,Backend::Dpcr);put(0x800c7a90u,0xfffe);
        for(uint32_t i=0;i<16;++i)put(0x800c7604u+i,7,1);
        put(0x800c75c8u,0x1f801c00);put(0x800c75ccu,Backend::Madr);
        put(0x800c75d0u,Backend::Bcr);put(0x800c75d4u,Backend::Chcr);put(0x800c75dcu,Backend::BusDelay);
        put(Map+6,1,1);put(Map+0x1c,0);put(Map+0x20,32);put(Map+0x24,16);put(Map+0x28,16);
        put(Map+0x2c,0xffffffffu);put(Map+0x30,0xffffffffu);
        for(uint32_t i=0;i<128;++i)put(Body+i,(i*3+7)&255,1);
        check(nba97_voice_mapping_table_write(&table,0,0xffffffffu)==1);
        check(device.importRegister(Backend::Dpcr,4,{0,1}));
        check(device.importRegister(Backend::BusDelay,4,{0,1}));
        binding.external=sampleHardware;binding.externalUser=this;
        put(0x800c7a80u,0);put(0x800c7dc4u,0x800c7dacu);put(0x800c7db0u,0);
        put(0x800c7db8u,0x8007f708u);put(0x800c7db4u,0x8007f9bcu);put(0x800c7dc8u,0,2);
        put(0x800c7e2cu,0x1f801070u);put(0x800c7e64u,0x1f801114u);put(0x800c7e70u,0);
        put(0x800c7e30u,nba97::SpuEventBackend::IrqMask);put(0x800c7e5cu,nba97::SpuEventBackend::Dicr);
        check(events.importRegister(nba97::SpuEventBackend::IrqMask,2,{0,1}));
        // General DICR is consistently owned by the explicit external fixture;
        // do not mix its pending flags with a retained registration-only cache.
        check(events.importRegister(nba97::SpuEventBackend::Dicr,4,{0,0}));
        eventBinding.external=eventHardware;eventBinding.externalUser=this;
        controllerBinding={&controller,eventBinding,1,controllerIo,this};
        check(controller.importIso9660Driver({1,1}));check(controller.importPadClearPolicy({1,1}));
        initializeBinding.controller=&controllerBinding;initializeBinding.transfers=&binding;
        initializeBinding.controllerJournal=controllerJournal.data();initializeBinding.controllerCapacity=controllerJournal.size();
        initializeBinding.eventJournal=eventJournal.data();initializeBinding.eventCapacity=eventJournal.size();
        initializeBinding.transferJournal=pioJournal.data();initializeBinding.transferCapacity=pioJournal.size();
        initializeBinding.accessBudget=10000;
        if(!initializeNow)return;
        Nba97SpuInitialize startup{mapping.memory,nba97::spuInitializeIo,&initializeBinding,10000};
        int startupRc=nba97_spu_initialize(&startup,NBA97_SPU_INITIALIZE_7E6EC,0,0,0,
            initializeJournal.data(),initializeJournal.size(),&initializeProgress);
        if(startupRc!=1)std::fprintf(stderr,"startup %d pc %08x lower %d pio %08x device %d\n",startupRc,
            initializeProgress.stopped_pc,initializeBinding.completion,initializeBinding.transferProgress.stopped_pc,int(device.lastResult().status));
        check(startupRc==1&&initializeProgress.completed&&initializeProgress.returned.word==0x1f801da2u);
        check(pioCopies==1&&initializeBinding.operationsCompleted==3);
        for(uint32_t i=0;i<16;++i)check(device.samples()[0x1000+i]==7&&device.known()[0x1000+i]==1);
        Nba97SpuTransferValue configuration{};
        check(device.writtenConfiguration(0x1f801da2u,configuration)&&configuration.word==0xfffe);
        Nba97SpuHeap heap{mapping.memory,1000};Nba97SpuHeapProgress init{};
        check(nba97_spu_heap(&heap,NBA97_SPU_HEAP_INITIALIZE_7E940,128,Descriptors,heapJournal.data(),heapJournal.size(),&init)==1);
        check(controller.contextCaptured()&&controller.hookInstalled());
        check(get(0x800c7db0u)==0x8007fdb8u&&get(0x800c7e38u,2)==1);
        eventProgress=initializeBinding.eventProgress;
        handle=get(0x800c7678u);
        check(handle!=0&&get(0x800c7e4cu)==0x8007d668u&&get(0x800c7a80u)==1);
        check(eventProgress.completed&&!eventProgress.returned.known);
        check(events.criticalEnabled().known&&events.criticalEnabled().word==1);
    }
    void put(uint32_t at,uint32_t value,uint32_t width=4) { check(nba97_voice_patl_write(&mapping.memory,at,width,value)==1); }
    uint32_t get(uint32_t at,uint32_t width=4) { uint32_t value=0;check(nba97_voice_patl_read(&mapping.memory,at,width,&value)==1);return value; }
    int eventOperation(Nba97SpuEventsOperation operation) {
        Nba97SpuEvents owner{mapping.memory,nba97::SpuEventBackend::io,&eventBinding,10000};
        return nba97_spu_events(&owner,operation,0,0,0,0,eventJournal.data(),eventJournal.size(),&eventProgress);
    }
    int controllerOperation(Nba97InterruptOperation operation) {
        return controller.run(controllerBinding,mapping.memory,operation,0,0,{0,0},controllerJournal.data(),
            controllerJournal.size(),controllerProgress,10000);
    }
    static int sampleHardware(void* p,const Nba97VoicePatlMemory* memory,const Nba97SpuTransferEvent* e,Nba97SpuTransferValue* out) {
        auto& f=*static_cast<Fixture*>(p);check(memory->spans==f.mapping.memory.spans);
        if(e->kind!=NBA97_SPU_TRANSFER_DEVICE_READ||e->address!=Backend::Status||e->width!=2)return 0;
        // Explicit synchronous service point and scripted status observation.
        // The backend copies the queued original bytes; it does not invent the
        // readback, physical cadence, interrupt or source completion event.
        if(f.device.pioRequest().phase==nba97::SpuPioPhase::Requested) {
            if(!f.device.servicePendingPio(f.binding.memoryGeneration))return 0;
            ++f.pioCopies;
        }
        *out={0,1};return 1;
    }
    static int eventHardware(void* p,const Nba97VoicePatlMemory* memory,const Nba97SpuEventsEvent* e,Nba97SpuTransferValue* out) {
        auto& f=*static_cast<Fixture*>(p);check(memory->spans==f.mapping.memory.spans);
        const bool write=e->kind==NBA97_SPU_EVENTS_DEVICE_WRITE;
        if(!write&&e->kind!=NBA97_SPU_EVENTS_DEVICE_READ)return 0;
        if(e->address>=0x1f801d88u&&e->address<=0x1f801d8eu&&e->width==2) {
            auto& command=f.keyCommands[(e->address-0x1f801d88u)/2];
            if(write)command=e->value;else *out={command,1};return 1;
            // Explicit key-command fixture observations, not synthesized voices.
        }
        if(e->address==0x1f801070u&&e->width==2) {
            if(write)f.irqStatus&=e->value;else *out={f.irqStatus,1};return 1;
        }
        if(e->address==0x1f801114u&&e->width==4) {
            if(write)f.timerMode=e->value;else *out={f.timerMode,1};return 1;
        }
        if(e->address==nba97::SpuEventBackend::Dicr&&e->width==4) {
            if(write) {
                f.dicr=(f.dicr&0x7f000000u&~e->value)|(e->value&0x00ffffffu);
                f.updateDmaFlag();
            } else *out={f.dicr,1};
            return 1;
        }
        return 0;
    }
    void updateDmaFlag() {
        dicr&=0x7fffffffu;
        if((dicr&0x8000u)||((dicr&0x800000u)&&((dicr>>24)&(dicr>>16)&0x7fu)))dicr|=0x80000000u;
    }
    static int controllerIo(void* p,const Nba97VoicePatlMemory* memory,const Nba97InterruptEvent* e,Nba97SpuTransferValue* out) {
        auto& f=*static_cast<Fixture*>(p);check(memory->spans==f.mapping.memory.spans);
        if(e->kind==NBA97_INTERRUPT_CHANGE_CLEAR_COUNTER) {
            if(e->argument[0]>=4||e->argument[1]>1)return 0;
            f.counterPolicy[e->argument[0]]=e->argument[1];*out={};return 1;
        }
        if(e->kind!=NBA97_INTERRUPT_CALLBACK||e->address!=0x8007d668u)return 0;
        const auto ticket=f.device.request().ticket;
        if(!f.device.beginIsr(ticket))return 0;
        Nba97SpuTransfer cpu{*memory,io,&f,10000};Nba97SpuTransferProgress progress{};
        int rc=nba97_spu_transfer(&cpu,NBA97_SPU_ISR_7D668,0,0,0,f.isrJournal.data(),f.isrJournal.size(),&progress);
        const bool finished=bool(f.device.finishIsr(ticket,rc==1));
        if(rc!=1||!finished)return 0;
        ++f.interrupts;*out=progress.returned;return 1;
    }
    static int io(void* opaque,const Nba97VoicePatlMemory* memory,const Nba97SpuTransferEvent* event,Nba97SpuTransferValue* result) {
        auto& f=*static_cast<Fixture*>(opaque);
        check(memory->spans==f.mapping.memory.spans&&memory->count==f.mapping.memory.count);
        if(event->kind==NBA97_SPU_TRANSFER_TEST_EVENT_B0_0B&&f.service&&f.device.request().phase==Phase::Requested) {
            // Explicit scheduling/device fixture: service actual sample bytes,
            // signal its DMA source, and enter the installed native context.
            // Actual controller -> DMA dispatcher -> registered SPU ISR completes
            // the event. This does not claim physical device timing/cadence.
            if(!f.device.servicePendingDma(*memory,f.binding.memoryGeneration))return 0;
            ++f.copied;f.dicr|=0x10000000u;f.updateDmaFlag();
            if(!(f.dicr&0x80000000u))return 0;
            f.irqStatus|=8u;
            int rc=f.controller.enterException(f.controllerBinding,*memory,f.controllerJournal.data(),
                f.controllerJournal.size(),f.controllerProgress,10000);
            if(rc!=NBA97_INTERRUPT_TRANSFERRED||!f.controllerProgress.transferred)return 0;
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
    check(f.controller.exceptionPhase()==nba97::InterruptExceptionPhase::Returned);
    check(f.controllerProgress.transferred&&f.irqStatus==0&&!(f.dicr&0xff000000u));
    check(f.get(Map+0x2c)==0x1010&&f.get(0x800c6d2du,1)==0);
    check(f.heapBridge.progress.operations_completed==1&&f.transferBridge.progress.operations_completed==2);
    check(std::count(f.device.known().begin(),f.device.known().end(),uint8_t(1))==80);
    for(size_t i=0;i<64;++i)check(f.device.samples()[0x1010+i]==f.bytes[5][i]&&f.device.known()[0x1010+i]==1);
    uint32_t event=2;check(f.device.testEvent(f.handle,event)&&event==0);
    check(f.tableKnown[8]==0&&f.tableKnown[12]==0); // Original leaves no new sentinel.
    const auto samples=f.device.samples(),known=f.device.known();
    f.heapBridge.progress={};result=nba97_voice_mapping_unload(&f.mapping,Map,&f.progress);
    check(result.completion==1&&result.value==0&&f.get(Descriptors)==0x40001010);
    check(f.device.samples()==samples&&f.device.known()==known&&f.get(Map+0x2c)==0x1010);
    check(f.eventOperation(NBA97_SPU_EVENTS_SHUTDOWN_7E81C)==1);
    check(f.events.closedHandle(f.handle)&&f.get(0x800c7678u)==f.handle);
    check(f.get(0x800c7a80u)==0&&f.get(0x800c7e4cu)==0&&f.events.criticalEnabled().word==1);
    check(f.device.testEvent(f.handle,event).status==nba97::SpuSampleStatus::InvalidEvent);
    check(f.device.samples()==samples&&f.device.known()==known);
    auto oldHandle=f.handle;
    check(f.eventOperation(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==1);
    f.handle=f.get(0x800c7678u);
    check(f.handle!=oldHandle&&f.device.testEvent(f.handle,event)&&event==0);
    check(f.eventOperation(NBA97_SPU_EVENTS_SHUTDOWN_7E81C)==1);
    check(f.controllerOperation(NBA97_INTERRUPT_SHUTDOWN_7FAE4)==1);
    // Original shutdown clears the saved context without detaching its hook.
    // The native owner retains that hook but cannot enter a cleared context.
    check(f.controller.hookInstalled()&&f.get(0x800c7dfcu)==0);
    check(f.controller.enterException(f.controllerBinding,f.mapping.memory,f.controllerJournal.data(),
        f.controllerJournal.size(),f.controllerProgress,10000)==NBA97_PATL_IO_REFUSED);
    check(f.controllerProgress.events==0&&f.events.criticalEnabled().word==1);
    check(f.controllerOperation(NBA97_INTERRUPT_INITIALIZE_7F708)==1);
    check(f.eventOperation(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==1&&f.get(0x800c7678u)!=f.handle);
}
void stereo_rounding_and_source_failure() {
    {Fixture f;f.put(Map+6,2,1);auto result=f.upload();
        check(result.completion==1&&result.value==0&&f.copied==2&&f.interrupts==2);
        check(f.get(Map+0x2c)==0x1010&&f.get(Map+0x30)==0x1020);
        // Two 16-byte allocations still each transfer64bytes. The original
        // second transfer overwrites part of the first transfer's padding.
        for(size_t i=0;i<16;++i)check(f.device.samples()[0x1010+i]==f.bytes[5][i]);
        for(size_t i=0;i<64;++i)check(f.device.samples()[0x1020+i]==f.bytes[5][32+i]);
        check(std::count(f.device.known().begin(),f.device.known().end(),uint8_t(1))==96);}
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
        check(std::count(f.device.known().begin(),f.device.known().end(),uint8_t(1))==16);
        check(f.get(Descriptors)==0x1010&&f.get(Map+0x2c)==0xffffffffu&&f.get(0x800c6d2du,1)==1);}
    {Fixture f;f.service=false;f.mapping.step_budget=200;auto result=f.upload();
        check(result.completion==NBA97_MAPPING_LIMIT&&f.copied==0&&f.device.request().phase==Phase::Requested);}
    {Fixture f;f.service=false;check(f.device.deliverEvent(0xf0000009,0x20));auto result=f.upload();
        // A stale completion can satisfy the original wait before this new
        // copy. Do not clear the event at start to hide this original behavior.
        check(result.completion==1&&result.value==0&&f.copied==0&&f.device.request().phase==Phase::Requested);
        check(std::count(f.device.known().begin(),f.device.known().end(),uint8_t(1))==16);}
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
    std::printf("spu transfer mapping: %u checks passed\n",checks);return 0;
}
