#pragma once
#include "spu_sample_backend.hpp"
#include "recovered/spu_events.h"

namespace nba97 {
class SpuEventBackend;
// Temporary view; clone both retained owners together and rebuild this binding.
// generation identifies the sample/event registry, not a CPU address. No source
// event handle or callback pointer is supplied as a manufactured default.
struct SpuEventIoContext {
    SpuEventBackend* backend=nullptr;
    SpuSampleBackend* samples=nullptr;
    std::uint64_t sampleGeneration=0;
    Nba97SpuEventsIo external=nullptr;
    void* externalUser=nullptr;
};
// Native BIOS lifecycle contract, not a reconstruction of BIOS ROM/control
// blocks. Closed Disable has deliberately unknown original return. Syscall
// critical state is separate from the device interrupt-mask register.
class SpuEventBackend {
public:
    static constexpr std::uint32_t IrqMask=0x1f801074,Dicr=0x1f8010f4;
    SpuEventBackend()=default;
    SpuEventBackend(const SpuEventBackend&)=default;
    SpuEventBackend& operator=(const SpuEventBackend&);
    SpuEventBackend(SpuEventBackend&&) noexcept=default;
    SpuEventBackend& operator=(SpuEventBackend&&) noexcept=default;
    // Explicit incoming provenance. Unknown word payload is unused. No ready
    // defaults. I_MASK is16bits; supported DICR registration state has only
    // bits16..23 set (no pending/force/reserved flags). Other states refuse or
    // use an actual external platform operation, never an invented ack model.
    SpuSampleResult importCritical(Nba97SpuTransferValue enabled);
    SpuSampleResult importRegister(std::uint32_t address,std::uint32_t width,Nba97SpuTransferValue);
    Nba97SpuTransferValue criticalEnabled() const { return critical_; }
    bool closedHandle(std::uint32_t) const;
    const SpuSampleResult& lastResult() const { return last_; }
    static int io(void*,const Nba97VoicePatlMemory*,const Nba97SpuEventsEvent*,Nba97SpuTransferValue*);
private:
    Nba97SpuTransferValue critical_{},irqMask_{},dicr_{};
    bool irqExternal_=false,dicrExternal_=false;
    std::uint64_t sampleGeneration_=0;
    std::vector<std::uint32_t> closed_;
    SpuSampleResult last_;
    SpuSampleResult result(SpuSampleStatus,std::uint32_t=0);
    int invoke(SpuEventIoContext&,const Nba97VoicePatlMemory&,const Nba97SpuEventsEvent&,Nba97SpuTransferValue&);
    int forward(SpuEventIoContext&,const Nba97VoicePatlMemory&,const Nba97SpuEventsEvent&,Nba97SpuTransferValue&);
};
}
