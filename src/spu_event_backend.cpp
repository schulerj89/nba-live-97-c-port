#include "spu_event_backend.hpp"
#include <algorithm>
#include <new>
#include <utility>

namespace nba97 {
namespace {
bool overlap(std::uint32_t a,std::uint32_t w,std::uint32_t b,std::uint64_t n) {
    return std::uint64_t(a)<std::uint64_t(b)+n&&std::uint64_t(b)<std::uint64_t(a)+w;
}
bool mapped(const Nba97VoicePatlMemory& m,std::uint32_t a,std::uint32_t w) {
    if (!m.spans) return false;
    for (std::size_t i=0;i<m.count;++i) {
        const auto& s=m.spans[i];
        if (s.source_address_known&&std::uint64_t(s.size)<=UINT64_C(0x100000000)-s.source_address&&overlap(a,w,s.source_address,s.size)) return true;
    }
    return false;
}
bool supportedDicr(std::uint32_t v) { return (v&0xff00ffffu)==0; }
}
SpuEventBackend& SpuEventBackend::operator=(const SpuEventBackend& other) {
    if (this!=&other) { SpuEventBackend copy(other);*this=std::move(copy); }
    return *this;
}
SpuSampleResult SpuEventBackend::result(SpuSampleStatus s,std::uint32_t a) {
    last_={s,a,0,0};return last_;
}
bool SpuEventBackend::closedHandle(std::uint32_t h) const {
    return std::find(closed_.begin(),closed_.end(),h)!=closed_.end();
}
SpuSampleResult SpuEventBackend::importCritical(Nba97SpuTransferValue v) {
    if (v.known>1||(v.known&&v.word>1)) return result(SpuSampleStatus::Metadata);
    critical_=v;return result(SpuSampleStatus::Complete);
}
SpuSampleResult SpuEventBackend::importRegister(std::uint32_t a,std::uint32_t w,Nba97SpuTransferValue v) {
    if (v.known>1) return result(SpuSampleStatus::Metadata,a);
    if (a==IrqMask&&w==2) {
        if (v.known&&v.word>0xffffu) return result(SpuSampleStatus::Metadata,a);
        irqMask_=v;irqExternal_=false;return result(SpuSampleStatus::Complete,a);
    }
    if (a==Dicr&&w==4) {
        if (v.known&&!supportedDicr(v.word)) return result(SpuSampleStatus::UnsupportedTransfer,a);
        dicr_=v;dicrExternal_=false;return result(SpuSampleStatus::Complete,a);
    }
    return result(SpuSampleStatus::UnsupportedAddress,a);
}
int SpuEventBackend::forward(SpuEventIoContext& c,const Nba97VoicePatlMemory& m,const Nba97SpuEventsEvent& e,Nba97SpuTransferValue& v) {
    if (!c.external||c.external(c.externalUser,&m,&e,&v)!=1) { result(SpuSampleStatus::CallbackRefused,e.address);return 0; }
    // Already executed: the C owner must record completed callback state even
    // for known>1, then diagnose metadata. known0 retains arbitrary opaque bits.
    result(v.known>1?SpuSampleStatus::Metadata:SpuSampleStatus::Complete,e.address);return 1;
}
int SpuEventBackend::invoke(SpuEventIoContext& c,const Nba97VoicePatlMemory& m,const Nba97SpuEventsEvent& e,Nba97SpuTransferValue& out) {
    if (!c.sampleGeneration||(sampleGeneration_&&sampleGeneration_!=c.sampleGeneration)) { result(SpuSampleStatus::StaleGeneration);return 0; }
    if ((!m.spans&&m.count)||!c.samples) { result(SpuSampleStatus::Metadata);return 0; }
    sampleGeneration_=c.sampleGeneration;
    if (e.kind==NBA97_SPU_EVENTS_ENTER_CRITICAL) {
        // No nesting counter. Unknown prior state still permits the actual
        // disabling effect; an unavailable prior state makes its result unknown.
        out=critical_.known?Nba97SpuTransferValue{critical_.word,1}:Nba97SpuTransferValue{};
        critical_={0,1};result(SpuSampleStatus::Complete);return 1;
    }
    if (e.kind==NBA97_SPU_EVENTS_EXIT_CRITICAL) {
        critical_={1,1};out={};result(SpuSampleStatus::Complete);return 1;
    }
    if (e.kind==NBA97_SPU_EVENTS_DEVICE_READ||e.kind==NBA97_SPU_EVENTS_DEVICE_WRITE) {
        const auto a=e.address,w=e.width;const bool write=e.kind==NBA97_SPU_EVENTS_DEVICE_WRITE;
        if ((w!=1&&w!=2&&w!=4)||(a&(w-1))||std::uint64_t(a)+w>UINT64_C(0x100000000)) { result(SpuSampleStatus::Metadata,a);return 0; }
        const bool local=overlap(a,w,IrqMask,2)||overlap(a,w,Dicr,4);
        if (local&&mapped(m,a,w)) { result(SpuSampleStatus::Ambiguous,a);return 0; }
        if (a==IrqMask&&w==2) {
            if (irqExternal_) return forward(c,m,e,out);
            if (write) { irqMask_={e.value&0xffffu,1};result(SpuSampleStatus::Complete,a);return 1; }
            if (irqMask_.known) { out=irqMask_;result(SpuSampleStatus::Complete,a);return 1; }
            if (c.external) irqExternal_=true;
            return forward(c,m,e,out);
        }
        if (a==Dicr&&w==4) {
            // Registration-only, known zero-pending/no-force domain. Generic
            // DICR stores would destroy W1C pending semantics, so never use one.
            if (!dicrExternal_&&dicr_.known&&(!write||supportedDicr(e.value))) {
                if (write) dicr_={e.value,1};else out=dicr_;
                result(SpuSampleStatus::Complete,a);return 1;
            }
            // An external device operation may change the register. Do not
            // retain old cached readback as authoritative, even on refusal.
            if (c.external) { dicr_={};dicrExternal_=true; }
            return forward(c,m,e,out);
        }
        if (local) {
            if (c.external) {
                if (overlap(a,w,IrqMask,2)) { irqMask_={};irqExternal_=true; }
                if (overlap(a,w,Dicr,4)) { dicr_={};dicrExternal_=true; }
            }
            return forward(c,m,e,out);
        }
        // Reuse the frozen owner's exact Patl width/knownness/alias boundary.
        // Do not offer unknown or malformed mapped RAM to a fallback handler.
        auto r=write?c.samples->writeDevice(m,a,w,e.value,c.sampleGeneration):c.samples->readDevice(m,a,w,out);
        if (r) { last_=r;return 1; }
        // UnsupportedAddress can mean a known sample-owned register at an
        // unsupported width. Delegating that access would let an external
        // write diverge from the frozen owner's still-known cached register.
        // Only a truly Unowned address can acquire an external device owner.
        if (mapped(m,a,w)||r.status!=SpuSampleStatus::Unowned) { last_=r;return 0; }
        return forward(c,m,e,out);
    }
    if (e.kind==NBA97_SPU_EVENTS_OTHER_DISPATCH) return forward(c,m,e,out);
    const auto h=e.argument[0];SpuSampleResult r;
    if (e.kind==NBA97_SPU_EVENTS_OPEN_EVENT||e.kind==NBA97_SPU_EVENTS_CLOSE_EVENT) {
        if (!critical_.known||critical_.word!=0) { result(SpuSampleStatus::UnsupportedTransfer,h);return 0; }
    }
    switch (e.kind) {
    case NBA97_SPU_EVENTS_OPEN_EVENT: {
        std::uint32_t handle=0;r=c.samples->openEvent(e.argument[0],e.argument[1],e.argument[2],e.argument[3],handle);
        if (r) out={handle,1};break;
    }
    case NBA97_SPU_EVENTS_ENABLE_EVENT:
        if (closedHandle(h)) { result(SpuSampleStatus::InvalidEvent,h);return 0; }
        r=c.samples->enableEvent(h);if (r) out={1,1};break;
    case NBA97_SPU_EVENTS_CLOSE_EVENT:
        if (closedHandle(h)) { result(SpuSampleStatus::InvalidEvent,h);return 0; }
        // Reserve before releasing an actual event so allocation failure cannot
        // discard the only provenance for the following original Disable call.
        try { closed_.reserve(closed_.size()+1); }
        catch (const std::bad_alloc&) { result(SpuSampleStatus::OutOfMemory,h);return 0; }
        r=c.samples->closeEvent(h);if (r) { closed_.push_back(h);out={1,1}; }break;
    case NBA97_SPU_EVENTS_DISABLE_EVENT:
        if (closedHandle(h)) {
            // Original7E81C closes before disabling and does not clear C7678.
            // Retain that order. This native tombstone no-op has UNKNOWN ROM
            // return; never invent numeric0 or revive/re-enable the closed event.
            out={};result(SpuSampleStatus::Complete,h);return 1;
        }
        r=c.samples->disableEvent(h);if (r) out={1,1};break;
    default: result(SpuSampleStatus::UnsupportedTransfer,e.address);return 0;
    }
    last_=r;return r?1:0;
}
int SpuEventBackend::io(void* p,const Nba97VoicePatlMemory* m,const Nba97SpuEventsEvent* e,Nba97SpuTransferValue* out) {
    if (out) *out={};auto* c=static_cast<SpuEventIoContext*>(p);
    if (!c||!c->backend||!m||!e||!out) return 0;
    return c->backend->invoke(*c,*m,*e,*out);
}
}
