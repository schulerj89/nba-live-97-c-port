#include "interrupt_controller_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <utility>

using namespace nba97;
namespace {
unsigned checks;
void check(bool x) { ++checks;if (!x) { std::fprintf(stderr,"interrupt backend check %u failed\n",checks);std::exit(1); } }
void good(SpuSampleResult r) { check(bool(r)); }
constexpr std::uint32_t Base=0x800c7500,Context=InterruptControllerBackend::ContextAddress;
constexpr std::uint64_t Generation=91;
struct Fixture {
    SpuSampleBackend samples;SpuEventBackend events;InterruptControllerBackend backend;
    std::vector<std::uint8_t> bytes,known;
    std::array<Nba97VoicePatlSpan,3> spans{};Nba97VoicePatlMemory memory{};
    InterruptControllerIoContext binding{};
    std::array<Nba97InterruptEvent,512> journal{};Nba97InterruptProgress progress{};
    std::uint32_t timer=0,status=0,externalMask=0;
    std::array<std::uint32_t,4> counter{{1,1,1,1}};
    unsigned deviceCalls=0,policyCalls=0,callbackCalls=0;
    bool refuseTimer=false,refuseCounter=false,refuseStatus=false,badReturn=false;
    int callbackResult=1;
    Fixture():bytes(0xa00,0xcd),known(0xa00,0) {
        bind();
        // Proven static pointer/dispatch values; mutable entry flags are
        // explicit fixtures, not an assertion that a loader ran here.
        for (auto p:std::array<std::pair<std::uint32_t,std::uint32_t>,15>{{
            {0x800c7dc4,0x800c7dac},{0x800c7db0,0},{0x800c7db4,0x8007f9bc},
            {0x800c7dc0,0},{0x800c7e2c,0x1f801070},{0x800c7e30,SpuEventBackend::IrqMask},
            {0x800c7e5c,SpuEventBackend::Dicr},{0x800c7e64,0x1f801114},
            {0x800c7e70,17},{0x800c7e34,0},{0x800c7a80,0},
            {0x800c7620,7},{0x800c7600,0x12345678},{0x800c75fc,0},{0x800c7678,0xabcdef01}}}) put(p.first,p.second);
        put(0x800c7dc8,0,2);
        good(events.importRegister(SpuEventBackend::IrqMask,2,{0,1}));
        good(events.importRegister(SpuEventBackend::Dicr,4,{0,1}));
        good(backend.importIso9660Driver({1,1}));good(backend.importPadClearPolicy({1,1}));
    }
    void bind() {
        spans[0]={bytes.data(),known.data(),bytes.size(),Base,1,1,0};spans[1]={};spans[2]={};memory={spans.data(),1};
        binding={&backend,{&events,&samples,Generation,device,this},Generation,policy,this};
    }
    void put(std::uint32_t a,std::uint32_t v,std::uint32_t w=4) { check(nba97_voice_patl_write(&memory,a,w,v)==1); }
    std::uint32_t get(std::uint32_t a,std::uint32_t w=4) { std::uint32_t v=0;check(nba97_voice_patl_read(&memory,a,w,&v)==1);return v; }
    static int device(void* p,const Nba97VoicePatlMemory*,const Nba97SpuEventsEvent* e,Nba97SpuTransferValue* v) {
        auto& f=*static_cast<Fixture*>(p);++f.deviceCalls;
        // Declared fixture operations. These do not claim timer behavior,
        // I_STAT hardware acknowledgement or a physical IRQ producer.
        if (e->address==0x1f801114&&e->width==4&&e->kind==NBA97_SPU_EVENTS_DEVICE_WRITE) {
            if (f.refuseTimer) return 0;f.timer=e->value;return 1;
        }
        if (e->address==0x1f801070&&e->width==2) {
            if (f.refuseStatus) return 0;
            if (e->kind==NBA97_SPU_EVENTS_DEVICE_WRITE) f.status=0;
            else *v={f.status,1};return 1;
        }
        if (e->address==SpuEventBackend::IrqMask&&e->width==2) {
            if (e->kind==NBA97_SPU_EVENTS_DEVICE_WRITE) f.externalMask=e->value;
            else *v={f.externalMask,1};return 1;
        }
        return 0;
    }
    static int policy(void* p,const Nba97VoicePatlMemory*,const Nba97InterruptEvent* e,Nba97SpuTransferValue* v) {
        auto& f=*static_cast<Fixture*>(p);++f.policyCalls;
        if (e->kind==NBA97_INTERRUPT_CHANGE_CLEAR_COUNTER&&e->argument[0]<4&&e->argument[1]<=1) {
            if (f.refuseCounter) return 0;f.counter[e->argument[0]]=e->argument[1];*v={0x87654321,0};return 1;
        }
        if (e->kind==NBA97_INTERRUPT_CALLBACK) {
            ++f.callbackCalls;*v={0x55667788,static_cast<std::uint8_t>(f.badReturn?2:0)};return f.callbackResult;
        }
        return 0;
    }
    int run(Nba97InterruptOperation op,std::uint32_t a=0,std::uint32_t b=0,std::size_t budget=5000,std::size_t capacity=512) {
        return backend.run(binding,memory,op,a,b,{},journal.data(),capacity,progress,budget);
    }
    void init() { check(run(NBA97_INTERRUPT_INITIALIZE_7F708)==1);check(progress.completed&&backend.hookInstalled()); }
    int enter(std::size_t budget=5000) { return backend.enterException(binding,memory,journal.data(),journal.size(),progress,budget); }
    int invoke(Nba97InterruptEvent e,Nba97SpuTransferValue& v) { return InterruptControllerBackend::io(&binding,&memory,&e,&v); }
};
void startupAndShutdown() {
    Fixture f;check(!f.events.criticalEnabled().known);check(f.get(0x800c7db0)==0);f.init();
    check(f.get(0x800c7db0)==0x8007fdb8&&f.get(0x800c7dc0)==0x8007ffe8);
    check(f.get(0x800c7dcc)==0x8007ff9c&&f.get(0x800c7dd8)==0x8007fc54);
    check(f.get(0x800c7df8)==9&&f.get(0x800c7e70)==17);
    check(f.timer==0x107&&f.counter[3]==0&&f.backend.padClearPolicy().word==0&&f.backend.iso9660Driver().word==0);
    check(f.events.criticalEnabled().known&&f.events.criticalEnabled().word==1);
    check(f.get(Context)==InterruptControllerBackend::ContinuationPc&&f.get(Context+4)==InterruptControllerBackend::ExceptionSp);
    check(f.get(Context+12)==InterruptControllerBackend::SourceS0);
    for (unsigned i=0;i<48;++i) check(f.known[Context-Base+i]==((i<8||(i>=12&&i<16))?1:0));
    check(f.backend.contextCaptured());
    // Existing guard must skip all BIOS work, not recapture saved registers.
    check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==1&&f.progress.events==0);
    check(f.run(NBA97_INTERRUPT_SHUTDOWN_7FAE4)==1);check(f.backend.hookInstalled());
    for (unsigned i=0;i<48;++i) check(f.known[Context-Base+i]==1&&f.bytes[Context-Base+i]==0);
    check(f.enter()==NBA97_PATL_IO_REFUSED&&f.progress.events==0);
    check(f.backend.exceptionPhase()==InterruptExceptionPhase::Idle);
    // Actual source shutdown retains the hook and VBlank counter, then fresh
    // startup performs a real new capture before reinstalling that same hook.
    check(f.get(0x800c7e70)==17);f.init();check(f.enter()==NBA97_INTERRUPT_TRANSFERRED);
    check(f.progress.transferred&&f.backend.exceptionPhase()==InterruptExceptionPhase::Returned);
}
void exceptionFlow() {
    Fixture f;f.init();const auto dispatch=f.get(0x800c7db0);
    check(f.enter()==2&&f.progress.transferred);check(f.get(0x800c7dca,2)==0);
    check(f.get(0x800c7db0)==dispatch&&f.events.criticalEnabled().word==1);
    check(f.journal[f.progress.events-1].kind==NBA97_INTERRUPT_RETURN_EXCEPTION);
    for (std::size_t i=0;i<f.progress.events;++i) check(f.journal[i].kind!=NBA97_INTERRUPT_CAPTURE_CONTEXT);
    // VBlank call is reached via the real IRQ dispatcher and increments only
    // once; the external fixture provides the declared pending/ack transition.
    f.status=1;check(f.enter()==2);check(f.get(0x800c7e70)==18);
    f.put(0x800c7e6c,0x80012340);f.status=1;check(f.enter()==2&&f.callbackCalls==1);
    check(f.events.criticalEnabled().word==1);
    Fixture fail;fail.init();fail.refuseStatus=true;check(fail.enter()==NBA97_PATL_IO_REFUSED);
    check(fail.backend.exceptionPhase()==InterruptExceptionPhase::Refused&&fail.events.criticalEnabled().word==0);
    fail.refuseStatus=false;check(fail.enter()==NBA97_PATL_IO_REFUSED&&fail.progress.events==0);
    Fixture transfer;transfer.init();transfer.put(0x800c7e6c,0x80012340);transfer.status=1;transfer.callbackResult=2;
    check(transfer.enter()==2&&transfer.backend.exceptionPhase()==InterruptExceptionPhase::ExternalTransfer);
    check(transfer.events.criticalEnabled().word==0);check(transfer.enter()==NBA97_PATL_IO_REFUSED);
    Fixture bad;bad.init();bad.put(0x800c7e6c,0x80012340);bad.status=1;bad.badReturn=true;
    check(bad.enter()==NBA97_PATL_METADATA);const auto& e=bad.journal[bad.progress.events-1];
    check(e.completed&&e.returned.known==2&&bad.backend.exceptionPhase()==InterruptExceptionPhase::Refused);
}
void capturesAndOwnership() {
    Fixture f;std::array<Nba97SpuTransferValue,12> r{};
    for (unsigned i=0;i<12;++i) r[i]={0x99887700+i,1};r[0]={};r[3]={};
    good(f.backend.importCaptureRegisters(r));f.init();
    for (unsigned i=0;i<12;++i) if(i!=0&&i!=1&&i!=3) check(f.get(Context+4*i)==r[i].word);
    auto invalid=r;invalid[11].known=2;check(!f.backend.importCaptureRegisters(invalid));
    invalid=r;invalid[0]={99,1};check(!f.backend.importCaptureRegisters(invalid));
    Fixture copy=f;copy.bind();check(copy.enter()==2);copy.put(Context+8,123);
    check(copy.enter()==NBA97_PATL_IO_REFUSED);check(f.enter()==2);
    copy=f;copy.bind();copy.binding.memoryGeneration++;
    check(copy.enter()==NBA97_PATL_IO_REFUSED&&copy.backend.lastResult().status==SpuSampleStatus::StaleGeneration);
    copy=f;copy.bind();copy.binding.events.sampleGeneration++;
    check(copy.enter()==NBA97_PATL_IO_REFUSED&&copy.backend.lastResult().status==SpuSampleStatus::StaleGeneration);
    check(copy.events.criticalEnabled().word==1&&copy.backend.exceptionPhase()==InterruptExceptionPhase::Returned);
    Fixture unknown;unknown.init();unknown.known[Context+4-Base]=0;check(unknown.enter()==NBA97_PATL_IO_REFUSED);
    unknown.known[Context+5-Base]=2;check(unknown.enter()==NBA97_PATL_IO_REFUSED&&unknown.backend.lastResult().status==SpuSampleStatus::Metadata);
    Fixture poisoned;poisoned.init();poisoned.known[Context+47-Base]=2;
    check(poisoned.enter()==NBA97_PATL_IO_REFUSED&&poisoned.backend.lastResult().status==SpuSampleStatus::Metadata);
    Fixture opaque;opaque.init();opaque.bytes[Context+47-Base]^=0xff;check(opaque.enter()==2);
    // Aliasing source addresses must address the same retained bytes, but
    // overlapping encoded ranges cannot select a guessed registry winner.
    Fixture alias;alias.spans[1]={alias.bytes.data(),alias.known.data(),alias.bytes.size(),0x000c7500,1,1,0};alias.memory.count=2;alias.init();
    alias.put(0x000c7dfc,0);check(alias.enter()==NBA97_PATL_IO_REFUSED);
    Fixture overlap;overlap.spans[1]=overlap.spans[0];overlap.memory.count=2;
    check(overlap.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_RESOURCE);
    for (bool dataAlias:std::array<bool,2>{{false,true}}) {
        Fixture words;words.init();
        // Three nonoverlapping source ranges cover the same registry as before,
        // but the final context word aliases an earlier payload OR mask cell.
        const auto at=Context+44-Base;
        words.spans[0].size=at;
        words.spans[1]={words.bytes.data()+at,words.known.data()+at,4,Context+44,1,1,0};
        if(dataAlias) words.spans[1].data=words.bytes.data()+Context-Base+40;
        else words.spans[1].known=words.known.data()+Context-Base+40;
        words.spans[2]={words.bytes.data()+at+4,words.known.data()+at+4,words.bytes.size()-at-4,Context+48,1,1,0};
        words.memory.count=3;
        check(words.enter()==NBA97_PATL_IO_REFUSED&&words.backend.lastResult().status==SpuSampleStatus::Ambiguous);
        check(words.events.criticalEnabled().word==1);
        // Fresh startup reaches its preceding clears, then refuses capture.
        words.put(0x800c7dc8,0,2);
        check(words.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_IO_REFUSED);
        check(words.backend.lastResult().status==SpuSampleStatus::Ambiguous&&words.get(Context)==0);
    }
}
void requiredEffectsAndPrefix() {
    Fixture f;f.refuseTimer=true;check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_IO_REFUSED);
    check(f.backend.contextCaptured()&&!f.backend.hookInstalled());check(f.get(0x800c7dc8,2)==1);
    check(f.events.criticalEnabled().word==0&&f.get(0x800c7db0)==0);
    // Original early ready store survives failure; retry skips initialization.
    f.refuseTimer=false;check(f.run(NBA97_INTERRUPT_INITIALIZE_7F708)==1&&!f.backend.hookInstalled());
    Fixture counter;counter.refuseCounter=true;check(counter.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_IO_REFUSED);
    check(counter.backend.padClearPolicy().word==0&&counter.timer==0x107);
    Fixture noPolicy;noPolicy.backend=InterruptControllerBackend{};check(noPolicy.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_IO_REFUSED);
    check(noPolicy.timer==0x107&&!noPolicy.backend.padClearPolicy().known);
    Fixture allKnown;for(auto& k:allKnown.known) k=1;allKnown.spans[0].fully_known=1;
    check(allKnown.run(NBA97_INTERRUPT_INITIALIZE_7F708)==NBA97_PATL_IO_REFUSED);
    check(!allKnown.backend.contextCaptured()&&allKnown.backend.lastResult().status==SpuSampleStatus::ReadOnly);
    check(allKnown.get(0x800c7dc8,2)==0&&allKnown.events.criticalEnabled().word==0);
    for (std::size_t cap=0;cap<80;++cap) {
        Fixture limited;const int rc=limited.run(NBA97_INTERRUPT_INITIALIZE_7F708,0,0,5000,cap);
        check(rc==1||rc==NBA97_INTERRUPT_LIMIT);check(limited.progress.events<=cap);
        if(rc==1) check(limited.backend.hookInstalled());
    }
    Fixture direct;Nba97InterruptEvent e{};Nba97SpuTransferValue v{};
    e.kind=NBA97_INTERRUPT_CAPTURE_CONTEXT;e.argument[0]=Context;
    check(direct.invoke(e,v)==0&&!direct.backend.contextCaptured());
    e.kind=NBA97_INTERRUPT_RETURN_EXCEPTION;check(direct.invoke(e,v)==0);
}
void sharedDeviceRouting() {
    Fixture f;good(f.events.importRegister(SpuEventBackend::IrqMask,2,{}));
    Nba97InterruptEvent e{};Nba97SpuTransferValue v{};e.kind=NBA97_INTERRUPT_DEVICE_READ;e.address=SpuEventBackend::IrqMask;e.width=2;
    check(f.invoke(e,v)==1&&v.known&&v.word==0);e.kind=NBA97_INTERRUPT_DEVICE_WRITE;e.value=0x4567;
    check(f.invoke(e,v)==1&&f.externalMask==0x4567);e.kind=NBA97_INTERRUPT_DEVICE_READ;
    check(f.invoke(e,v)==1&&v.word==0x4567&&f.deviceCalls==3&&f.policyCalls==0);
    good(f.samples.importRegister(SpuSampleBackend::BusDelay,4,{0xabcdef01,1}));
    e.address=SpuSampleBackend::BusDelay;e.width=2;check(f.invoke(e,v)==0&&f.deviceCalls==3);
    e.width=4;check(f.invoke(e,v)==1&&v.word==0xabcdef01&&f.deviceCalls==3);
}
void structuralArguments() {
    for(unsigned which=0;which<3;++which) {
        Fixture f;f.progress.accesses=777;
        auto m=f.memory;auto* j=f.journal.data();auto op=NBA97_INTERRUPT_INITIALIZE_7F708;
        if(which==0) j=nullptr;
        if(which==1) m.spans=nullptr;
        if(which==2) op=static_cast<Nba97InterruptOperation>(23);
        check(f.backend.run(f.binding,m,op,0,0,{},j,1,f.progress,5000)==NBA97_PATL_ARGUMENT);
        check(f.progress.accesses==777&&!f.backend.contextCaptured()&&!f.events.criticalEnabled().known);
        ++f.binding.memoryGeneration;f.init(); // invalid request did not bind91
    }
    for(unsigned which=0;which<2;++which) {
        Fixture f;f.init();auto m=f.memory;auto* j=f.journal.data();f.progress.accesses=777;
        if(which==0) j=nullptr;else m.spans=nullptr;
        check(f.backend.enterException(f.binding,m,j,1,f.progress,5000)==NBA97_PATL_ARGUMENT);
        check(f.progress.accesses==777&&f.events.criticalEnabled().word==1&&f.backend.exceptionPhase()==InterruptExceptionPhase::Idle);
        check(f.enter()==2);
    }
    for(bool budgetZero:std::array<bool,2>{{false,true}}) {
        Fixture f;f.init();
        check(f.backend.enterException(f.binding,f.memory,budgetZero?f.journal.data():nullptr,
            budgetZero?f.journal.size():0,f.progress,budgetZero?0:5000)==NBA97_INTERRUPT_LIMIT);
        check(f.events.criticalEnabled().word==0&&f.backend.exceptionPhase()==InterruptExceptionPhase::Refused);
    }
}
void actualEventBootstrap() {
    Fixture f;f.init();
    std::array<Nba97SpuEventsEvent,100> journal{};Nba97SpuEventsProgress p{};
    Nba97SpuEvents owner{f.memory,SpuEventBackend::io,&f.binding.events,5000};
    check(nba97_spu_events(&owner,NBA97_SPU_EVENTS_INITIALIZE_7E4C4,0,0,0,0,journal.data(),journal.size(),&p)==1);
    const auto h=f.get(0x800c7678);check(h&&h!=0xabcdef01&&f.get(0x800c7e4c)==0x8007d668);
    std::uint32_t occurred=123;good(f.samples.testEvent(h,occurred));check(occurred==0);
    // No copy, start, initialization or controller registration invents delivery.
    good(f.samples.deliverEvent(0xf0000009,0x20));good(f.samples.testEvent(h,occurred));check(occurred==1);
    good(f.samples.testEvent(h,occurred));check(occurred==0);
    check(nba97_spu_events(&owner,NBA97_SPU_EVENTS_SHUTDOWN_7E81C,0,0,0,0,journal.data(),journal.size(),&p)==1);
    check(f.events.closedHandle(h)&&f.get(0x800c7678)==h);
    check(f.run(NBA97_INTERRUPT_SHUTDOWN_7FAE4)==1&&f.backend.hookInstalled());
}
}
int main() {
    startupAndShutdown();exceptionFlow();capturesAndOwnership();requiredEffectsAndPrefix();sharedDeviceRouting();structuralArguments();actualEventBootstrap();
    std::printf("interrupt controller backend: %u checks passed\n",checks);
}
