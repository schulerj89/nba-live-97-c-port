#include "spu_sample_backend.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace nba97 {
namespace {
struct Byte { std::uint8_t* data=nullptr;std::uint8_t* known=nullptr;bool fully=false,writable=false; };
bool overlaps(std::uint32_t a,std::uint32_t n,std::uint32_t b,std::size_t size) {
    return std::uint64_t(a)<std::uint64_t(b)+size&&std::uint64_t(b)<std::uint64_t(a)+n;
}
bool mappedOverlap(const Nba97VoicePatlMemory& m,std::uint32_t a,std::uint32_t n) {
    if (!m.spans) return false;
    for (std::size_t i=0;i<m.count;++i) {
        const auto& s=m.spans[i];
        if (s.source_address_known&&s.size&&std::uint64_t(s.size)<=UINT64_C(0x100000000)-s.source_address&&overlaps(a,n,s.source_address,s.size)) return true;
    }
    return false;
}
SpuSampleStatus widthAt(const Nba97VoicePatlMemory& m,std::uint32_t a,std::uint32_t width,Byte* out) {
    if (!m.spans&&m.count) return SpuSampleStatus::Metadata;
    bool found=false;
    for (std::size_t i=0;i<m.count;++i) {
        const auto& s=m.spans[i];
        if (!s.source_address_known||a<s.source_address||std::uint64_t(a-s.source_address)>s.size||width>s.size-(a-s.source_address)) continue;
        if (s.source_address_known>1||s.fully_known>1||s.writable>1||!s.data||std::uint64_t(s.size)>UINT64_C(0x100000000)-s.source_address) return SpuSampleStatus::Metadata;
        const auto at=static_cast<std::size_t>(a-s.source_address);
        for (std::uint32_t j=0;j<width;++j) if (s.known&&(s.known[at+j]>1||(s.fully_known&&s.known[at+j]!=1))) return SpuSampleStatus::Metadata;
        if (found) return SpuSampleStatus::Ambiguous;
        found=true;for (std::uint32_t j=0;j<width;++j) out[j]={s.data+at+j,s.known?s.known+at+j:nullptr,s.fully_known!=0,s.writable!=0};
    }
    return found?SpuSampleStatus::Complete:SpuSampleStatus::Unowned;
}
bool mainRam(std::uint32_t a,std::uint32_t count) {
    const auto segment=a&0xe0000000u;
    if (segment!=0&&segment!=0x80000000u&&segment!=0xa0000000u) return false;
    // Accepted actual address windows only. No implicit alias or modulo lookup.
    const auto physical=a-segment;
    return physical<0x200000u&&std::uint64_t(physical)+count<=0x200000u;
}
bool active(SpuDmaPhase p) {
    return p!=SpuDmaPhase::Idle&&p!=SpuDmaPhase::IsrComplete;
}
bool active(SpuPioPhase p) { return p==SpuPioPhase::Filling||p==SpuPioPhase::Requested; }
bool inStorage(const std::uint8_t* p,const std::vector<std::uint8_t>& storage) {
    if (!p||storage.empty()) return false;
    const auto a=reinterpret_cast<std::uintptr_t>(p),base=reinterpret_cast<std::uintptr_t>(storage.data());
    return a>=base&&a-base<storage.size();
}
bool sampleAlias(const Byte& b,const std::vector<std::uint8_t>& samples,const std::vector<std::uint8_t>& known) {
    return inStorage(b.data,samples)||inStorage(b.data,known)||inStorage(b.known,samples)||inStorage(b.known,known);
}
}
SpuSampleBackend::SpuSampleBackend():samples_(SampleBytes,0),known_(SampleBytes,0),registers_{{
    {TransferAddress,2,{}},{Control,2,{}},{TransferControl,2,{}},{Status,2,{}},
    {Madr,4,{}},{Bcr,4,{}},{Chcr,4,{}},{Dpcr,4,{}},{BusDelay,4,{}}
}} {
    std::size_t at=9;
    for (std::uint32_t voice=0;voice<24;++voice)
        for (std::uint32_t offset=0;offset<=10;offset+=2)
            registers_[at++]={0x1f801c00u+16*voice+offset,2,{},true};
    const std::uint32_t offsets[]={0x180,0x182,0x184,0x186,0x190,0x192,0x194,0x196,
        0x198,0x19a,0x1a2,0x1b0,0x1b2,0x1b4,0x1b6};
    for (auto offset:offsets) registers_[at++]={0x1f801c00u+offset,2,{},true};
}
SpuSampleBackend& SpuSampleBackend::operator=(const SpuSampleBackend& other) {
    if (this!=&other) { SpuSampleBackend copy(other);*this=std::move(copy); }
    return *this;
}
SpuSampleResult SpuSampleBackend::result(SpuSampleStatus s,std::uint32_t a,std::uint32_t n) {
    last_={s,a,n,request_.ticket};return last_;
}
SpuSampleBackend::Register* SpuSampleBackend::reg(std::uint32_t a,std::uint32_t w) {
    for (auto& r:registers_) if (r.address==a&&r.width==w) return &r;
    return nullptr;
}
SpuSampleBackend::Event* SpuSampleBackend::event(std::uint32_t handle) {
    for (auto& e:events_) if (e.handle==handle) return &e;
    return nullptr;
}
SpuSampleResult SpuSampleBackend::importRegister(std::uint32_t a,std::uint32_t w,Nba97SpuTransferValue v) {
    if (active(request_.phase)||active(pio_.phase)) return result(SpuSampleStatus::Busy,a,w);
    auto* r=reg(a,w);
    if (!r||r->configurationOnly) return result(SpuSampleStatus::UnsupportedAddress,a,w);
    if (v.known>1||(!v.known&&v.word)||(w==2&&v.word>0xffffu)) return result(SpuSampleStatus::Metadata,a,w);
    r->value=v;if (a==TransferAddress) freshAddress_=false;
    return result(SpuSampleStatus::Complete,a,w);
}
SpuSampleResult SpuSampleBackend::importSamples(std::uint32_t at,const std::uint8_t* data,const std::uint8_t* mask,std::size_t n) {
    if (active(request_.phase)||active(pio_.phase)) return result(SpuSampleStatus::Busy,at);
    if (samples_.size()!=SampleBytes||known_.size()!=SampleBytes||at>SampleBytes||n>SampleBytes-at||(!data&&n)||(!mask&&n)) return result(SpuSampleStatus::Metadata,at);
    for (std::size_t i=0;i<n;++i) if (mask[i]>1) return result(SpuSampleStatus::Metadata,at+static_cast<std::uint32_t>(i));
    // Snapshot inputs may overlap this owner's retained sample/known storage.
    // Capture both before publishing either; a forward copy would corrupt them.
    std::vector<std::uint8_t> copied,copiedKnown;
    try { if (n) { copied.assign(data,data+n);copiedKnown.assign(mask,mask+n); } }
    catch (const std::bad_alloc&) { return result(SpuSampleStatus::OutOfMemory,at); }
    for (std::size_t i=0;i<n;++i) { samples_[at+i]=copied[i];known_[at+i]=copiedKnown[i]; }
    return result(SpuSampleStatus::Complete,at,static_cast<std::uint32_t>(n));
}
SpuSampleResult SpuSampleBackend::readDevice(const Nba97VoicePatlMemory& m,std::uint32_t a,std::uint32_t w,Nba97SpuTransferValue& out) {
    out={};
    if ((!m.spans&&m.count)||(w!=1&&w!=2&&w!=4)||(a&(w-1))||std::uint64_t(a)+w>UINT64_C(0x100000000)) return result(SpuSampleStatus::Metadata,a,w);
    auto* r=reg(a,w);const bool mapped=mappedOverlap(m,a,w);
    bool device=false;for (const auto& item:registers_) device=device||overlaps(a,w,item.address,item.width);
    device=device||overlaps(a,w,Fifo,2);
    if (mapped&&device) return result(SpuSampleStatus::Ambiguous,a,w);
    if (device) {
        if (!r) return result(SpuSampleStatus::UnsupportedAddress,a,w);
        if (r->configurationOnly||!r->value.known) return result(SpuSampleStatus::Unknown,a,w);
        out=r->value;return result(SpuSampleStatus::Complete,a,w);
    }
    std::array<Byte,4> bytes{};
    auto access=widthAt(m,a,w,bytes.data());if (access!=SpuSampleStatus::Complete) return result(access,a,w);
    for (std::uint32_t i=0;i<w;++i) if (sampleAlias(bytes[i],samples_,known_)) return result(SpuSampleStatus::Ambiguous,a+i,w);
    for (std::uint32_t i=0;i<w;++i) if (!bytes[i].fully&&(!bytes[i].known||!*bytes[i].known)) return result(SpuSampleStatus::Unknown,a+i,w);
    out.known=1;for (std::uint32_t i=0;i<w;++i) out.word|=std::uint32_t(*bytes[i].data)<<(8*i);
    return result(SpuSampleStatus::Complete,a,w);
}
SpuSampleResult SpuSampleBackend::writeDevice(const Nba97VoicePatlMemory& m,std::uint32_t a,std::uint32_t w,std::uint32_t v,std::uint64_t generation) {
    if ((!m.spans&&m.count)||(w!=1&&w!=2&&w!=4)||(a&(w-1))||std::uint64_t(a)+w>UINT64_C(0x100000000)) return result(SpuSampleStatus::Metadata,a,w);
    auto* r=reg(a,w);const bool mapped=mappedOverlap(m,a,w);
    bool device=false;for (const auto& item:registers_) device=device||overlaps(a,w,item.address,item.width);
    device=device||overlaps(a,w,Fifo,2);
    if (mapped&&device) return result(SpuSampleStatus::Ambiguous,a,w);
    if (device) {
        if (a==Fifo&&w==2) {
            if (active(request_.phase)) return result(SpuSampleStatus::Busy,a,w);
            return queuePio(v,generation);
        }
        if (!r||a==Status) return result(SpuSampleStatus::UnsupportedAddress,a,w);
        if (active(request_.phase)&&!(a==Control&&request_.phase==SpuDmaPhase::IsrRunning)) return result(SpuSampleStatus::Busy,a,w);
        if (active(pio_.phase)&&!(pio_.phase==SpuPioPhase::Filling&&a==Control&&(v&0x30u)==0x10u))
            return result(SpuSampleStatus::Busy,a,w);
        r->value={w==2?v&0xffffu:v,1};
        if (a==TransferAddress) { freshAddress_=true;pio_={}; }
        if (a==Control&&(v&0x30u)==0x10u) return startPio(generation);
        // Once a PIO mode transition occurs, a former status snapshot is not
        // current hardware evidence. Poll observations must come from the
        // actual platform; neither service nor mode-stop invents ready bits.
        if (a==Control&&pio_.phase==SpuPioPhase::Copied) reg(Status,2)->value={};
        // Preserve the reached CHCR store even when its unsupported request is
        // refused. The CPU owner's prefix is not rolled back by this backend.
        if (a==Chcr) return startDma(generation);
        return result(SpuSampleStatus::Complete,a,w);
    }
    std::array<Byte,4> bytes{};
    auto access=widthAt(m,a,w,bytes.data());if (access!=SpuSampleStatus::Complete) return result(access,a,w);
    for (std::uint32_t i=0;i<w;++i) if (sampleAlias(bytes[i],samples_,known_)) return result(SpuSampleStatus::Ambiguous,a+i,w);
    for (std::uint32_t i=0;i<w;++i) if (!bytes[i].writable||(!bytes[i].fully&&!bytes[i].known)) return result(SpuSampleStatus::ReadOnly,a+i,w);
    for (std::uint32_t i=0;i<w;++i) { *bytes[i].data=static_cast<std::uint8_t>(v>>(8*i));if (bytes[i].known) *bytes[i].known=1; }
    return result(SpuSampleStatus::Complete,a,w);
}
SpuSampleResult SpuSampleBackend::writtenConfiguration(std::uint32_t a,Nba97SpuTransferValue& out) {
    out={};auto* r=reg(a,2);
    if (!r||!r->configurationOnly) return result(SpuSampleStatus::UnsupportedAddress,a,2);
    if (!r->value.known) return result(SpuSampleStatus::Unknown,a,2);
    out=r->value;return result(SpuSampleStatus::Complete,a,2);
}
SpuSampleResult SpuSampleBackend::queuePio(std::uint32_t value,std::uint64_t generation) {
    if (pio_.phase==SpuPioPhase::Requested) return result(SpuSampleStatus::Busy,Fifo,2);
    if (pio_.phase==SpuPioPhase::Copied) return result(SpuSampleStatus::UnsupportedTransfer,Fifo,2);
    if (!generation||(pio_.phase==SpuPioPhase::Filling&&generation!=pio_.memoryGeneration))
        return result(SpuSampleStatus::StaleGeneration,Fifo,2);
    const auto control=reg(Control,2)->value,type=reg(TransferControl,2)->value,address=reg(TransferAddress,2)->value;
    if (!control.known||!type.known||!address.known) return result(SpuSampleStatus::Unknown,Fifo,2);
    if ((control.word&0x30u)!=0||type.word!=4||
        (pio_.phase!=SpuPioPhase::Filling&&!freshAddress_)||pio_.bytes>=64)
        return result(SpuSampleStatus::UnsupportedTransfer,Fifo,2);
    const auto start=pio_.phase==SpuPioPhase::Filling?pio_.spuAddress:address.word*8u;
    const auto count=pio_.phase==SpuPioPhase::Filling?pio_.bytes:0u;
    if (std::uint64_t(start)+count+2>SampleBytes) return result(SpuSampleStatus::UnsupportedTransfer,start,count+2);
    if (pio_.phase!=SpuPioPhase::Filling) pio_={SpuPioPhase::Filling,generation,start,0};
    pioWords_[pio_.bytes/2]=static_cast<std::uint16_t>(value);pio_.bytes+=2;freshAddress_=false;
    return result(SpuSampleStatus::Complete,Fifo,2);
}
SpuSampleResult SpuSampleBackend::startPio(std::uint64_t generation) {
    // The reached control store is retained even if this supported-domain
    // guard refuses. No empty flush, transfer-type guess or implicit cursor.
    reg(Status,2)->value={};
    if (pio_.phase!=SpuPioPhase::Filling||!pio_.bytes) return result(SpuSampleStatus::UnsupportedTransfer,Control,2);
    if (!generation||generation!=pio_.memoryGeneration) return result(SpuSampleStatus::StaleGeneration,Control,2);
    pio_.phase=SpuPioPhase::Requested;return result(SpuSampleStatus::Complete,Control,2);
}
SpuSampleResult SpuSampleBackend::servicePendingPio(std::uint64_t generation) {
    if (pio_.phase!=SpuPioPhase::Requested) return result(SpuSampleStatus::NoRequest);
    if (!generation||generation!=pio_.memoryGeneration) return result(SpuSampleStatus::StaleGeneration);
    const auto start=pio_.spuAddress,n=pio_.bytes;
    if (samples_.size()!=SampleBytes||known_.size()!=SampleBytes) return result(SpuSampleStatus::Metadata);
    for (std::uint32_t i=0;i<n;++i) if (known_[start+i]>1) return result(SpuSampleStatus::Metadata,start+i,n);
    for (std::uint32_t i=0;i<n;++i) { samples_[start+i]=static_cast<std::uint8_t>(pioWords_[i/2]>>(8*(i%2)));known_[start+i]=1; }
    pio_.phase=SpuPioPhase::Copied;
    // Do not assert persistent internal transfer-cursor/readback behavior.
    // Another chunk requires an actual fresh address store; previous copied
    // bytes and any old BIOS pending event remain exactly as they are.
    reg(TransferAddress,2)->value={};reg(Status,2)->value={};
    return result(SpuSampleStatus::Complete,start,n);
}
SpuSampleResult SpuSampleBackend::startDma(std::uint64_t generation) {
    const std::uint32_t required[]={Madr,Bcr,Chcr,Control,TransferAddress,TransferControl,Dpcr};
    for (auto a:required) if (!reg(a,(a>=TransferAddress&&a<=TransferControl)?2u:4u)->value.known) return result(SpuSampleStatus::Unknown,a);
    const auto madr=reg(Madr,4)->value.word,bcr=reg(Bcr,4)->value.word,chcr=reg(Chcr,4)->value.word;
    const auto control=reg(Control,2)->value.word,spu=reg(TransferAddress,2)->value.word*8u;
    const auto blocks=bcr>>16,bytes=blocks*64u;
    // Source7DC54 truncates ceil(requestedSize/64) into BCR's high16 bits.
    // Do not repair oversized/zero requests using the original requested size.
    // Zero-count hardware behavior and SPU wrap are unproved supported domains.
    if (!freshAddress_||(bcr&0xffffu)!=16u||!blocks||
        (chcr!=0x01000201u&&chcr!=0x01000200u)||
        (control&0x30u)!=(chcr==0x01000201u?0x20u:0x30u)||
        reg(TransferControl,2)->value.word!=4u||!(reg(Dpcr,4)->value.word&0x80000u)||
        std::uint64_t(spu)+bytes>SampleBytes) return result(SpuSampleStatus::UnsupportedTransfer,Chcr,bytes);
    if (madr&3u) return result(SpuSampleStatus::UnsupportedDmaAlignment,madr,bytes);
    if (!mainRam(madr,bytes)) return result(SpuSampleStatus::UnsupportedAddress,madr,bytes);
    if (!generation||!nextTicket_) return result(SpuSampleStatus::StaleGeneration,madr,bytes);
    request_={SpuDmaPhase::Requested,nextTicket_++,generation,madr,spu,bytes,madr,bcr,chcr,chcr==0x01000201u};
    freshAddress_=false;
    // Original has no old pending-event clear at7DC90/7D9E8. An earlier event
    // can therefore be consumed while this new request has not completed.
    return result(SpuSampleStatus::Complete,madr,bytes);
}
SpuSampleResult SpuSampleBackend::servicePendingDma(const Nba97VoicePatlMemory& m,std::uint64_t generation) {
    if (request_.phase!=SpuDmaPhase::Requested) return result(SpuSampleStatus::NoRequest);
    if (!generation||generation!=request_.memoryGeneration) return result(SpuSampleStatus::StaleGeneration);
    const auto n=request_.bytes,spu=request_.spuAddress;
    if (samples_.size()!=SampleBytes||known_.size()!=SampleBytes) return result(SpuSampleStatus::Metadata);
    std::vector<Byte> cpu;
    try { cpu.resize(n); } catch (const std::bad_alloc&) { return result(SpuSampleStatus::OutOfMemory); }
    // Complete reached metadata validation precedes unknown-data refusal. A
    // malformed later mask must not be hidden behind an earlier unknown byte.
    for (std::uint32_t i=0;i<n;i+=4) {
        auto s=widthAt(m,request_.cpuAddress+i,4,cpu.data()+i);if (s!=SpuSampleStatus::Complete) return result(s,request_.cpuAddress+i,n);
    }
    for (std::uint32_t i=0;i<n;++i) {
        if (sampleAlias(cpu[i],samples_,known_)) return result(SpuSampleStatus::Ambiguous,request_.cpuAddress+i,n);
        if (known_[spu+i]>1) return result(SpuSampleStatus::Metadata,spu+i,n);
    }
    for (std::uint32_t i=0;i<n;++i) {
        if (request_.toSpu) {
            if (!cpu[i].fully&&(!cpu[i].known||!*cpu[i].known)) return result(SpuSampleStatus::Unknown,request_.cpuAddress+i,n);
        } else {
            if (!cpu[i].writable||(!cpu[i].fully&&!cpu[i].known)) return result(SpuSampleStatus::ReadOnly,request_.cpuAddress+i,n);
            if (!known_[spu+i]) return result(SpuSampleStatus::Unknown,spu+i,n);
        }
    }
    for (std::uint32_t i=0;i<n;++i) {
        if (request_.toSpu) { samples_[spu+i]=*cpu[i].data;known_[spu+i]=1; }
        else { *cpu[i].data=samples_[spu+i];if (cpu[i].known) *cpu[i].known=1; }
    }
    // DMA hardware updates these registers. Exact final readback values are
    // outside this bounded copy contract; never expose stale start values as
    // proven completed-hardware state. The recovered ISR does not read them.
    reg(Madr,4)->value={};reg(Bcr,4)->value={};reg(Chcr,4)->value={};
    request_.phase=SpuDmaPhase::CopiedAwaitingIsr;
    return result(SpuSampleStatus::Complete,request_.cpuAddress,n);
}
SpuSampleResult SpuSampleBackend::beginIsr(std::uint64_t ticket) {
    if (!ticket||ticket!=request_.ticket||request_.phase!=SpuDmaPhase::CopiedAwaitingIsr) return result(SpuSampleStatus::InvalidTicket);
    request_.phase=SpuDmaPhase::IsrRunning;return result(SpuSampleStatus::Complete);
}
SpuSampleResult SpuSampleBackend::finishIsr(std::uint64_t ticket,bool completed) {
    if (!ticket||ticket!=request_.ticket||request_.phase!=SpuDmaPhase::IsrRunning) return result(SpuSampleStatus::InvalidTicket);
    request_.phase=completed?SpuDmaPhase::IsrComplete:SpuDmaPhase::IsrRefused;
    return result(completed?SpuSampleStatus::Complete:SpuSampleStatus::CallbackRefused);
}
SpuSampleResult SpuSampleBackend::openEvent(std::uint32_t cls,std::uint32_t spec,std::uint32_t mode,std::uint32_t handler,std::uint32_t& handle) {
    handle=0;
    if (mode!=0x2000u||handler) return result(SpuSampleStatus::UnsupportedTransfer);
    if (!nextHandle_||nextHandle_==0xffffffffu) return result(SpuSampleStatus::OutOfMemory);
    try { events_.push_back({nextHandle_,cls,spec,false,false}); }
    catch (const std::bad_alloc&) { return result(SpuSampleStatus::OutOfMemory); }
    handle=nextHandle_++;return result(SpuSampleStatus::Complete);
}
SpuSampleResult SpuSampleBackend::enableEvent(std::uint32_t h) {
    auto* e=event(h);if (!e) return result(SpuSampleStatus::InvalidEvent);
    e->enabled=true;e->pending=false;return result(SpuSampleStatus::Complete);
}
SpuSampleResult SpuSampleBackend::disableEvent(std::uint32_t h) {
    auto* e=event(h);if (!e) return result(SpuSampleStatus::InvalidEvent);
    e->enabled=false;e->pending=false;return result(SpuSampleStatus::Complete);
}
SpuSampleResult SpuSampleBackend::closeEvent(std::uint32_t h) {
    for (auto i=events_.begin();i!=events_.end();++i) if (i->handle==h) { events_.erase(i);return result(SpuSampleStatus::Complete); }
    return result(SpuSampleStatus::InvalidEvent);
}
SpuSampleResult SpuSampleBackend::deliverEvent(std::uint32_t cls,std::uint32_t spec) {
    for (auto& e:events_) if (e.eventClass==cls&&e.spec==spec&&e.enabled) e.pending=true;
    return result(SpuSampleStatus::Complete);
}
SpuSampleResult SpuSampleBackend::testEvent(std::uint32_t h,std::uint32_t& occurred) {
    occurred=0;auto* e=event(h);if (!e) return result(SpuSampleStatus::InvalidEvent);
    occurred=e->enabled&&e->pending?1u:0u;
    if (occurred) e->pending=false;
    return result(SpuSampleStatus::Complete);
}
int SpuSampleBackend::io(void* opaque,const Nba97VoicePatlMemory* memory,const Nba97SpuTransferEvent* e,Nba97SpuTransferValue* out) {
    if (out) *out={};
    auto* binding=static_cast<SpuSampleIoContext*>(opaque);
    if (!binding||!binding->backend||!memory||!e||!out) return 0;
    auto& b=*binding->backend;
    switch (e->kind) {
    case NBA97_SPU_TRANSFER_DEVICE_READ: {
        const auto r=b.readDevice(*memory,e->address,e->width,*out);
        if (r) return 1;
        // Only canonical SPUSTAT permits a required live observation. Never
        // cache it, infer it from Control, or fall back for unknown CPU bytes,
        // aliases, unsupported widths or other retained register values.
        if (r.status!=SpuSampleStatus::Unknown||e->address!=Status||e->width!=2) return 0;
        break;
    }
    case NBA97_SPU_TRANSFER_DEVICE_WRITE: return b.writeDevice(*memory,e->address,e->width,e->value,binding->memoryGeneration)?1:0;
    case NBA97_SPU_TRANSFER_TEST_EVENT_B0_0B: {
        std::uint32_t v=0;auto r=b.testEvent(e->argument[0],v);if (r) *out={v,1};return r?1:0;
    }
    case NBA97_SPU_TRANSFER_DELIVER_EVENT_B0_07: return b.deliverEvent(e->argument[0],e->argument[1])?1:0;
    case NBA97_SPU_TRANSFER_CALLBACK_7D668:
        if (b.request_.phase!=SpuDmaPhase::IsrRunning) { b.result(SpuSampleStatus::InvalidTicket);return 0; }
        [[fallthrough]];
    case NBA97_SPU_TRANSFER_DIAGNOSTIC_83B20:
        break;
    default: b.result(SpuSampleStatus::UnsupportedTransfer,e->address);return 0;
    }
    if (!binding->external||binding->external(binding->externalUser,memory,e,out)!=1) { b.result(SpuSampleStatus::CallbackRefused,e->address);return 0; }
    // The external callback already ran. Preserve both its output bits
    // (known0 permits an unused opaque word) and the completed-call prefix.
    // The C owner diagnoses known>1 AFTER recording completed execution.
    if (out->known>1) { b.result(SpuSampleStatus::Metadata,e->address);return 1; }
    b.result(SpuSampleStatus::Complete,e->address);return 1;
}
}
