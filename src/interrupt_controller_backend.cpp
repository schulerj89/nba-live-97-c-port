#include "interrupt_controller_backend.hpp"

namespace nba97 {
namespace {
struct SharedForward {
    InterruptControllerIoContext* context;
    const Nba97InterruptEvent* event;
    int completed=0;
};
int sharedExternal(void* p,const Nba97VoicePatlMemory* m,const Nba97SpuEventsEvent* e,Nba97SpuTransferValue* v) {
    auto& s=*static_cast<SharedForward*>(p);
    if (s.context->events.external)
        s.completed=s.context->events.external(s.context->events.externalUser,m,e,v);
    else if (s.context->external)
        s.completed=s.context->external(s.context->externalUser,m,s.event,v);
    else return 0;
    // Preserve completed-but-invalid transfer dispositions for the outer C
    // owner to journal and diagnose, rather than relabeling them unexecuted.
    return s.completed==1||s.completed==2?1:0;
}
bool flag(Nba97SpuTransferValue v) { return v.known<=1&&(!v.known||v.word<=1); }
}
SpuSampleResult InterruptControllerBackend::result(SpuSampleStatus s,std::uint32_t a) {
    last_={s,a,0,0};return last_;
}
SpuSampleResult InterruptControllerBackend::importCaptureRegisters(const std::array<Nba97SpuTransferValue,12>& r) {
    if (running_||phase_==InterruptExceptionPhase::Running) return result(SpuSampleStatus::Busy);
    for (auto v:r) if (v.known>1) return result(SpuSampleStatus::Metadata);
    if ((r[0].known&&r[0].word!=ContinuationPc)||(r[3].known&&r[3].word!=SourceS0)) return result(SpuSampleStatus::Metadata);
    registers_=r;return result(SpuSampleStatus::Complete);
}
SpuSampleResult InterruptControllerBackend::importIso9660Driver(Nba97SpuTransferValue v) {
    if (!flag(v)) return result(SpuSampleStatus::Metadata);
    isoDriver_=v;isoOwned_=true;return result(SpuSampleStatus::Complete);
}
SpuSampleResult InterruptControllerBackend::importPadClearPolicy(Nba97SpuTransferValue v) {
    if (!flag(v)) return result(SpuSampleStatus::Metadata);
    padClear_=v;padOwned_=true;return result(SpuSampleStatus::Complete);
}
int InterruptControllerBackend::forward(InterruptControllerIoContext& c,const Nba97VoicePatlMemory& m,
    const Nba97InterruptEvent& e,Nba97SpuTransferValue& out) {
    const int rc=c.external?c.external(c.externalUser,&m,&e,&out):0;
    if (rc!=1&&rc!=2) { result(SpuSampleStatus::CallbackRefused,e.address);return 0; }
    result(out.known>1?SpuSampleStatus::Metadata:SpuSampleStatus::Complete,e.address);return rc;
}
bool InterruptControllerBackend::contextBytes(const Nba97VoicePatlMemory& m,bool writable,
    std::array<std::uint8_t*,48>& data,std::array<std::uint8_t*,48>& known) {
    if (!m.spans&&m.count) { result(SpuSampleStatus::Metadata);return false; }
    // This native capture/seal operation reaches the 12-word jmp_buf, not the
    // rest of any allocation. Each word requires one encompassing Patl span.
    for (std::size_t n=0;n<12;++n) {
        const auto a=ContextAddress+static_cast<std::uint32_t>(4*n);
        const Nba97VoicePatlSpan* found=nullptr;std::size_t offset=0;
        for (std::size_t j=0;j<m.count;++j) {
            const auto& s=m.spans[j];
            if (!s.source_address_known||!s.data||std::uint64_t(s.size)>UINT64_C(0x100000000)-s.source_address||a<s.source_address) continue;
            const std::size_t at=a-s.source_address;
            if (at>s.size||4>s.size-at) continue;
            if (s.source_address_known>1||s.writable>1||s.fully_known>1) { result(SpuSampleStatus::Metadata,a);return false; }
            if (s.known) for (std::size_t k=0;k<4;++k)
                if (s.known[at+k]>1||(s.fully_known&&s.known[at+k]!=1)) { result(SpuSampleStatus::Metadata,a);return false; }
            if (found) { result(SpuSampleStatus::Ambiguous,a);return false; }
            found=&s;offset=at;
        }
        if (!found) { result(SpuSampleStatus::Unowned,a);return false; }
        if (writable&&(!found->writable||!found->known||found->fully_known)) {
            // Immutable/all-known views cannot represent unknown saved CPU
            // registers. Refuse instead of leaving the preceding clear known.
            result(SpuSampleStatus::ReadOnly,a);return false;
        }
        for (std::size_t k=0;k<4;++k) {
            data[4*n+k]=found->data+offset+k;
            known[4*n+k]=found->known?found->known+offset+k:nullptr;
            if (!found->fully_known&&!found->known) { result(SpuSampleStatus::Unknown,a);return false; }
        }
    }
    // RAM and per-byte metadata are distinct storage. Capturing into a mask
    // alias could silently corrupt ownership while an operation is executing.
    for (auto d:data) for (auto k:known) if (d==k) { result(SpuSampleStatus::Ambiguous,ContextAddress);return false; }
    // The BIOS store order has not been proved. Do not select a winner when
    // distinct saved words/bytes alias the same payload or knownness cell.
    for (std::size_t i=0;i<48;++i) for (std::size_t j=0;j<i;++j)
        if (data[i]==data[j]||(known[i]&&known[i]==known[j])) {
            result(SpuSampleStatus::Ambiguous,ContextAddress+static_cast<std::uint32_t>(i));return false;
        }
    return true;
}
bool InterruptControllerBackend::liveContext(const Nba97VoicePatlMemory& m,bool seal) {
    if (!captured_) { result(SpuSampleStatus::Unowned,ContextAddress);return false; }
    std::uint32_t pc=0,sp=0;
    const int p=nba97_voice_patl_read(&m,ContextAddress,4,&pc);
    if (p!=1) { result(p==NBA97_PATL_METADATA?SpuSampleStatus::Metadata:SpuSampleStatus::Unknown,ContextAddress);return false; }
    const int s=nba97_voice_patl_read(&m,ContextAddress+4,4,&sp);
    if (s!=1) { result(s==NBA97_PATL_METADATA?SpuSampleStatus::Metadata:SpuSampleStatus::Unknown,ContextAddress+4);return false; }
    if (pc!=ContinuationPc||sp!=ExceptionSp) { result(SpuSampleStatus::UnsupportedTransfer,ContextAddress);return false; }
    std::array<std::uint8_t*,48> data{},known{};
    if (!contextBytes(m,false,data,known)) return false;
    for (std::size_t i=0;i<48;++i) {
        const auto k=known[i]?*known[i]:std::uint8_t(1);
        if (seal) { sealedKnown_[i]=k;sealedData_[i]=k?*data[i]:0; }
        else if (k!=sealedKnown_[i]||(k&&*data[i]!=sealedData_[i])) {
            result(SpuSampleStatus::UnsupportedTransfer,ContextAddress+static_cast<std::uint32_t>(i));return false;
        }
    }
    return true;
}
int InterruptControllerBackend::invoke(InterruptControllerIoContext& c,const Nba97VoicePatlMemory& m,
    const Nba97InterruptEvent& e,Nba97SpuTransferValue& out) {
    if (!c.memoryGeneration||(generation_&&generation_!=c.memoryGeneration)) { result(SpuSampleStatus::StaleGeneration);return 0; }
    if ((!m.spans&&m.count)||!c.events.backend||!c.events.samples||!c.events.sampleGeneration) { result(SpuSampleStatus::Metadata);return 0; }
    generation_=c.memoryGeneration;
    if (e.kind==NBA97_INTERRUPT_DEVICE_READ||e.kind==NBA97_INTERRUPT_DEVICE_WRITE||
        e.kind==NBA97_INTERRUPT_ENTER_CRITICAL||e.kind==NBA97_INTERRUPT_EXIT_CRITICAL) {
        Nba97SpuEventsEvent event{};event.pc=e.pc;event.address=e.address;event.width=e.width;event.value=e.value;
        switch(e.kind) {
        case NBA97_INTERRUPT_DEVICE_READ:event.kind=NBA97_SPU_EVENTS_DEVICE_READ;break;
        case NBA97_INTERRUPT_DEVICE_WRITE:event.kind=NBA97_SPU_EVENTS_DEVICE_WRITE;break;
        case NBA97_INTERRUPT_ENTER_CRITICAL:event.kind=NBA97_SPU_EVENTS_ENTER_CRITICAL;break;
        default:event.kind=NBA97_SPU_EVENTS_EXIT_CRITICAL;break;
        }
        SharedForward forwarding{&c,&e};auto shared=c.events;
        // Keep the SAME device owner. Delegation/invalidation is performed by
        // its existing sticky-ownership protocol, never by an extra shadow.
        shared.external=sharedExternal;shared.externalUser=&forwarding;
        const int rc=SpuEventBackend::io(&shared,&m,&event,&out);last_=c.events.backend->lastResult();
        return rc==1&&forwarding.completed==2?2:rc;
    }
    switch(e.kind) {
    case NBA97_INTERRUPT_CAPTURE_CONTEXT: {
        if (!captureAllowed_||e.argument[0]!=ContextAddress) { result(SpuSampleStatus::UnsupportedTransfer,e.address);return 0; }
        std::array<std::uint8_t*,48> data{},known{};
        if (!contextBytes(m,true,data,known)) return 0;
        auto saved=registers_;saved[0]={ContinuationPc,1};saved[3]={SourceS0,1};
        // Sony SETJMP.H and LIBC2's setjmp establish this 12-word API. This is
        // native capture, not a claim about BIOS ROM's internal store order.
        // Source7F734 has already cleared these bytes. Capture overwrites the
        // unavailable registers as UNKNOWN; their earlier zeros are not facts.
        for (std::size_t i=0;i<48;++i) {
            const auto v=saved[i/4];*data[i]=v.known?static_cast<std::uint8_t>(v.word>>(8*(i%4))):0;*known[i]=v.known;
        }
        captured_=true;out={0,1};result(SpuSampleStatus::Complete,ContextAddress);return 1;
    }
    case NBA97_INTERRUPT_HOOK_CONTEXT:
        if (e.argument[0]!=ContextAddress) { result(SpuSampleStatus::UnsupportedAddress,e.address);return 0; }
        if (!liveContext(m,true)) return 0;
        hooked_=true;out={};result(SpuSampleStatus::Complete,ContextAddress);return 1;
    case NBA97_INTERRUPT_RETURN_EXCEPTION:
        if (phase_!=InterruptExceptionPhase::Running) { result(SpuSampleStatus::UnsupportedTransfer);return 0; }
        if (!c.events.backend->importCritical(interruptedCritical_)) { last_=c.events.backend->lastResult();return 0; }
        phase_=InterruptExceptionPhase::Returned;out={};result(SpuSampleStatus::Complete);return NBA97_INTERRUPT_TRANSFERRED;
    case NBA97_INTERRUPT_REMOVE_CDROM_DRIVER:
        if (isoOwned_) { isoDriver_={0,1};out={};result(SpuSampleStatus::Complete);return 1; }
        return forward(c,m,e,out);
    case NBA97_INTERRUPT_CHANGE_CLEAR_PAD:
        if (padOwned_&&e.argument[0]<=1) { padClear_={e.argument[0],1};out={};result(SpuSampleStatus::Complete);return 1; }
        if (c.external) { padOwned_=false;padClear_={}; }
        return forward(c,m,e,out);
    default:
        // No unproved C0/0A policy, general IRQ ack, diagnostic, CD driver or
        // callback is replaced by a successful no-op.
        return forward(c,m,e,out);
    }
}
int InterruptControllerBackend::io(void* p,const Nba97VoicePatlMemory* m,const Nba97InterruptEvent* e,Nba97SpuTransferValue* v) {
    if (v) *v={};auto* c=static_cast<InterruptControllerIoContext*>(p);
    if (!c||!c->backend||!m||!e||!v) return 0;
    return c->backend->invoke(*c,*m,*e,*v);
}
int InterruptControllerBackend::run(InterruptControllerIoContext& c,const Nba97VoicePatlMemory& m,
    Nba97InterruptOperation op,std::uint32_t a0,std::uint32_t a1,Nba97SpuTransferValue incoming,
    Nba97InterruptEvent* journal,std::size_t capacity,Nba97InterruptProgress& progress,std::size_t budget) {
    // A malformed C request has no source or native-entry effect. In
    // particular it must not bind a previously unused allocation generation.
    if ((!journal&&capacity)||(!m.spans&&m.count)||
        op<NBA97_INTERRUPT_INITIALIZE_7F708||op>NBA97_INTERRUPT_RETURN_7FB98) {
        result(SpuSampleStatus::Metadata);return NBA97_PATL_ARGUMENT;
    }
    progress={};
    if (c.backend!=this||running_||!c.memoryGeneration||(generation_&&generation_!=c.memoryGeneration)) {
        result(running_?SpuSampleStatus::Busy:SpuSampleStatus::StaleGeneration);return NBA97_PATL_IO_REFUSED;
    }
    generation_=c.memoryGeneration;running_=true;captureAllowed_=op==NBA97_INTERRUPT_INITIALIZE_7F708;
    Nba97InterruptController owner{m,io,&c,budget};
    const int rc=nba97_interrupt_controller(&owner,op,a0,a1,incoming,journal,capacity,&progress);
    running_=false;captureAllowed_=false;return rc;
}
int InterruptControllerBackend::enterException(InterruptControllerIoContext& c,const Nba97VoicePatlMemory& m,
    Nba97InterruptEvent* journal,std::size_t capacity,Nba97InterruptProgress& progress,std::size_t budget) {
    // Check structural arguments BEFORE disabling or retaining a native frame.
    // capacity0/budget0 are valid bounded requests and retain entry effects.
    if ((!journal&&capacity)||(!m.spans&&m.count)) {
        result(SpuSampleStatus::Metadata);return NBA97_PATL_ARGUMENT;
    }
    progress={};
    if (c.backend!=this||!c.events.backend||!c.memoryGeneration||generation_!=c.memoryGeneration) {
        result(SpuSampleStatus::StaleGeneration);return NBA97_PATL_IO_REFUSED;
    }
    if (running_||phase_==InterruptExceptionPhase::Running||phase_==InterruptExceptionPhase::Refused||phase_==InterruptExceptionPhase::ExternalTransfer) {
        result(SpuSampleStatus::Busy);return NBA97_PATL_IO_REFUSED;
    }
    if (!hooked_||!liveContext(m,false)) { if (!hooked_) result(SpuSampleStatus::Unowned,ContextAddress);return NBA97_PATL_IO_REFUSED; }
    interruptedCritical_=c.events.backend->criticalEnabled();
    if (!interruptedCritical_.known||interruptedCritical_.word!=1) { result(SpuSampleStatus::UnsupportedTransfer);return NBA97_PATL_IO_REFUSED; }
    // Reuse the shared owner's binding validation before changing critical
    // state. importCritical alone would bypass its sample-generation guard.
    // This is the native exception-entry effect, not an extra source syscall.
    Nba97SpuEventsEvent enter{};enter.kind=NBA97_SPU_EVENTS_ENTER_CRITICAL;
    Nba97SpuTransferValue previous{};
    if (SpuEventBackend::io(&c.events,&m,&enter,&previous)!=1) {
        last_=c.events.backend->lastResult();return NBA97_PATL_IO_REFUSED;
    }
    phase_=InterruptExceptionPhase::Running;
    const int rc=run(c,m,NBA97_INTERRUPT_HANDLE_7F7C8,0,0,{},journal,capacity,progress,budget);
    if (phase_==InterruptExceptionPhase::Running)
        phase_=rc==NBA97_INTERRUPT_TRANSFERRED?InterruptExceptionPhase::ExternalTransfer:InterruptExceptionPhase::Refused;
    return rc;
}
}
