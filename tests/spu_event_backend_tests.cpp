#include "spu_event_backend.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <utility>

using namespace nba97;
namespace {
unsigned checks;
void check(bool p) { ++checks;if (!p) { std::fprintf(stderr,"SPU event backend check %u failed\n",checks);std::exit(1); } }
void good(SpuSampleResult r) { check(r.status==SpuSampleStatus::Complete); }
constexpr std::uint32_t Base=0x800c7500,Cpu=0x80012000;
constexpr std::uint64_t Generation=9;
struct Fixture {
    SpuSampleBackend samples;SpuEventBackend backend;
    std::vector<std::uint8_t> globals,data,gk,dk;
    std::array<Nba97VoicePatlSpan,3> spans{};Nba97VoicePatlMemory memory{};
    SpuEventIoContext binding{};SpuSampleIoContext transferBinding{};
    std::array<Nba97SpuEventsEvent,64> journal{};Nba97SpuEventsProgress progress{};
    Fixture():globals(0xa00,0xcd),data(128),gk(0xa00,0),dk(128,1) {
        for (unsigned i=0;i<128;++i) data[i]=static_cast<std::uint8_t>(17*i+3);bind();
        for (auto p:std::array<std::pair<std::uint32_t,std::uint32_t>,17>{{
            {0x800c7a80,0},{0x800c7dc4,0x800c7dac},{0x800c7db0,0x8007fdb8},
            {0x800c7e30,SpuEventBackend::IrqMask},{0x800c7e5c,SpuEventBackend::Dicr},
            {0x800c75c8,0x1f801c00},{0x800c75cc,SpuSampleBackend::Madr},
            {0x800c75d0,SpuSampleBackend::Bcr},{0x800c75d4,SpuSampleBackend::Chcr},
            {0x800c75dc,SpuSampleBackend::BusDelay},{0x800c75e0,0},
            {0x800c75ec,3},{0x800c75fc,0},{0x800c7614,0},
            {0x800c7600,0x12345678},{0x800c7620,7},{0x800c7678,0xabcdef01}}}) put(p.first,p.second);
        put(0x800c7e38,1,2);put(0x800c75c4,0x200,2);
        for (unsigned i=0;i<7;++i) put(0x800c7e3c+4*i,0);
        good(backend.importRegister(SpuEventBackend::IrqMask,2,{0x345,1}));good(backend.importRegister(SpuEventBackend::Dicr,4,{0,1}));
        good(samples.importRegister(SpuSampleBackend::Control,2,{0,1}));
        good(samples.importRegister(SpuSampleBackend::TransferControl,2,{4,1}));
        good(samples.importRegister(SpuSampleBackend::Dpcr,4,{0x80000,1}));good(samples.importRegister(SpuSampleBackend::BusDelay,4,{0,1}));
    }
    void bind() {
        spans[0]={globals.data(),gk.data(),globals.size(),Base,1,1,0};spans[1]={data.data(),dk.data(),data.size(),Cpu,1,1,0};spans[2]={};
        memory={spans.data(),2};binding={&backend,&samples,Generation,nullptr,nullptr};transferBinding={&samples,Generation,nullptr,nullptr};
    }
    void put(std::uint32_t a,std::uint32_t v,std::uint32_t w=4) { check(nba97_voice_patl_write(&memory,a,w,v)==1); }
    std::uint32_t get(std::uint32_t a,std::uint32_t w=4) { std::uint32_t v=0;check(nba97_voice_patl_read(&memory,a,w,&v)==1);return v; }
    int run(Nba97SpuEventsOperation op,std::uint32_t a=0,std::uint32_t b=0,std::uint32_t c=0,std::uint32_t d=0) {
        Nba97SpuEvents owner{memory,SpuEventBackend::io,&binding,2000};return nba97_spu_events(&owner,op,a,b,c,d,journal.data(),journal.size(),&progress);
    }
    void init() { check(run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==1);check(progress.completed==1&&progress.returned.known==0); }
    Nba97SpuTransferValue reg(std::uint32_t a,std::uint32_t width) {
        Nba97SpuEventsEvent e{};e.kind=NBA97_SPU_EVENTS_DEVICE_READ;e.address=a;e.width=width;Nba97SpuTransferValue v{};
        check(SpuEventBackend::io(&binding,&memory,&e,&v)==1);return v;
    }
    void transfer() {
        std::array<Nba97SpuTransferEvent,64> events{};Nba97SpuTransferProgress p{};
        Nba97SpuTransfer owner{memory,SpuSampleBackend::io,&transferBinding,10000};
        check(nba97_spu_transfer(&owner,NBA97_SPU_TRANSFER_7DC90,Cpu,65,0,events.data(),events.size(),&p)==1);
        good(samples.servicePendingDma(memory,Generation));auto ticket=samples.request().ticket;good(samples.beginIsr(ticket));
        check(nba97_spu_transfer(&owner,NBA97_SPU_ISR_7D668,0,0,0,events.data(),events.size(),&p)==1);good(samples.finishIsr(ticket,true));
        check(nba97_spu_transfer(&owner,NBA97_SPU_TEST_EVENT_7F568,get(0x800c7678),0,0,events.data(),events.size(),&p)==1);check(p.returned.known&&p.returned.word==1);
        check(nba97_spu_transfer(&owner,NBA97_SPU_TEST_EVENT_7F568,get(0x800c7678),0,0,events.data(),events.size(),&p)==1);check(p.returned.known&&p.returned.word==0);
    }
};
void chain() {
    Fixture f;check(!f.backend.criticalEnabled().known);f.init();auto h=f.get(0x800c7678);check(h&&h!=0xabcdef01);
    check(f.get(0x800c7a80)==1&&f.get(0x800c7e4c)==0x8007d668);
    check(f.reg(SpuEventBackend::IrqMask,2).word==0x345);check(f.reg(SpuEventBackend::Dicr,4).word==0x00900000);
    check(f.backend.criticalEnabled().known&&f.backend.criticalEnabled().word==1);
    f.transfer();for (unsigned i=0;i<128;++i) check(f.samples.known()[0x1000+i]==1&&f.samples.samples()[0x1000+i]==f.data[i]);
    check(f.run(NBA97_SPU_EVENTS_SHUTDOWN_7E81C)==1);check(f.progress.returned.known==0);
    check(f.get(0x800c7678)==h&&f.backend.closedHandle(h));check(f.get(0x800c7a80)==0&&f.get(0x800c7e4c)==0);
    check(f.reg(SpuEventBackend::Dicr,4).word==0x00800000);check(f.reg(SpuEventBackend::IrqMask,2).word==0x345);
    unsigned closes=0,disables=0;std::size_t closeAt=0,disableAt=0;
    for (std::size_t i=0;i<f.progress.events;++i) {
        const auto& e=f.journal[i];if (e.kind==NBA97_SPU_EVENTS_CLOSE_EVENT) { ++closes;closeAt=i;check(e.returned.known&&e.returned.word==1&&e.completed); }
        if (e.kind==NBA97_SPU_EVENTS_DISABLE_EVENT) { ++disables;disableAt=i;check(!e.returned.known&&e.completed); }
    }
    check(closes==1&&disables==1&&closeAt<disableAt);
    std::uint32_t n=0;check(f.samples.testEvent(h,n).status==SpuSampleStatus::InvalidEvent);
    check(f.run(NBA97_SPU_EVENTS_SHUTDOWN_7E81C)==1&&f.progress.events==0);
}
void criticalAndClones() {
    for (Nba97SpuTransferValue incoming:std::array<Nba97SpuTransferValue,3>{{{0,1},{1,1},{0xdeadbeef,0}}}) {
        Fixture f;good(f.backend.importCritical(incoming));check(f.run(NBA97_SPU_EVENTS_ENTER_7F308)==1);
        check(f.progress.returned.known==incoming.known);if (incoming.known) check(f.progress.returned.word==incoming.word);
        check(f.backend.criticalEnabled().known&&f.backend.criticalEnabled().word==0);
        check(f.run(NBA97_SPU_EVENTS_ENTER_7F308)==1&&f.progress.returned.known&&f.progress.returned.word==0);
        check(f.run(NBA97_SPU_EVENTS_EXIT_7F578)==1&&!f.progress.returned.known);
        check(f.backend.criticalEnabled().word==1);check(f.reg(SpuEventBackend::IrqMask,2).word==0x345);
    }
    Fixture f;check(f.backend.importCritical({2,1}).status==SpuSampleStatus::Metadata);check(f.backend.importCritical({0,2}).status==SpuSampleStatus::Metadata);
    f.init();auto h=f.get(0x800c7678);Fixture copy=f;copy.bind();check(copy.run(NBA97_SPU_EVENTS_SHUTDOWN_7E81C)==1);
    check(copy.backend.closedHandle(h)&&!f.backend.closedHandle(h));std::uint32_t n=0;good(f.samples.testEvent(h,n));
    good(f.samples.deliverEvent(0xf0000009,0x20));good(f.samples.testEvent(h,n));check(n==1);
    check(copy.samples.testEvent(h,n).status==SpuSampleStatus::InvalidEvent);
    copy.binding.sampleGeneration=Generation+1;check(copy.run(NBA97_SPU_EVENTS_ENTER_7F308)==NBA97_PATL_IO_REFUSED);check(copy.backend.lastResult().status==SpuSampleStatus::StaleGeneration);
    SpuEventBackend assigned;assigned=f.backend;SpuEventBackend moved=std::move(assigned);check(!moved.closedHandle(h));
}
struct Mutation { SpuEventIoContext* binding;std::uint32_t replacement; };
int mutateClose(void* p,const Nba97VoicePatlMemory* m,const Nba97SpuEventsEvent* e,Nba97SpuTransferValue* out) {
    auto& c=*static_cast<Mutation*>(p);int rc=SpuEventBackend::io(c.binding,m,e,out);
    if (rc==1&&e->kind==NBA97_SPU_EVENTS_CLOSE_EVENT) check(nba97_voice_patl_write(m,0x800c7678,4,c.replacement)==1);
    return rc;
}
void liveHandleAndFailures() {
    for (bool valid:std::array<bool,2>{{true,false}}) {
        Fixture f;f.init();auto h=f.get(0x800c7678);std::uint32_t other=0x33445566;
        if (valid) { good(f.samples.openEvent(9,8,0x2000,0,other));good(f.samples.enableEvent(other));good(f.samples.deliverEvent(9,8)); }
        Mutation mutation{&f.binding,other};Nba97SpuEvents owner{f.memory,mutateClose,&mutation,2000};
        const auto rc=nba97_spu_events(&owner,NBA97_SPU_EVENTS_SHUTDOWN_7E81C,0,0,0,0,f.journal.data(),f.journal.size(),&f.progress);
        check(rc==(valid?1:NBA97_PATL_IO_REFUSED));check(f.backend.closedHandle(h));check(f.get(0x800c7678)==other);
        if (valid) { std::uint32_t n=0;good(f.samples.testEvent(other,n));check(n==0); }
        else { check(f.backend.criticalEnabled().word==0);check(f.progress.stopped_pc==0x8007f4fcu); }
    }
    for (auto marker:std::array<std::uint32_t,3>{{1,2,0xffffffff}}) {
        Fixture f;f.put(0x800c7a80,marker);check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==1);check(f.progress.events==0&&f.progress.returned.word==marker);
    }
    Fixture f;f.put(0x800c7db0,0);check(f.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==NBA97_PATL_IO_REFUSED);check(f.get(0x800c7a80)==1);
    check(f.get(0x800c7678)==0xabcdef01);check(f.backend.criticalEnabled().known&&f.backend.criticalEnabled().word==0);
}
struct External { unsigned calls=0;std::uint32_t mask=0x555;Nba97SpuTransferValue returned{0xdeadbeef,0};bool execute=true; };
int external(void* p,const Nba97VoicePatlMemory*,const Nba97SpuEventsEvent* e,Nba97SpuTransferValue* out) {
    auto& s=*static_cast<External*>(p);++s.calls;
    if (!s.execute) return 0;
    if (e->kind==NBA97_SPU_EVENTS_DEVICE_READ&&e->address==SpuEventBackend::IrqMask) { *out={s.mask,1};return 1; }
    if (e->kind==NBA97_SPU_EVENTS_DEVICE_WRITE&&e->address==SpuEventBackend::IrqMask) { s.mask=e->value&0xffffu;*out={};return 1; }
    *out=s.returned;return 1;
}
int partialDevice(void* p,const Nba97VoicePatlMemory* m,const Nba97SpuEventsEvent* e,Nba97SpuTransferValue* out) {
    auto& s=*static_cast<External*>(p);
    if (e->kind==NBA97_SPU_EVENTS_DEVICE_WRITE&&!s.execute) { ++s.calls;s.mask=e->value&0xffffu;return 0; }
    return external(p,m,e,out);
}
void deviceAndExternal() {
    Fixture f;f.put(Cpu,0x89ab7654);f.put(0x800c7e30,Cpu);
    check(f.run(NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,0x11223344)==1);check(f.progress.returned.word==0x7654&&f.get(Cpu)==0x89ab3344);
    f.dk[0]=0;f.dk[1]=2;check(f.run(NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,0)==NBA97_PATL_IO_REFUSED);check(f.backend.lastResult().status==SpuSampleStatus::Metadata);
    for (auto value:std::array<std::uint32_t,4>{{0x01000000,0x80000000,0x8000,1}}) {
        Fixture g;check(g.backend.importRegister(SpuEventBackend::Dicr,4,{value,1}).status==SpuSampleStatus::UnsupportedTransfer);
    }
    {
        Fixture g;good(g.backend.importRegister(SpuEventBackend::Dicr,4,{}));check(g.run(NBA97_SPU_EVENTS_INITIALIZE_7E4C4)==NBA97_PATL_IO_REFUSED);
        check(g.get(0x800c7e4c)==0x8007d668);check(g.reg(SpuEventBackend::IrqMask,2).word==0);
    }
    {
        Fixture g;External host;g.binding.external=external;g.binding.externalUser=&host;
        good(g.backend.importRegister(SpuEventBackend::IrqMask,2,{}));check(g.run(NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,0)==1);
        check(g.progress.returned.word==0x555&&host.mask==0&&host.calls==2);
        check(g.run(NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,0x555)==1);check(g.progress.returned.word==0&&host.mask==0x555&&host.calls==4);
    }
    for (auto marker:std::array<std::uint8_t,3>{{0,1,2}}) {
        Fixture g;g.put(0x800c7db0,0x80012340);External host;host.returned.known=marker;g.binding.external=external;g.binding.externalUser=&host;
        check(g.run(NBA97_SPU_EVENTS_DISPATCH_7F630,4,0x8007d668)==(marker==2?NBA97_PATL_METADATA:1));
        const auto& e=g.journal[g.progress.events-1];check(e.completed&&e.address==0x80012340&&e.returned.word==0xdeadbeef&&e.returned.known==marker);
        check(e.argument[0]==4&&e.argument[1]==0x8007d668&&host.calls==1);
    }
    {
        Fixture g;g.spans[2]=g.spans[1];g.spans[2].source_address=SpuEventBackend::IrqMask;g.memory.count=3;
        check(g.run(NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,0)==NBA97_PATL_IO_REFUSED);check(g.backend.lastResult().status==SpuSampleStatus::Ambiguous);
    }
    {
        Fixture g;External host;host.execute=false;g.binding.external=partialDevice;g.binding.externalUser=&host;
        Nba97SpuEventsEvent e{};e.kind=NBA97_SPU_EVENTS_DEVICE_WRITE;e.address=SpuEventBackend::IrqMask;e.width=4;e.value=0xdead0456;
        Nba97SpuTransferValue v{};check(SpuEventBackend::io(&g.binding,&g.memory,&e,&v)==0);check(host.mask==0x456);
        host.execute=true;check(g.run(NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,0)==1);check(g.progress.returned.word==0x456&&host.calls==3&&host.mask==0);
        good(g.backend.importRegister(SpuEventBackend::IrqMask,2,{0x123,1}));
        check(g.run(NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,0)==1);check(g.progress.returned.word==0x123&&host.calls==3);
    }
    {
        Fixture g;check(g.run(NBA97_SPU_EVENTS_SET_CALLBACK_7FDB8,8,0x80012340)==NBA97_PATL_IO_REFUSED);
        // Raw channel8 aliases the live DICR pointer slot; no invented source
        // bounds guard or rollback of the published callback is permitted.
        check(g.get(0x800c7e5c)==0x80012340);check(g.reg(SpuEventBackend::IrqMask,2).word==0);
    }
    {
        Fixture g;External host;g.binding.external=external;g.binding.externalUser=&host;
        g.put(0x800c7e30,SpuSampleBackend::BusDelay);
        check(g.run(NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,0x2222)==NBA97_PATL_IO_REFUSED);
        check(host.calls==0&&g.backend.lastResult().status==SpuSampleStatus::UnsupportedAddress);
        Nba97SpuTransferValue v{};good(g.samples.readDevice(g.memory,SpuSampleBackend::BusDelay,4,v));check(v.known&&v.word==0);
    }
}
}
int main() { chain();criticalAndClones();liveHandleAndFailures();deviceAndExternal();std::printf("spu event backend: %u checks passed\n",checks); }
