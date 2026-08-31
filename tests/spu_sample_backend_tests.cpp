#include "spu_sample_backend.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <utility>

using namespace nba97;
namespace {
unsigned checks;
void check(bool p) { ++checks;if (!p) { std::fprintf(stderr,"SPU sample check %u failed\n",checks);std::exit(1); } }
void good(SpuSampleResult r) { check(r.status==SpuSampleStatus::Complete); }
constexpr std::uint32_t Cpu=0x80011000,Globals=0x800c7500;
constexpr std::uint64_t Generation=17;
struct Fixture {
    SpuSampleBackend backend;
    std::vector<std::uint8_t> bytes,known,globals,globalKnown;
    std::array<Nba97VoicePatlSpan,3> spans{};
    Nba97VoicePatlMemory memory{};
    SpuSampleIoContext binding{};
    std::array<Nba97SpuTransferEvent,64> journal{};
    Nba97SpuTransferProgress progress{};
    Fixture():bytes(256),known(256,1),globals(0x300,0xcd),globalKnown(0x300,0) {
        for (std::size_t i=0;i<bytes.size();++i) bytes[i]=static_cast<std::uint8_t>(i*29+7);
        rebind();
        for (auto p:std::array<std::pair<std::uint32_t,std::uint32_t>,9>{{
            {0x800c75c8,SpuSampleBackend::TransferAddress-0x1a6},
            {0x800c75cc,SpuSampleBackend::Madr},{0x800c75d0,SpuSampleBackend::Bcr},
            {0x800c75d4,SpuSampleBackend::Chcr},{0x800c75dc,SpuSampleBackend::BusDelay},
            {0x800c75e0,0},{0x800c75ec,3},{0x800c75fc,0},{0x800c7614,0}}}) put(p.first,p.second);
        put(0x800c75c4,0x200,2);
        good(backend.importRegister(SpuSampleBackend::Control,2,{0,1}));
        good(backend.importRegister(SpuSampleBackend::TransferControl,2,{4,1}));
        good(backend.importRegister(SpuSampleBackend::Dpcr,4,{0x80000,1}));
        good(backend.importRegister(SpuSampleBackend::BusDelay,4,{0,1}));
    }
    void rebind() {
        spans[0]={bytes.data(),known.data(),bytes.size(),Cpu,1,1,0};
        spans[1]={globals.data(),globalKnown.data(),globals.size(),Globals,1,1,0};
        spans[2]={};memory={spans.data(),2};binding={&backend,Generation,nullptr,nullptr};
    }
    void put(std::uint32_t a,std::uint32_t v,std::uint32_t width=4) { check(nba97_voice_patl_write(&memory,a,width,v)==1); }
    int run(Nba97SpuTransferOperation op,std::uint32_t a=0,std::uint32_t b=0,std::uint32_t c=0) {
        Nba97SpuTransfer owner{memory,SpuSampleBackend::io,&binding,10000};
        return nba97_spu_transfer(&owner,op,a,b,c,journal.data(),journal.size(),&progress);
    }
    std::uint32_t open() { std::uint32_t h=0;good(backend.openEvent(0xf0000009,0x20,0x2000,0,h));good(backend.enableEvent(h));return h; }
    std::uint32_t test(std::uint32_t h) { check(run(NBA97_SPU_TEST_EVENT_7F568,h)==1);check(progress.returned.known==1);return progress.returned.word; }
    void start(std::uint32_t size=65) { check(run(NBA97_SPU_TRANSFER_7DC90,Cpu,size)==1);check(progress.returned.known==1&&progress.returned.word==size); }
    void isr(bool complete=true) {
        auto t=backend.request().ticket;good(backend.beginIsr(t));int r=run(NBA97_SPU_ISR_7D668);
        check((r==1)==complete);auto done=backend.finishIsr(t,r==1);
        check(done.status==(complete?SpuSampleStatus::Complete:SpuSampleStatus::CallbackRefused));
    }
};
void ordinary() {
    Fixture f;auto h=f.open();f.start();check(f.backend.request().bytes==128);check(f.backend.request().phase==SpuDmaPhase::Requested);
    check(f.test(h)==0);check(f.backend.known()[0x1000]==0);
    f.bytes[15]=0x81;good(f.backend.servicePendingDma(f.memory,Generation));check(f.test(h)==0);
    check(f.backend.request().phase==SpuDmaPhase::CopiedAwaitingIsr);
    for (std::size_t i=0;i<SpuSampleBackend::SampleBytes;++i) {
        const bool written=i>=0x1000&&i<0x1080;check(f.backend.known()[i]==(written?1:0));
        if (written) check(f.backend.samples()[i]==f.bytes[i-0x1000]);
    }
    check(f.backend.servicePendingDma(f.memory,Generation).status==SpuSampleStatus::NoRequest);
    Nba97SpuTransferValue v{};
    check(f.backend.readDevice(f.memory,SpuSampleBackend::Madr,4,v).status==SpuSampleStatus::Unknown);
    f.isr();check(f.test(h)==1);check(f.test(h)==0);
    good(f.backend.readDevice(f.memory,SpuSampleBackend::Control,2,v));check((v.word&0x30)==0);
    check(f.backend.beginIsr(f.backend.request().ticket).status==SpuSampleStatus::InvalidTicket);
}
void paddingAndRefusals() {
    for (auto size:std::array<std::uint32_t,5>{{1,63,64,65,0x400001}}) {
        Fixture f;f.start(size);const auto n=size==65?128u:64u;check(f.backend.request().bytes==n);
        f.known[n-1]=0;check(f.backend.servicePendingDma(f.memory,Generation).status==SpuSampleStatus::Unknown);
        check(f.backend.known()[0x1000]==0);f.known[n-1]=2;f.known[0]=0;
        check(f.backend.servicePendingDma(f.memory,Generation).status==SpuSampleStatus::Metadata);
        f.known[0]=1;f.known[n-1]=1;good(f.backend.servicePendingDma(f.memory,Generation));
    }
    for (auto size:std::array<std::uint32_t,3>{{0,0x400000,0xffffffff}}) {
        Fixture f;check(f.run(NBA97_SPU_TRANSFER_7DC90,Cpu,size)==NBA97_PATL_IO_REFUSED);
        check(f.backend.lastResult().status==SpuSampleStatus::UnsupportedTransfer);
        check(f.backend.request().phase==SpuDmaPhase::Idle);
        Nba97SpuTransferValue v{};good(f.backend.readDevice(f.memory,SpuSampleBackend::Chcr,4,v));check(v.word==0x01000201);
    }
    for (unsigned offset=1;offset<=3;++offset) {
        Fixture f;check(f.run(NBA97_SPU_TRANSFER_7DC90,Cpu+offset,64)==NBA97_PATL_IO_REFUSED);
        check(f.backend.lastResult().status==SpuSampleStatus::UnsupportedDmaAlignment);
    }
    {
        Fixture f;f.start(65);f.spans[0].size=65;
        check(f.backend.servicePendingDma(f.memory,Generation).status==SpuSampleStatus::Unowned);
        check(f.backend.known()[0x1000]==0);
    }
    {
        Fixture f;f.start();check(f.backend.servicePendingDma(f.memory,Generation+1).status==SpuSampleStatus::StaleGeneration);
        check(f.backend.beginIsr(f.backend.request().ticket).status==SpuSampleStatus::InvalidTicket);
        check(f.backend.writeDevice(f.memory,SpuSampleBackend::TransferAddress,2,0,Generation).status==SpuSampleStatus::Busy);
    }
    {
        Fixture f;f.put(0x800c75c4,0xfff8,2);f.start(64);good(f.backend.servicePendingDma(f.memory,Generation));
        check(f.backend.known().back()==1);
    }
    {
        Fixture f;f.put(0x800c75c4,0xfff9,2);check(f.run(NBA97_SPU_TRANSFER_7DC90,Cpu,64)==NBA97_PATL_IO_REFUSED);
        check(f.backend.lastResult().status==SpuSampleStatus::UnsupportedTransfer);check(f.backend.known()[0]==0);
    }
    {
        Fixture f;good(f.backend.importRegister(SpuSampleBackend::TransferControl,2,{}));
        check(f.run(NBA97_SPU_TRANSFER_7DC90,Cpu,64)==NBA97_PATL_IO_REFUSED);check(f.backend.lastResult().status==SpuSampleStatus::Unknown);
    }
    {
        Fixture f;good(f.backend.importRegister(SpuSampleBackend::Dpcr,4,{0,1}));
        check(f.run(NBA97_SPU_TRANSFER_7DC90,Cpu,64)==NBA97_PATL_IO_REFUSED);check(f.backend.lastResult().status==SpuSampleStatus::UnsupportedTransfer);
    }
}
void clonesAndAliases() {
    Fixture f;auto h=f.open();f.start();
    auto clone=f.backend;auto cloneMemory=f.bytes;auto cloneKnown=f.known;
    Nba97VoicePatlSpan span{cloneMemory.data(),cloneKnown.data(),cloneMemory.size(),Cpu,1,1,0};Nba97VoicePatlMemory m{&span,1};
    cloneMemory[0]=13;good(clone.servicePendingDma(m,Generation));check(clone.samples()[0x1000]==13);check(f.backend.known()[0x1000]==0);
    good(clone.deliverEvent(0xf0000009,0x20));std::uint32_t occurrence=0;good(clone.testEvent(h,occurrence));check(occurrence==1);check(f.test(h)==0);
    SpuSampleBackend assigned;assigned=clone;SpuSampleBackend moved=std::move(assigned);check(moved.samples()[0x1000]==13);check(moved.request().phase==SpuDmaPhase::CopiedAwaitingIsr);
    good(f.backend.servicePendingDma(f.memory,Generation));check(f.backend.samples()[0x1000]==f.bytes[0]);
    {
        Fixture a;a.spans[2]=a.spans[0];a.spans[2].source_address=0xa0011000;a.memory.count=3;
        check(a.run(NBA97_SPU_TRANSFER_7DC90,0xa0011000,64)==1);good(a.backend.servicePendingDma(a.memory,Generation));check(a.backend.samples()[0x1000]==a.bytes[0]);
    }
    {
        Fixture a;a.put(0x800c75d4,Cpu);a.start(64);check(a.backend.request().phase==SpuDmaPhase::Idle);
        check(a.bytes[0]==1&&a.bytes[1]==2&&a.bytes[2]==0&&a.bytes[3]==1);
    }
    {
        Fixture a;a.spans[2]=a.spans[0];a.spans[2].source_address=SpuSampleBackend::Madr;a.memory.count=3;
        check(a.backend.writeDevice(a.memory,SpuSampleBackend::Madr,4,Cpu,Generation).status==SpuSampleStatus::Ambiguous);
    }
    {
        Fixture a;a.start(64);a.spans[2]=a.spans[0];a.memory.count=3;
        check(a.backend.servicePendingDma(a.memory,Generation).status==SpuSampleStatus::Ambiguous);
    }
    {
        Fixture a;a.start(64);a.known[0]=0;a.known[63]=2;
        check(a.backend.servicePendingDma(a.memory,Generation).status==SpuSampleStatus::Metadata);
        a.known[63]=1;a.spans[0].fully_known=1;
        check(a.backend.servicePendingDma(a.memory,Generation).status==SpuSampleStatus::Metadata);
    }
    {
        SpuSampleBackend b;std::array<std::uint8_t,5> values{{1,2,3,4,5}},mask{{1,1,1,1,1}};
        good(b.importSamples(0,values.data(),mask.data(),5));
        good(b.importSamples(1,b.samples().data(),b.known().data(),4));
        check(b.samples()[0]==1&&b.samples()[1]==1&&b.samples()[2]==2&&b.samples()[3]==3&&b.samples()[4]==4);
    }
    {
        Fixture a;a.start(64);
        a.spans[0].data=const_cast<std::uint8_t*>(a.backend.samples().data());
        check(a.backend.servicePendingDma(a.memory,Generation).status==SpuSampleStatus::Ambiguous);
    }
    {
        Fixture a;a.spans[0].size=2;a.spans[2]=a.spans[0];a.spans[2].data+=2;a.spans[2].known+=2;
        a.spans[2].source_address+=2;a.spans[2].size=254;a.memory.count=3;
        Nba97SpuTransferValue v{};
        check(a.backend.readDevice(a.memory,Cpu,4,v).status==SpuSampleStatus::Unowned);
        check(a.backend.writeDevice(a.memory,Cpu,4,0,Generation).status==SpuSampleStatus::Unowned);
        a.start(64);check(a.backend.servicePendingDma(a.memory,Generation).status==SpuSampleStatus::Unowned);
    }
}
int callback(void* p,const Nba97VoicePatlMemory*,const Nba97SpuTransferEvent* e,Nba97SpuTransferValue* out) {
    auto* count=static_cast<unsigned*>(p);++*count;
    check(e->kind==NBA97_SPU_TRANSFER_CALLBACK_7D668&&e->address==0x80012340&&e->argument[0]==0xf0000000);
    *out={};return 1;
}
int callbackValue(void* p,const Nba97VoicePatlMemory*,const Nba97SpuTransferEvent*,Nba97SpuTransferValue* out) {
    *out=*static_cast<Nba97SpuTransferValue*>(p);return 1;
}
void eventsAndIsr() {
    {
        Fixture f;auto h=f.open();good(f.backend.deliverEvent(0xf0000009,0x20));f.start(64);
        // Original stale event: a new start cannot clear a previous completion.
        check(f.test(h)==1);check(f.backend.request().phase==SpuDmaPhase::Requested);check(f.test(h)==0);
    }
    {
        Fixture f;auto h=f.open();f.put(0x800c75fc,0x80012340);f.start(64);good(f.backend.servicePendingDma(f.memory,Generation));
        unsigned called=0;f.binding.external=callback;f.binding.externalUser=&called;f.isr();check(called==1);check(f.test(h)==0);
    }
    {
        Fixture f;auto h=f.open();f.put(0x800c75fc,0x80012340);f.start(64);good(f.backend.servicePendingDma(f.memory,Generation));f.isr(false);
        check(f.backend.request().phase==SpuDmaPhase::IsrRefused);check(f.backend.known()[0x1000]==1);check(f.test(h)==0);
        check(f.backend.beginIsr(f.backend.request().ticket).status==SpuSampleStatus::InvalidTicket);
    }
    {
        SpuSampleBackend b;std::uint32_t h=0,h2=0,n=3;good(b.openEvent(1,2,0x2000,0,h));good(b.deliverEvent(1,2));good(b.testEvent(h,n));check(n==0);
        good(b.enableEvent(h));good(b.openEvent(1,2,0x2000,0,h2));good(b.enableEvent(h2));check(h!=h2);
        good(b.deliverEvent(1,3));good(b.testEvent(h,n));check(n==0);
        good(b.deliverEvent(1,2));good(b.deliverEvent(1,2));good(b.testEvent(h,n));check(n==1);good(b.testEvent(h,n));check(n==0);
        good(b.testEvent(h2,n));check(n==1);good(b.deliverEvent(1,2));good(b.disableEvent(h));good(b.testEvent(h,n));check(n==0);
        good(b.enableEvent(h));good(b.testEvent(h,n));check(n==0);good(b.closeEvent(h));check(b.testEvent(h,n).status==SpuSampleStatus::InvalidEvent);
        std::uint32_t h3=0;good(b.openEvent(1,2,0x2000,0,h3));check(h3!=h);check(b.openEvent(1,2,0x1000,0,h3).status==SpuSampleStatus::UnsupportedTransfer);
    }
    for (auto marker:std::array<std::uint8_t,2>{{0,2}}) {
        Fixture f;f.put(0x800c75fc,0x80012340);f.start(64);good(f.backend.servicePendingDma(f.memory,Generation));
        Nba97SpuTransferValue returned{0xdeadbeef,marker};f.binding.external=callbackValue;f.binding.externalUser=&returned;
        auto t=f.backend.request().ticket;good(f.backend.beginIsr(t));int rc=f.run(NBA97_SPU_ISR_7D668);
        check(rc==(marker==0?1:NBA97_PATL_METADATA));const auto& e=f.journal[f.progress.events-1];
        check(e.kind==NBA97_SPU_TRANSFER_CALLBACK_7D668&&e.completed==1&&e.returned.word==0xdeadbeef&&e.returned.known==marker);
        auto done=f.backend.finishIsr(t,rc==1);check(done.status==(marker==0?SpuSampleStatus::Complete:SpuSampleStatus::CallbackRefused));
    }
}
void reverseAndPio() {
    Fixture f;f.start(64);good(f.backend.servicePendingDma(f.memory,Generation));f.isr();
    auto original=f.bytes;std::fill(f.bytes.begin(),f.bytes.end(),std::uint8_t(0xcd));std::fill(f.known.begin(),f.known.end(),std::uint8_t(0));
    check(f.run(NBA97_SPU_CONTROL_7D9E8,2,0x1000)==1);check(f.run(NBA97_SPU_CONTROL_7D9E8,0)==1);
    check(f.run(NBA97_SPU_CONTROL_7D9E8,3,Cpu,64)==1);check(!f.backend.request().toSpu);
    f.spans[0].writable=0;check(f.backend.servicePendingDma(f.memory,Generation).status==SpuSampleStatus::ReadOnly);
    f.spans[0].writable=1;good(f.backend.servicePendingDma(f.memory,Generation));
    for (unsigned i=0;i<256;++i) { check(f.known[i]==(i<64?1:0));if (i<64) check(f.bytes[i]==original[i]); }
    f.isr();
    check(f.run(NBA97_SPU_CONTROL_7D9E8,2,0x2000)==1);check(f.run(NBA97_SPU_CONTROL_7D9E8,0)==1);
    check(f.run(NBA97_SPU_CONTROL_7D9E8,3,Cpu,64)==1);check(f.backend.servicePendingDma(f.memory,Generation).status==SpuSampleStatus::Unknown);
    Fixture p;check(p.backend.writeDevice(p.memory,SpuSampleBackend::Fifo,2,0,Generation).status==SpuSampleStatus::UnsupportedAddress);
    check(p.backend.writeDevice(p.memory,SpuSampleBackend::Control,2,0x10,Generation).status==SpuSampleStatus::UnsupportedTransfer);
    SpuSampleBackend empty;check(empty.importRegister(SpuSampleBackend::Control,2,{1,0}).status==SpuSampleStatus::Metadata);
    check(empty.importRegister(SpuSampleBackend::Control,2,{0,2}).status==SpuSampleStatus::Metadata);
}
}
int main() { ordinary();paddingAndRefusals();clonesAndAliases();eventsAndIsr();reverseAndPio();std::printf("spu sample backend: %u checks passed\n",checks); }
