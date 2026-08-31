#pragma once
#include "spu_event_backend.hpp"
#include "recovered/interrupt_controller.h"

namespace nba97 {
class InterruptControllerBackend;
// Borrowed bindings must be rebuilt after cloning all retained owners/RAM.
// memoryGeneration identifies the actual CPU allocation generation. The sample
// generation remains the one used by the shared sample/event registry.
struct InterruptControllerIoContext {
    InterruptControllerBackend* backend=nullptr;
    SpuEventIoContext events{};
    std::uint64_t memoryGeneration=0;
    Nba97InterruptIo external=nullptr;
    void* externalUser=nullptr;
};
enum class InterruptExceptionPhase { Idle, Running, Returned, Refused, ExternalTransfer };
// Native BIOS/context contract, not BIOS ROM or saved host/MIPS stack execution.
// Counter-clear policy, I_STAT, timer hardware and general DICR remain required
// platform operations. The shared event/sample device owners are never copied
// into a second register cache. No physical interrupt, DMA or VBlank cadence.
// Active IRQ callbacks may not switch thread/context or independently invoke
// critical-section systemcalls: those can destroy the BIOS interrupted context
// and require a broader context owner. No nested IRQ or TCB emulation is claimed.
// The 48 reached context bytes/masks must not internally alias one another;
// BIOS capture store order on aliased storage is not established. Separate
// source-address aliases of the complete retained buffer remain supported.
class InterruptControllerBackend {
public:
    static constexpr std::uint32_t ContextAddress=0x800c7dfc,
        ContinuationPc=0x8007f748,SourceS0=0x800c7dc8,ExceptionSp=0x800dad08;
    InterruptControllerBackend()=default;
    // No borrowed pointer is retained. Copy RAM and all backend owners together,
    // preserve their generation identities, then rebuild temporary bindings.
    InterruptControllerBackend(const InterruptControllerBackend&)=default;
    InterruptControllerBackend& operator=(const InterruptControllerBackend&)=default;
    // Optional actual incoming saved-register provenance, indices as Sony
    // SETJMP.H [PC,SP,FP,S0..S7,GP]. PC/S0 are source-produced constants here;
    // unknown payload is irrelevant. No implicit zero-filled register snapshot.
    SpuSampleResult importCaptureRegisters(const std::array<Nba97SpuTransferValue,12>&);
    // Explicit native service ownership/provenance. These flags govern the
    // native ISO driver availability / PAD interrupt completion policy. Imported
    // unknown state is permitted: the subsequent void setter supplies its value.
    SpuSampleResult importIso9660Driver(Nba97SpuTransferValue);
    SpuSampleResult importPadClearPolicy(Nba97SpuTransferValue);
    Nba97SpuTransferValue iso9660Driver() const { return isoDriver_; }
    Nba97SpuTransferValue padClearPolicy() const { return padClear_; }
    bool contextCaptured() const { return captured_; }
    bool hookInstalled() const { return hooked_; }
    InterruptExceptionPhase exceptionPhase() const { return phase_; }
    const SpuSampleResult& lastResult() const { return last_; }
    // Caller owns the source journal/prefix; refusal never rolls back. Capture
    // is authorized only inside this wrapper's INITIALIZE_7F708 operation.
    int run(InterruptControllerIoContext&,const Nba97VoicePatlMemory&,
        Nba97InterruptOperation,std::uint32_t,std::uint32_t,Nba97SpuTransferValue,
        Nba97InterruptEvent*,std::size_t,Nba97InterruptProgress&,std::size_t accessBudget);
    // Explicit platform IRQ entry, NOT automatic scheduling or evidence that a
    // device requested an IRQ. Requires an installed live sealed context and
    // known enabled critical state. Executes the real recovered handler; only
    // successful ReturnFromException restores this native interrupted context.
    // A refused handler retains its prefix/disabled state and cannot be retried.
    int enterException(InterruptControllerIoContext&,const Nba97VoicePatlMemory&,
        Nba97InterruptEvent*,std::size_t,Nba97InterruptProgress&,std::size_t accessBudget);
    static int io(void*,const Nba97VoicePatlMemory*,const Nba97InterruptEvent*,Nba97SpuTransferValue*);
private:
    std::array<Nba97SpuTransferValue,12> registers_{};
    std::array<std::uint8_t,48> sealedData_{},sealedKnown_{};
    std::uint64_t generation_=0;
    bool captured_=false,hooked_=false,captureAllowed_=false,running_=false;
    bool isoOwned_=false,padOwned_=false;
    Nba97SpuTransferValue isoDriver_{},padClear_{},interruptedCritical_{};
    InterruptExceptionPhase phase_=InterruptExceptionPhase::Idle;
    SpuSampleResult last_{};
    SpuSampleResult result(SpuSampleStatus,std::uint32_t=0);
    int invoke(InterruptControllerIoContext&,const Nba97VoicePatlMemory&,
        const Nba97InterruptEvent&,Nba97SpuTransferValue&);
    int forward(InterruptControllerIoContext&,const Nba97VoicePatlMemory&,
        const Nba97InterruptEvent&,Nba97SpuTransferValue&);
    bool contextBytes(const Nba97VoicePatlMemory&,bool writable,
        std::array<std::uint8_t*,48>&,std::array<std::uint8_t*,48>&);
    bool liveContext(const Nba97VoicePatlMemory&,bool seal);
};
}
