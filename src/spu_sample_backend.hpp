#pragma once
#include "recovered/spu_transfer.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nba97 {
enum class SpuSampleStatus {
    Complete, NoRequest, Unknown, Metadata, Unowned, Ambiguous, ReadOnly,
    UnsupportedAddress, UnsupportedDmaAlignment, UnsupportedTransfer,
    Busy, StaleGeneration, InvalidTicket, InvalidEvent, CallbackRefused,
    OutOfMemory
};
enum class SpuDmaPhase { Idle, Requested, CopiedAwaitingIsr, IsrRunning, IsrComplete, IsrRefused };
enum class SpuPioPhase { Idle, Filling, Requested, Copied };
struct SpuPioRequest {
    SpuPioPhase phase=SpuPioPhase::Idle;
    std::uint64_t memoryGeneration=0;
    std::uint32_t spuAddress=0,bytes=0;
};
struct SpuSampleResult {
    SpuSampleStatus status=SpuSampleStatus::Complete;
    std::uint32_t address=0,bytes=0;
    std::uint64_t ticket=0;
    explicit operator bool() const { return status==SpuSampleStatus::Complete; }
};
struct SpuDmaRequest {
    SpuDmaPhase phase=SpuDmaPhase::Idle;
    std::uint64_t ticket=0,memoryGeneration=0;
    std::uint32_t cpuAddress=0,spuAddress=0,bytes=0,madr=0,bcr=0,chcr=0;
    bool toSpu=false;
};
class SpuSampleBackend;
// Rebuild this temporary binding after cloning/moving either retained owner.
// memoryGeneration identifies the actual retained CPU allocation generation;
// no host pointer or CPU-byte snapshot is retained by the backend request.
struct SpuSampleIoContext {
    SpuSampleBackend* backend=nullptr;
    std::uint64_t memoryGeneration=0;
    Nba97SpuTransferIo external=nullptr;
    void* externalUser=nullptr;
};

// Bounded native sample storage, NOT an SPU emulator/audio renderer. No startup,
// ISR registration, address wrap, DMA timing or physical BIOS-handle
// claim. Source and known-mask storage must not alias backend/context metadata.
// CPU source-address spans cannot share the sample array/known mask: original
// CPU RAM and SPU RAM are separate storage. Reached aliases are refused.
class SpuSampleBackend {
public:
    static constexpr std::size_t SampleBytes=0x80000;
    static constexpr std::uint32_t TransferAddress=0x1f801da6,
        Fifo=0x1f801da8,Control=0x1f801daa,TransferControl=0x1f801dac,
        Status=0x1f801dae,Madr=0x1f8010c0,Bcr=0x1f8010c4,Chcr=0x1f8010c8,
        Dpcr=0x1f8010f0,BusDelay=0x1f801014;
    SpuSampleBackend();
    SpuSampleBackend(const SpuSampleBackend&)=default;
    SpuSampleBackend& operator=(const SpuSampleBackend&);
    SpuSampleBackend(SpuSampleBackend&&) noexcept=default;
    SpuSampleBackend& operator=(SpuSampleBackend&&) noexcept=default;

    const std::vector<std::uint8_t>& samples() const { return samples_; }
    const std::vector<std::uint8_t>& known() const { return known_; }
    const SpuDmaRequest& request() const { return request_; }
    const SpuPioRequest& pioRequest() const { return pio_; }
    const SpuSampleResult& lastResult() const { return last_; }

    // Explicit known entry/snapshot import, not implicit hardware initialization.
    // Register import cannot create a request or count as a fresh address store.
    // This snapshot API requires known0/word0. This is stricter than the external
    // C callback ABI, where known0 may carry arbitrary unused word bits.
    // A Status import is an explicitly authoritative fixture/snapshot value,
    // not proof that hardware status remains unchanged after Control writes.
    // The existing DMA snapshot domain is retained; PIO start/copy/stop makes
    // Status unknown. Natural startup must use actual external observations.
    SpuSampleResult importRegister(std::uint32_t address,std::uint32_t width,Nba97SpuTransferValue);
    SpuSampleResult importSamples(std::uint32_t offset,const std::uint8_t*,const std::uint8_t*,std::size_t);
    SpuSampleResult readDevice(const Nba97VoicePatlMemory&,std::uint32_t address,std::uint32_t width,Nba97SpuTransferValue&);
    SpuSampleResult writeDevice(const Nba97VoicePatlMemory&,std::uint32_t address,std::uint32_t width,std::uint32_t value,std::uint64_t generation);
    // Requires known rounded source bytes, canonical metadata, proven main-RAM
    // mappings, aligned MADR, ordinary nonzero BCR blocks and no SPU end crossing.
    // Refusal changes no copy bytes. Service is synchronous without interleaving.
    // It neither delivers an event nor claims the ISR ran. Repeated service after
    // a successful copy cannot copy twice.
    // Each CPU word requires one encompassing span, as in Patl's width4 access;
    // adjacent spans can cover different words, not split a single word.
    // MADR/BCR/CHCR become unknown after copying: final hardware readback is not
    // established here. Fresh source writes may establish their next values.
    SpuSampleResult servicePendingDma(const Nba97VoicePatlMemory&,std::uint64_t generation);
    // One1..32-halfword FIFO chunk per fresh transfer-address store, type4,
    // initially stopped transfer mode. FIFO stores retain actual CPU values;
    // Control mode10 requests their copy. Service copies exactly2*N bytes,
    // without DMA rounding, padding, status manufacture or event delivery.
    // Multi-chunk cursor continuation and cancellation are unproved domains:
    // a next chunk without a fresh address refuses, preserving copied bytes.
    // Source7CE18's16-byte initialization at1000 is inside this domain.
    SpuSampleResult servicePendingPio(std::uint64_t generation);
    // Startup configuration storage is distinct from hardware readback. These
    // new slots retain exact successful writes, but readDevice returns Unknown;
    // physical voice/envelope/reverb behavior is not implemented here. Key-on/
    // off command registers remain Unowned and require actual platform effects.
    SpuSampleResult writtenConfiguration(std::uint32_t address,Nba97SpuTransferValue&);
    // The caller must execute the actual recovered ISR between these calls.
    // false preserves copied data and source prefix, terminally refusing retry.
    SpuSampleResult beginIsr(std::uint64_t ticket);
    SpuSampleResult finishIsr(std::uint64_t ticket,bool actualOwnerCompleted);

    // Actual native event allocation, distinct from Sony ROM handle encoding.
    // Only the proven NOINTR2000/no-handler mode is supported. New events are
    // disabled. No default handle/readiness, implicit enabling or start-time clear.
    SpuSampleResult openEvent(std::uint32_t eventClass,std::uint32_t spec,std::uint32_t mode,std::uint32_t handler,std::uint32_t& handle);
    SpuSampleResult enableEvent(std::uint32_t handle);
    SpuSampleResult disableEvent(std::uint32_t handle);
    SpuSampleResult closeEvent(std::uint32_t handle);
    SpuSampleResult deliverEvent(std::uint32_t eventClass,std::uint32_t spec);
    SpuSampleResult testEvent(std::uint32_t handle,std::uint32_t& occurred);
    // An unknown canonical Status halfword read requires external observation.
    // Its exact return/knownness is forwarded without caching or auto-service.
    // Other unknown registers/RAM and unsupported accesses never fall back.
    static int io(void*,const Nba97VoicePatlMemory*,const Nba97SpuTransferEvent*,Nba97SpuTransferValue*);
private:
    struct Register { std::uint32_t address,width;Nba97SpuTransferValue value;bool configurationOnly=false; };
    struct Event { std::uint32_t handle,eventClass,spec;bool enabled,pending; };
    std::vector<std::uint8_t> samples_,known_;
    std::array<Register,168> registers_;
    std::vector<Event> events_;
    SpuDmaRequest request_;
    SpuPioRequest pio_;
    std::array<std::uint16_t,32> pioWords_{};
    SpuSampleResult last_;
    std::uint64_t nextTicket_=1;
    std::uint32_t nextHandle_=1;
    bool freshAddress_=false;
    Register* reg(std::uint32_t,std::uint32_t);
    Event* event(std::uint32_t);
    SpuSampleResult result(SpuSampleStatus,std::uint32_t=0,std::uint32_t=0);
    SpuSampleResult startDma(std::uint64_t);
    SpuSampleResult queuePio(std::uint32_t,std::uint64_t);
    SpuSampleResult startPio(std::uint64_t);
};
}
