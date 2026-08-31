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
    Fixture p;check(p.backend.writeDevice(p.memory,SpuSampleBackend::Fifo,2,0,Generation).status==SpuSampleStatus::Unknown);
    check(p.backend.writeDevice(p.memory,SpuSampleBackend::Control,2,0x10,Generation).status==SpuSampleStatus::UnsupportedTransfer);
    SpuSampleBackend empty;check(empty.importRegister(SpuSampleBackend::Control,2,{1,0}).status==SpuSampleStatus::Metadata);
    check(empty.importRegister(SpuSampleBackend::Control,2,{0,2}).status==SpuSampleStatus::Metadata);
}
}
void pioRequestAndCopy() {
    for(unsigned words=1;words<=32;++words) {
        Fixture f;const auto h=f.open();good(f.backend.deliverEvent(0xf0000009,0x20));
        good(f.backend.writeDevice(f.memory,SpuSampleBackend::TransferAddress,2,0x200,Generation));
        for(unsigned i=0;i<words;++i) good(f.backend.writeDevice(f.memory,SpuSampleBackend::Fifo,2,0x1234+i,Generation));
        check(f.backend.pioRequest().phase==SpuPioPhase::Filling&&f.backend.pioRequest().bytes==2*words);
        check(f.backend.known()[0x1000]==0&&f.backend.request().phase==SpuDmaPhase::Idle);
        good(f.backend.writeDevice(f.memory,SpuSampleBackend::Control,2,0x10,Generation));
        check(f.backend.pioRequest().phase==SpuPioPhase::Requested&&f.backend.known()[0x1000]==0);
        check(f.backend.servicePendingPio(Generation+1).status==SpuSampleStatus::StaleGeneration);
        check(f.backend.writeDevice(f.memory,SpuSampleBackend::Fifo,2,0,Generation).status==SpuSampleStatus::Busy);
        Fixture copy=f;copy.rebind();good(copy.backend.servicePendingPio(Generation));
        check(f.backend.known()[0x1000]==0);good(f.backend.servicePendingPio(Generation));
        for(unsigned i=0;i<2*words;++i) check(f.backend.known()[0x1000+i]==1&&
            f.backend.samples()[0x1000+i]==static_cast<std::uint8_t>((0x1234+i/2)>>(8*(i%2))));
        check(f.backend.known()[0xfff]==0&&f.backend.known()[0x1000+2*words]==0);
        check(f.backend.servicePendingPio(Generation).status==SpuSampleStatus::NoRequest);
        check(f.test(h)==1&&f.test(h)==0); // old pending only; PIO never delivers
        Nba97SpuTransferValue v{};check(f.backend.readDevice(f.memory,SpuSampleBackend::Status,2,v).status==SpuSampleStatus::Unknown);
        good(f.backend.writeDevice(f.memory,SpuSampleBackend::Control,2,0,Generation));
        check(f.backend.writeDevice(f.memory,SpuSampleBackend::Fifo,2,0,Generation).status==SpuSampleStatus::UnsupportedTransfer);
        f.start(64);good(f.backend.servicePendingDma(f.memory,Generation));f.isr();check(f.test(h)==1);
    }
    Fixture edge;good(edge.backend.writeDevice(edge.memory,SpuSampleBackend::TransferAddress,2,0xffff,Generation));
    for(unsigned i=0;i<4;++i) good(edge.backend.writeDevice(edge.memory,SpuSampleBackend::Fifo,2,0xabcd,Generation));
    check(edge.backend.writeDevice(edge.memory,SpuSampleBackend::Fifo,2,0,Generation).status==SpuSampleStatus::UnsupportedTransfer);
    good(edge.backend.writeDevice(edge.memory,SpuSampleBackend::Control,2,0x10,Generation));good(edge.backend.servicePendingPio(Generation));
    check(edge.backend.known()[0x7ffff]==1&&edge.backend.known()[0]==0);
    Fixture type;good(type.backend.importRegister(SpuSampleBackend::TransferControl,2,{6,1}));
    good(type.backend.writeDevice(type.memory,SpuSampleBackend::TransferAddress,2,0x200,Generation));
    check(type.backend.writeDevice(type.memory,SpuSampleBackend::Fifo,2,0,Generation).status==SpuSampleStatus::UnsupportedTransfer);
    Fixture full;good(full.backend.writeDevice(full.memory,SpuSampleBackend::TransferAddress,2,0x200,Generation));
    for(unsigned i=0;i<32;++i) good(full.backend.writeDevice(full.memory,SpuSampleBackend::Fifo,2,i,Generation));
    check(full.backend.writeDevice(full.memory,SpuSampleBackend::Fifo,2,0,Generation).status==SpuSampleStatus::UnsupportedTransfer);
    check(full.backend.writeDevice(full.memory,SpuSampleBackend::Madr,4,Cpu,Generation).status==SpuSampleStatus::Busy);
    check(full.backend.importRegister(SpuSampleBackend::Control,2,{0,1}).status==SpuSampleStatus::Busy);
}
struct PioFixture {
    Fixture f;
    unsigned statusReads=0,services=0;
    bool service=true;
    PioFixture() { f.binding.external=io;f.binding.externalUser=this; }
    static int io(void* p,const Nba97VoicePatlMemory* m,const Nba97SpuTransferEvent* e,Nba97SpuTransferValue* out) {
        auto& self=*static_cast<PioFixture*>(p);
        if(e->kind==NBA97_SPU_TRANSFER_DEVICE_READ&&e->address==SpuSampleBackend::Status) {
            ++self.statusReads;
            if(self.f.backend.pioRequest().phase==SpuPioPhase::Requested) {
                if(!self.service) return 0;
                good(self.f.backend.servicePendingPio(Generation));++self.services;
            }
            // Explicit scripted observation for CPU composition, not a claim
            // that Control readback predicts physical SPUSTAT or busy timing.
            Nba97SpuTransferValue control{};good(self.f.backend.readDevice(*m,SpuSampleBackend::Control,2,control));
            *out={control.word&0x30u,1};return 1;
        }
        return 0;
    }
    int run(std::uint32_t n) {
        Nba97SpuTransfer owner{f.memory,SpuSampleBackend::io,&f.binding,10000};
        return nba97_spu_transfer(&owner,NBA97_SPU_PIO_7D334,Cpu,n,0,f.journal.data(),f.journal.size(),&f.progress);
    }
};
void actualPioProtocol() {
    for(unsigned n=0;n<=65;++n) {
        PioFixture p;const int rc=p.run(n);
        if(n<=64) check(rc==1);else check(rc==NBA97_PATL_IO_REFUSED);
        const unsigned bytes=n==65?64:((n+1)&~1u);
        for(unsigned i=0;i<bytes;++i) check(p.f.backend.known()[0x1000+i]==1&&p.f.backend.samples()[0x1000+i]==p.f.bytes[i]);
        check(p.f.backend.known()[0x1000+bytes]==0&&p.f.backend.request().phase==SpuDmaPhase::Idle);
        check(p.services==(n?1u:0u));
    }
    PioFixture startup;for(unsigned i=0;i<16;++i) startup.f.bytes[i]=7;
    check(startup.run(16)==1);for(unsigned i=0;i<16;++i) check(startup.f.backend.samples()[0x1000+i]==7);
    PioFixture unknown;unknown.f.known[5]=0;check(unknown.run(16)==NBA97_PATL_RESOURCE);
    check(unknown.f.backend.pioRequest().phase==SpuPioPhase::Filling&&unknown.f.backend.pioRequest().bytes==4);
    check(unknown.f.backend.known()[0x1000]==0);
    PioFixture pending;pending.service=false;check(pending.run(16)==NBA97_PATL_IO_REFUSED);
    check(pending.f.backend.pioRequest().phase==SpuPioPhase::Requested&&pending.f.backend.known()[0x1000]==0);
    PioFixture alias;alias.f.bytes.resize(512);alias.f.known.resize(512,1);alias.f.rebind();alias.f.put(0x800c75c8,Cpu);
    check(nba97_voice_patl_write(&alias.f.memory,Cpu+0x1aa,2,0)==1);
    check(nba97_voice_patl_write(&alias.f.memory,Cpu+0x1ae,2,0)==1);
    // Redirected base uses actual retained RAM. It cannot start the real FIFO.
    check(alias.run(16)==1);
    check(alias.f.bytes[0x1a8]==alias.f.bytes[14]&&alias.f.bytes[0x1a9]==alias.f.bytes[15]);
    check(alias.f.bytes[0x1a6]==0&&alias.f.bytes[0x1a7]==2&&alias.statusReads==0);
    check(alias.f.backend.pioRequest().phase==SpuPioPhase::Idle);
}
void startupConfigurations() {
    Fixture f;Nba97SpuTransferValue v{};
    for(unsigned voice=0;voice<24;++voice) for(unsigned offset=0;offset<=10;offset+=2) {
        const auto a=0x1f801c00u+16*voice+offset;
        check(f.backend.writtenConfiguration(a,v).status==SpuSampleStatus::Unknown);
        good(f.backend.writeDevice(f.memory,a,2,0x12340000+voice*16+offset,Generation));
        good(f.backend.writtenConfiguration(a,v));check(v.word==voice*16+offset&&v.known);
        check(f.backend.readDevice(f.memory,a,2,v).status==SpuSampleStatus::Unknown);
    }
    for(unsigned offset:std::array<unsigned,15>{{0x180,0x182,0x184,0x186,0x190,0x192,0x194,0x196,0x198,0x19a,0x1a2,0x1b0,0x1b2,0x1b4,0x1b6}}) {
        const auto a=0x1f801c00u+offset;good(f.backend.writeDevice(f.memory,a,2,0xffff,Generation));
        good(f.backend.writtenConfiguration(a,v));check(v.word==0xffff&&v.known);
        check(f.backend.readDevice(f.memory,a,2,v).status==SpuSampleStatus::Unknown);
    }
    for(unsigned offset=0x188;offset<=0x18e;offset+=2) {
        check(f.backend.writeDevice(f.memory,0x1f801c00+offset,2,0xffff,Generation).status==SpuSampleStatus::Unowned);
        check(f.backend.readDevice(f.memory,0x1f801c00+offset,2,v).status==SpuSampleStatus::Unowned);
    }
    Fixture clone=f;clone.rebind();good(clone.backend.writeDevice(clone.memory,0x1f801c00,2,777,Generation));
    good(f.backend.writtenConfiguration(0x1f801c00,v));check(v.word==0);
    f.spans[2]={f.bytes.data(),f.known.data(),4,0x1f801c00,1,1,0};f.memory.count=3;
    check(f.backend.writeDevice(f.memory,0x1f801c00,2,0,Generation).status==SpuSampleStatus::Ambiguous);
    check(f.backend.readDevice(f.memory,0x1f801c00,2,v).status==SpuSampleStatus::Ambiguous);
}
struct StatusObservation {
    Nba97SpuTransferValue value{};
    unsigned calls=0;
    bool executed=true;
    unsigned successLimit=0;
    static int io(void* p,const Nba97VoicePatlMemory* m,const Nba97SpuTransferEvent* e,Nba97SpuTransferValue* out) {
        auto& self=*static_cast<StatusObservation*>(p);++self.calls;
        check(e->kind==NBA97_SPU_TRANSFER_DEVICE_READ&&e->address==SpuSampleBackend::Status&&e->width==2);
        // A refused observation can still have an actual completed native
        // prefix. The caller cannot roll this retained effect back implicitly.
        check(nba97_voice_patl_write(m,Cpu+100,1,self.calls)==1);
        *out=self.value;return self.executed&&(!self.successLimit||self.calls<=self.successLimit)?1:0;
    }
};
void externalStatusObservations() {
    for(std::uint8_t marker:std::array<std::uint8_t,3>{{0,1,2}}) {
        Fixture f;StatusObservation observation{{0xdeadbeef,marker}};
        observation.successLimit=1;
        f.binding.external=StatusObservation::io;f.binding.externalUser=&observation;
        const int rc=f.run(NBA97_SPU_PIO_7D334,Cpu,16);
        check(rc==(marker==2?NBA97_PATL_METADATA:marker==0?NBA97_PATL_RESOURCE:NBA97_PATL_IO_REFUSED));
        const auto& first=f.journal[0];
        check(first.completed==1&&first.returned.word==0xdeadbeef&&first.returned.known==marker);
        check(f.bytes[100]==observation.calls&&observation.calls>=1);
        Nba97SpuTransferValue out{};check(f.backend.readDevice(f.memory,SpuSampleBackend::Status,2,out).status==SpuSampleStatus::Unknown);
    }
    Fixture f;StatusObservation observation{{0x3456,1}};
    f.binding.external=StatusObservation::io;f.binding.externalUser=&observation;
    Nba97SpuTransferEvent e{};e.kind=NBA97_SPU_TRANSFER_DEVICE_READ;e.address=SpuSampleBackend::Status;e.width=2;
    Nba97SpuTransferValue out{};
    for(unsigned i=0;i<3;++i) {
        check(SpuSampleBackend::io(&f.binding,&f.memory,&e,&out)==1);
        check(out.known==1&&out.word==0x3456&&observation.calls==i+1);
    }
    observation.executed=false;check(SpuSampleBackend::io(&f.binding,&f.memory,&e,&out)==0);
    check(observation.calls==4&&f.bytes[100]==4&&f.backend.lastResult().status==SpuSampleStatus::CallbackRefused);
    observation.executed=true;good(f.backend.importRegister(SpuSampleBackend::Status,2,{0x71,1}));
    check(SpuSampleBackend::io(&f.binding,&f.memory,&e,&out)==1&&out.word==0x71&&observation.calls==4);
    good(f.backend.importRegister(SpuSampleBackend::Status,2,{}));
    f.known[0]=0;
    for(auto a:std::array<std::uint32_t,5>{{Cpu,SpuSampleBackend::TransferAddress,SpuSampleBackend::Madr,0x1f801c00,SpuSampleBackend::Fifo}}) {
        e.address=a;e.width=a==SpuSampleBackend::Madr?4u:2u;
        check(SpuSampleBackend::io(&f.binding,&f.memory,&e,&out)==0&&observation.calls==4);
    }
    e.address=SpuSampleBackend::Status;
    for(auto w:std::array<std::uint32_t,3>{{0,1,4}}) {
        e.width=w;check(SpuSampleBackend::io(&f.binding,&f.memory,&e,&out)==0&&observation.calls==4);
    }
    e.width=2;f.spans[2]={f.bytes.data(),f.known.data(),2,SpuSampleBackend::Status,1,1,0};f.memory.count=3;
    check(SpuSampleBackend::io(&f.binding,&f.memory,&e,&out)==0&&observation.calls==4);
    check(f.backend.lastResult().status==SpuSampleStatus::Ambiguous);
}
int main() { ordinary();paddingAndRefusals();clonesAndAliases();eventsAndIsr();reverseAndPio();pioRequestAndCopy();actualPioProtocol();startupConfigurations();externalStatusObservations();std::printf("spu sample backend: %u checks passed\n",checks); }
