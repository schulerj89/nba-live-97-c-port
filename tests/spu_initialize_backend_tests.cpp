// Reuse the integration fixture's original static inputs and explicit platform
// conditions, so these checks exercise the same production composition.
#define main nba97_mapping_fixture_main
#include "spu_transfer_mapping_tests.cpp"
#undef main
#include <limits>
namespace {
int startup(Fixture& f) {
    Nba97SpuInitialize cpu{f.mapping.memory,nba97::spuInitializeIo,&f.initializeBinding,10000};
    return nba97_spu_initialize(&cpu,NBA97_SPU_INITIALIZE_7E6EC,0,0,0,
        f.initializeJournal.data(),f.initializeJournal.size(),&f.initializeProgress);
}
void complete_startup() {
    Fixture f(false);check(startup(f)==1);
    check(f.initializeBinding.operationsCompleted==3&&f.pioCopies==1);
    check(f.controller.hookInstalled()&&f.get(0x800c7e4cu)==0x8007d668u);
    check(f.initializeBinding.controllerEvents>0&&f.initializeBinding.transferEvents>0&&f.initializeBinding.eventEvents>0);
    check(f.get(0x800c75ecu)==3&&f.get(0x800c7630u)==0xfffe);
    for(uint32_t i=0;i<24;++i) {
        Nba97SpuTransferValue value{};
        check(f.device.writtenConfiguration(0x1f801c00u+i*16+4,value)&&value.word==0x3fff);
        check(f.device.writtenConfiguration(0x1f801c00u+i*16+6,value)&&value.word==0x200);
        check(f.get(0x800c7648u+i*2,2)==0xc000);
    }
    for(uint32_t i=0;i<16;++i)check(f.device.samples()[0x1000+i]==7&&f.device.known()[0x1000+i]==1);
    uint32_t event=9;check(f.device.testEvent(f.get(0x800c7678u),event)&&event==0);
    check(f.device.request().phase==Phase::Idle); // PIO did not fake a DMA/ISR/event.
}
void lower_limits() {
    Fixture full(false);check(startup(full)==1);
    const auto accessCount=full.initializeBinding.accesses;
    for(size_t budget=0;budget<accessCount;++budget) {
        Fixture f(false);f.initializeBinding.accessBudget=budget;
        check(startup(f)==NBA97_PATL_IO_REFUSED&&f.initializeBinding.completion==NBA97_INTERRUPT_LIMIT);
        check(f.initializeBinding.accesses==budget&&!f.initializeProgress.completed&&!f.initializeBinding.active);
    }
    for(unsigned journal=0;journal<3;++journal) {
        const auto count=journal==0?full.initializeBinding.controllerEvents:
            journal==1?full.initializeBinding.transferEvents:full.initializeBinding.eventEvents;
        for(size_t cap=0;cap<count;++cap) {
            Fixture f(false);
            if(journal==0)f.initializeBinding.controllerCapacity=cap;
            if(journal==1)f.initializeBinding.transferCapacity=cap;
            if(journal==2)f.initializeBinding.eventCapacity=cap;
            check(startup(f)==NBA97_PATL_IO_REFUSED&&f.initializeBinding.completion==NBA97_INTERRUPT_LIMIT);
            check(!f.initializeBinding.active&&!f.initializeProgress.completed);
            check((journal==0?f.initializeBinding.controllerEvents:journal==1?f.initializeBinding.transferEvents:f.initializeBinding.eventEvents)==cap);
        }
    }
}
void missing_provenance() {
    {Fixture f(false);f.binding.external=nullptr;
        check(startup(f)==NBA97_PATL_IO_REFUSED&&f.initializeProgress.stopped_pc==0x8007cedcu);
        check(f.controller.hookInstalled()&&!f.pioCopies&&f.initializeBinding.operationsCompleted==1);}
    {Fixture f(false);f.known[0][0x108]=0;
        check(startup(f)==NBA97_PATL_IO_REFUSED&&f.initializeBinding.transferProgress.stopped_pc==0x8007d3f4u);
        check(f.device.pioRequest().phase==nba97::SpuPioPhase::Filling&&f.device.pioRequest().bytes==4);
        check(!f.pioCopies&&f.get(0x800c7a80u)==0);}
    {Fixture f(false);f.eventBinding.external=nullptr;f.controllerBinding.events.external=nullptr;
        check(startup(f)==NBA97_PATL_IO_REFUSED&&!f.pioCopies&&!f.initializeProgress.completed);}
    for(unsigned kind=0;kind<10;++kind) {
        Fixture f(false);
        switch(kind) {
        case 0:f.initializeBinding.controller=nullptr;break;
        case 1:f.initializeBinding.transfers=nullptr;break;
        case 2:f.binding.memoryGeneration=2;break;
        case 3:f.controllerBinding.events.samples=nullptr;break;
        case 4:f.initializeBinding.accesses=f.initializeBinding.accessBudget+1;break;
        case 5:f.initializeBinding.controllerJournal=nullptr;break;
        case 6:f.initializeBinding.eventEvents=f.initializeBinding.eventCapacity+1;break;
        case 7:f.initializeBinding.transferEvents=f.initializeBinding.transferCapacity+1;break;
        case 8:f.initializeBinding.operationsCompleted=std::numeric_limits<size_t>::max();break;
        case 9:f.initializeBinding.active=true;break;
        }
        check(startup(f)==NBA97_PATL_IO_REFUSED&&f.initializeProgress.stopped_pc==0x8007f5e8u);
        check(!f.controller.contextCaptured()&&f.get(0x800c7dc8u,2)==0&&!f.pioCopies);
    }
}
struct ForwardCheck { Fixture* fixture;const Nba97VoicePatlMemory* memory;const Nba97SpuInitializeEvent* event;unsigned calls=0;int rc=1; };
int external(void* p,const Nba97VoicePatlMemory* m,const Nba97SpuInitializeEvent* e,Nba97SpuTransferValue* out) {
    auto& state=*static_cast<ForwardCheck*>(p);++state.calls;
    if(state.memory)check(m==state.memory);
    if(state.event)check(e==state.event);
    Nba97SpuTransferValue unused{};
    check(nba97::spuInitializeIo(&state.fixture->initializeBinding,m,e,&unused)==0); // Reentry refuses, without replay.
    *out={0xabcdef01,0};return state.rc;
}
void forwarded_and_transferred() {
    Fixture f(false);Nba97SpuInitializeEvent e{};Nba97SpuTransferValue out{};
    e.kind=NBA97_SPU_INITIALIZE_DIAGNOSTIC;e.pc=0x8007cf28u;e.argument[0]=0x80027dd0u;e.argument[1]=0x80027de0u;
    ForwardCheck state{&f,&f.mapping.memory,&e};f.initializeBinding.external=external;f.initializeBinding.externalUser=&state;
    check(nba97::spuInitializeIo(&f.initializeBinding,&f.mapping.memory,&e,&out)==1&&state.calls==1);
    check(out.word==0xabcdef01&&!out.known&&f.initializeBinding.operationsCompleted==0);
    state.memory=nullptr;state.event=nullptr;state.rc=2;f.put(0x800c7db8u,0x80012340u);
    check(startup(f)==NBA97_SPU_INITIALIZE_TRANSFERRED&&f.initializeProgress.transferred);
    check(!f.controller.contextCaptured()&&!f.pioCopies&&f.get(0x800c7a80u)==0);
    check(f.initializeBinding.accesses==0&&!f.initializeBinding.active);
}
void distinct_generation_register_alias() {
    Fixture f(false);f.binding.memoryGeneration=17;f.controllerBinding.memoryGeneration=17;
    // The event registry retains generation1. It is not the CPU allocation17.
    check(startup(f)==1&&f.device.pioRequest().memoryGeneration==17);
    Nba97SpuInitialize cpu{f.mapping.memory,nba97::spuInitializeIo,&f.initializeBinding,10000};
    auto write=[&](uint32_t index,uint32_t value) {
        return nba97_spu_initialize(&cpu,NBA97_SPU_INITIALIZE_REGISTER_7DD80,index,value,0,
            f.initializeJournal.data(),f.initializeJournal.size(),&f.initializeProgress);
    };
    check(write(0xd3,0x210)==1); // Actual register setter reaches transfer address.
    check(write(0xd4,0x4321)==1&&f.device.pioRequest().memoryGeneration==17);
    check(write(0xd5,0xc010)==1&&f.device.pioRequest().phase==nba97::SpuPioPhase::Requested);
    check(f.device.servicePendingPio(1).status==nba97::SpuSampleStatus::StaleGeneration);
    check(f.device.servicePendingPio(17)&&f.device.samples()[0x1080]==0x21&&f.device.samples()[0x1081]==0x43);
}
void interrupt_register_overlap() {
    for(uint32_t address:{nba97::SpuEventBackend::IrqMask,nba97::SpuEventBackend::Dicr}) {
        Fixture f(false);
        std::array<uint8_t,4> bytes{{0x12,0x34,0x56,0x78}},known{{1,1,1,1}};
        const auto before=bytes;
        std::array<Nba97VoicePatlSpan,7> spans{};
        std::copy(f.spans.begin(),f.spans.end(),spans.begin());
        spans[6]={bytes.data(),known.data(),bytes.size(),address,1,1,0};
        Nba97VoicePatlMemory memory{spans.data(),spans.size()};
        Nba97SpuInitializeEvent event{};Nba97SpuTransferValue out{};
        event.kind=NBA97_SPU_INITIALIZE_DEVICE_WRITE;event.address=address;event.value=0xabcd;
        event.width=address==nba97::SpuEventBackend::IrqMask?2u:4u;
        check(nba97::spuInitializeIo(&f.initializeBinding,&memory,&event,&out)==0);
        check(bytes==before&&f.events.lastResult().status==nba97::SpuSampleStatus::Ambiguous);
    }
}
}
int main() { complete_startup();lower_limits();missing_provenance();forwarded_and_transferred();distinct_generation_register_alias();interrupt_register_overlap();
    std::printf("spu initialize backend: %u checks passed\n",checks); }
