// Shared source-static inputs and explicit device/policy fixtures. This does
// not turn the fixture's observations into a hardware timing or playback claim.
#define main nba97_mapping_fixture_main
#include "spu_transfer_mapping_tests.cpp"
#undef main
#include "audio_startup_backend.hpp"
#include <limits>

namespace {
constexpr uint32_t Parameters=0x80160000u;
struct AudioFixture {
    Fixture f{false};
    std::array<std::vector<uint8_t>,5> added,addedKnown;
    std::array<Nba97VoicePatlSpan,11> spans{};
    std::array<Nba97AudioStartupEvent,256> journal{};
    std::array<Nba97SpuInitializeEvent,512> initializeJournal{};
    std::array<Nba97SpuHeapStore,32> heapJournal{};
    Nba97AudioStartupProgress progress{};
    nba97::AudioStartupIoContext binding{};
    AudioFixture() {
        const uint32_t bases[]={0x800c6d28u,0x800c6d4cu,0x800d96e8u,0x800e45e4u,Parameters};
        const size_t sizes[]={2,4,32,76,512};
        for(size_t i=0;i<added.size();++i) {
            added[i].assign(sizes[i],0xcd);addedKnown[i].assign(sizes[i],0);
            spans[i+6]={added[i].data(),addedKnown[i].data(),sizes[i],bases[i],1,1,0};
        }
        binding.accessBudget=10000;binding.initializeCapacity=initializeJournal.size();binding.heapCapacity=heapJournal.size();
        rebind();
        // Only these proven source-file inputs are imported. Reset destinations
        // and unused heap records remain unknown until their actual stores.
        f.put(0x800c6d28u,0,1);
        for(unsigned i=0;i<8;++i)f.put(0x800d96e8u+4*i,0);
    }
    void rebind() {
        for(size_t i=0;i<f.spans.size();++i) {
            f.spans[i].data=f.bytes[i].data();f.spans[i].known=f.known[i].data();
            spans[i]=f.spans[i];
        }
        for(size_t i=0;i<added.size();++i) { spans[i+6].data=added[i].data();spans[i+6].known=addedKnown[i].data(); }
        f.mapping.memory={spans.data(),spans.size()};f.mapping.context=&f.heapBridge;
        f.heapBridge.platform_context=&f.transferBridge;f.heapBridge.journal=f.heapJournal.data();
        f.transferBridge.io_context=&f;f.transferBridge.journal=f.transferJournal.data();
        f.table={f.tableBytes.data(),f.tableKnown.data(),f.tableBytes.size()};
        f.binding.backend=&f.device;f.binding.externalUser=&f;
        f.eventBinding.backend=&f.events;f.eventBinding.samples=&f.device;f.eventBinding.externalUser=&f;
        f.controllerBinding.backend=&f.controller;f.controllerBinding.events=f.eventBinding;f.controllerBinding.externalUser=&f;
        f.initializeBinding.controller=&f.controllerBinding;f.initializeBinding.transfers=&f.binding;
        f.initializeBinding.controllerJournal=f.controllerJournal.data();f.initializeBinding.eventJournal=f.eventJournal.data();
        f.initializeBinding.transferJournal=f.pioJournal.data();
        binding.initialize=&f.initializeBinding;binding.initializeJournal=initializeJournal.data();binding.heapJournal=heapJournal.data();
    }
    int run(Nba97AudioStartupOperation op=NBA97_AUDIO_STARTUP_700B0,uint32_t a0=0,size_t budget=10000,size_t capacity=256) {
        Nba97AudioStartup c{f.mapping.memory,nba97::audioStartupIo,&binding,budget};
        return nba97_audio_startup(&c,op,a0,journal.data(),capacity,&progress);
    }
    void newInvocation() {
        binding.accesses=0;binding.initializeEvents=0;binding.heapStores=0;binding.operationsCompleted=0;
        f.initializeBinding.accesses=0;f.initializeBinding.controllerEvents=0;f.initializeBinding.eventEvents=0;
        f.initializeBinding.transferEvents=0;f.initializeBinding.operationsCompleted=0;
    }
};
void ordinary_audio_startup() {
    AudioFixture a;check(a.run()==1&&a.progress.completed&&!a.progress.transferred);
    check(a.progress.returned.known&&a.progress.returned.word==0&&a.binding.operationsCompleted==2);
    check(a.f.initializeBinding.operationsCompleted==3&&a.f.pioCopies==1&&a.binding.heapStores==5);
    check(a.f.controller.hookInstalled()&&a.f.get(0x800c7e4cu)==0x8007d668u);
    check(a.f.get(0x800c7a84u)==128&&a.f.get(0x800c7a8cu)==Descriptors);
    check(a.f.get(Descriptors)==0x40001010u&&a.f.get(Descriptors+4)==0x7eff0u);
    for(size_t i=8;i<a.f.known[1].size();++i)check(a.f.known[1][i]==0);
    check(a.f.get(0x800c6d28u,1)==1&&a.f.get(0x800c6d29u,1)==0&&a.f.get(0x800c6d4cu)==0);
    check(a.f.get(0x800d96e8u)==0x8007a6a8u);
    for(unsigned i=1;i<8;++i)check(a.f.get(0x800d96e8u+4*i)==0);
    check(a.f.get(0x800e45e4u,1)==255&&a.f.get(0x800e45e7u,1)==1&&a.f.get(0x800e45e8u,1)==2);
    check(a.f.get(0x800e460cu)==0xffffffffu&&a.f.get(0x800e462cu)==0);
    for(uint32_t hole:{0x800e4602u,0x800e4603u,0x800e4608u,0x800e460bu,0x800e4620u,0x800e4623u}) {
        uint32_t value=0;check(nba97_voice_patl_read(&a.f.mapping.memory,hole,1,&value)==NBA97_PATL_RESOURCE);
    }
    Nba97SpuTransferValue value{};
    for(uint32_t offset:{0x180u,0x182u,0x1b0u,0x1b2u}) {
        check(a.f.device.writtenConfiguration(0x1f801c00u+offset,value)&&value.word==0x3fff);
    }
    check(a.f.device.readDevice(a.f.mapping.memory,Backend::Control,2,value)&&value.word==0xc001);
    unsigned parameters=0;
    for(size_t i=0;i<a.progress.events;++i)if(a.journal[i].kind==NBA97_AUDIO_STARTUP_PARAMETER_STORE) {
        const auto& e=a.journal[i];const uint32_t offsets[]={0,4,6,16,18,24};
        check(e.address==offsets[parameters%6]&&e.completed);
        ++parameters;
    }
    check(parameters==12&&a.f.device.request().phase==Phase::Idle);
    uint32_t event=7;check(a.f.device.testEvent(a.f.get(0x800c7678u),event)&&event==0);
    // A new complete source invocation still initializes/resets, but guard1
    // preserves the original single registration. No timer callback runs here.
    a.f.put(0x800e4624u,0xabcdef01u);a.newInvocation();check(a.run()==1);
    check(a.f.pioCopies==2&&a.f.get(0x800e4624u)==0&&a.f.get(0x800d96ecu)==0);
}
void registrations_and_known_prefix() {
    for(unsigned empty=0;empty<=8;++empty) {
        AudioFixture a;for(unsigned i=0;i<8;++i)a.f.put(0x800d96e8u+4*i,i==empty?0:0x12340000u+i);
        check(a.run()==1&&a.f.get(0x800c6d28u,1)==1);
        for(unsigned i=0;i<8;++i)check(a.f.get(0x800d96e8u+4*i)==(i==empty?0x8007a6a8u:0x12340000u+i));
        // Source publishes guard before the unchecked full-table outcome.
        if(empty==8)check(a.progress.returned.word==0);
    }
    {AudioFixture a;a.addedKnown[0][0]=0;
        check(a.run()==NBA97_PATL_RESOURCE&&a.progress.stopped_pc==0x80070158u);
        check(a.binding.operationsCompleted==2&&a.f.get(0x800e45e8u,1)==2);}
    {AudioFixture a;a.addedKnown[2][0]=0;
        check(a.run()==NBA97_PATL_RESOURCE&&a.progress.stopped_pc==0x8008e0ecu);
        check(a.f.get(0x800c6d28u,1)==1&&a.f.pioCopies==1);}
    {AudioFixture a;a.addedKnown[3][0]=2;
        check(a.run()==NBA97_PATL_METADATA&&a.progress.stopped_pc==0x80073aacu);
        check(a.f.get(0x800c6d29u,1)==0&&a.f.get(0x800e45e8u,1)==2&&a.f.get(0x800c6d28u,1)==0);}
}
void lower_budget_prefixes() {
    AudioFixture full;check(full.run()==1);const size_t used=full.binding.accesses;
    for(size_t budget=0;budget<used;++budget) {
        AudioFixture a;a.binding.accessBudget=budget;
        check(a.run()==NBA97_PATL_IO_REFUSED&&a.binding.completion==NBA97_AUDIO_STARTUP_LIMIT);
        check(a.binding.accesses==budget&&!a.binding.active&&!a.progress.completed&&a.f.get(0x800c6d28u,1)==0);
    }
    for(unsigned kind=0;kind<2;++kind) {
        const size_t usedEvents=kind?full.binding.heapStores:full.binding.initializeEvents;
        for(size_t capacity=0;capacity<usedEvents;++capacity) {
            AudioFixture a;if(kind)a.binding.heapCapacity=capacity;else a.binding.initializeCapacity=capacity;
            check(a.run()==NBA97_PATL_IO_REFUSED&&!a.binding.active);
            check((kind?a.binding.heapStores:a.binding.initializeEvents)==capacity);
            check(a.binding.operationsCompleted==(kind?1u:0u)&&a.f.get(0x800c6d28u,1)==0);
        }
    }
}
void guards_and_heap_aliases() {
    for(unsigned kind=0;kind<8;++kind) {
        AudioFixture a;
        switch(kind) {
        case 0:a.binding.initialize=nullptr;break;
        case 1:a.binding.initializeJournal=nullptr;break;
        case 2:a.binding.heapJournal=nullptr;break;
        case 3:a.binding.accesses=a.binding.accessBudget+1;break;
        case 4:a.binding.initializeEvents=a.binding.initializeCapacity+1;break;
        case 5:a.binding.heapStores=a.binding.heapCapacity+1;break;
        case 6:a.binding.operationsCompleted=std::numeric_limits<size_t>::max();break;
        case 7:a.binding.active=true;break;
        }
        check(a.run()==NBA97_PATL_IO_REFUSED&&a.progress.stopped_pc==0x800700c8u);
        check(!a.f.controller.contextCaptured()&&!a.f.pioCopies&&a.f.get(0x800c6d28u,1)==0);
    }
    // Exact standalone lower invocation: descriptor storage aliases the live
    // heap globals. No detached projection can replace the actual source stores.
    AudioFixture a;a.f.put(0x800c75ecu,3);
    Nba97AudioStartupEvent e{};e.kind=NBA97_AUDIO_STARTUP_HEAP;e.address=0x8007e940u;
    e.argument[0]=0x80;e.argument[1]=0x800c7a84u;Nba97SpuTransferValue out{};
    check(nba97::audioStartupIo(&a.binding,&a.f.mapping.memory,&e,&out)==1&&out.known&&out.word==0x80);
    check(a.f.get(0x800c7a84u)==0x80&&a.f.get(0x800c7a88u)==0x7eff0u&&a.f.get(0x800c7a8cu)==0x800c7a84u);
    check(a.binding.heapStores==5&&a.binding.operationsCompleted==1);
    const auto before=a.f.bytes;
    for(auto kind:{NBA97_AUDIO_STARTUP_RAM_STORE,NBA97_AUDIO_STARTUP_PARAMETER_STORE}) {
        e.kind=kind;check(nba97::audioStartupIo(&a.binding,&a.f.mapping.memory,&e,&out)==0&&a.f.bytes==before);
    }
}
int terminal(void*,const Nba97VoicePatlMemory*,const Nba97SpuInitializeEvent* e,Nba97SpuTransferValue* out) {
    check(e->kind==NBA97_SPU_INITIALIZE_CONTROLLER&&e->address==0x80012340u);
    *out={0xabcdef01,0};return 2;
}
void transferred_and_cloned() {
    AudioFixture terminalCase;terminalCase.f.put(0x800c7db8u,0x80012340u);
    terminalCase.f.initializeBinding.external=terminal;
    check(terminalCase.run()==NBA97_AUDIO_STARTUP_TRANSFERRED&&terminalCase.progress.transferred&&terminalCase.progress.completed);
    // Nonreturn transfer has no caller return register. Its actual callback
    // output belongs to the reached lower journal, not a manufactured v0.
    check(!terminalCase.progress.returned.known);
    const auto& transfer=terminalCase.initializeJournal[terminalCase.binding.initializeEvents-1];
    check(transfer.transferred&&transfer.returned.word==0xabcdef01&&!transfer.returned.known);
    check(terminalCase.binding.operationsCompleted==1&&terminalCase.binding.heapStores==0&&!terminalCase.f.pioCopies);
    check(terminalCase.progress.events==1&&terminalCase.journal[0].transferred&&terminalCase.f.get(0x800c6d28u,1)==0);
    AudioFixture original;AudioFixture copy=original;copy.rebind();
    copy.f.binding.memoryGeneration=17;copy.f.controllerBinding.memoryGeneration=17;
    check(copy.run()==1&&copy.f.device.pioRequest().memoryGeneration==17);
    check(!original.f.controller.hookInstalled()&&original.f.device.known()[0x1000]==0&&original.f.get(0x800c6d28u,1)==0);
    check(original.run()==1&&original.f.device.pioRequest().memoryGeneration==1);
    copy.f.put(0x800e460cu,123);check(original.f.get(0x800e460cu)==0xffffffffu);
}
void common_setter_device_aliases() {
    AudioFixture a;a.f.binding.memoryGeneration=17;a.f.controllerBinding.memoryGeneration=17;
    check(a.f.device.importRegister(Backend::Control,2,{0,1}));
    check(a.f.device.importRegister(Backend::TransferControl,2,{4,1}));
    check(a.f.device.writeDevice(a.f.mapping.memory,Backend::TransferAddress,2,0x210,17));
    a.f.put(Parameters,1);a.f.put(Parameters+4,0x4321,2);
    a.f.put(0x800c75c8u,Backend::Fifo-0x180);
    check(a.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameters)==1);
    check(a.f.device.pioRequest().phase==nba97::SpuPioPhase::Filling&&a.f.device.pioRequest().memoryGeneration==17);
    check(a.binding.accesses==0&&a.binding.operationsCompleted==0&&a.f.device.known()[0x1080]==0);
    a.f.put(0x800c75c8u,Backend::Control-0x180);a.f.put(Parameters+4,0x10,2);
    check(a.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameters)==1);
    check(a.f.device.pioRequest().phase==nba97::SpuPioPhase::Requested);
    check(a.f.device.servicePendingPio(1).status==nba97::SpuSampleStatus::StaleGeneration);
    check(a.f.device.servicePendingPio(17)&&a.f.device.samples()[0x1080]==0x21&&a.f.device.samples()[0x1081]==0x43);
    // An actual control read redirected to canonical status follows the same
    // required observation callback. The following unsupported status write
    // refuses, retaining the completed read event; it cannot become a cache.
    a.f.put(Parameters,0x200);a.f.put(Parameters+24,0);a.f.put(0x800c75c8u,Backend::Status-0x1aa);
    check(a.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameters)==NBA97_PATL_IO_REFUSED);
    check(a.progress.events==2&&a.journal[0].completed&&!a.journal[1].completed);
    check(a.journal[0].address==Backend::Status&&a.journal[0].returned.known&&a.journal[0].returned.word==0);
    // Real CPU-RAM aliases are resolved through the same retained registry.
    AudioFixture ram;ram.f.put(Parameters,1);ram.f.put(Parameters+4,0x7654,2);
    ram.f.put(0x800c75c8u,Parameters);
    check(ram.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameters)==1&&ram.f.get(Parameters+0x180,2)==0x7654);
    check(ram.f.device.pioRequest().phase==nba97::SpuPioPhase::Idle);
}
struct LiveObservation {
    AudioFixture* fixture;
    uint8_t known;
    unsigned calls=0;
};
int live_observation(void* p,const Nba97VoicePatlMemory* memory,const Nba97SpuTransferEvent* e,Nba97SpuTransferValue* out) {
    auto& c=*static_cast<LiveObservation*>(p);auto& a=*c.fixture;++c.calls;
    check(e->kind==NBA97_SPU_TRANSFER_DEVICE_READ&&e->address==Backend::Status);
    Nba97AudioStartupEvent nested{};nested.kind=NBA97_AUDIO_STARTUP_DEVICE_READ;
    nested.address=e->address;nested.width=e->width;Nba97SpuTransferValue ignored{};
    check(nba97::audioStartupIo(&a.binding,memory,&nested,&ignored)==0&&a.binding.active);
    check(nba97_voice_patl_write(memory,0x800c75c8u,4,Parameters)==1);
    *out={0xabcdef00,c.known};return 1;
}
void observation_prefixes() {
    for(uint8_t marker:{uint8_t(0),uint8_t(1),uint8_t(2)}) {
        AudioFixture a;a.f.put(Parameters,0x200);a.f.put(Parameters+24,0);
        a.f.put(0x800c75c8u,Backend::Status-0x1aa);
        LiveObservation observer{&a,marker};a.f.binding.external=live_observation;a.f.binding.externalUser=&observer;
        const int rc=a.run(NBA97_AUDIO_STARTUP_COMMON_7DEA8,Parameters);
        check(rc==(marker==0?NBA97_PATL_RESOURCE:marker==2?NBA97_PATL_METADATA:NBA97_PATL_IO_REFUSED));
        check(observer.calls==1&&!a.binding.active&&a.binding.accesses==0);
        check(a.journal[0].completed&&a.journal[0].returned.known==marker&&a.journal[0].returned.word==0xabcdef00);
        check(a.f.get(0x800c75c8u)==Parameters);
        if(marker==1) {
            // Original common control caches its base across the callback.
            // The following write still targets Status, not changed RAM base.
            check(a.progress.events==2&&a.journal[1].address==Backend::Status&&!a.journal[1].completed);
            uint32_t value=0;check(nba97_voice_patl_read(&a.f.mapping.memory,Parameters+0x1aa,2,&value)==NBA97_PATL_RESOURCE);
        } else check(a.progress.events==1);
    }
}
}
int main() {
    ordinary_audio_startup();registrations_and_known_prefix();lower_budget_prefixes();guards_and_heap_aliases();transferred_and_cloned();common_setter_device_aliases();observation_prefixes();
    std::printf("audio startup backend: %u checks passed\n",checks);return 0;
}
