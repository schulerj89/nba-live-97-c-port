#include "spu_initialize_backend.hpp"
#include <limits>

namespace nba97 {
namespace {
bool valid(const SpuInitializeIoContext& c) {
    if(!c.controller||!c.transfers||!c.controller->backend||!c.controller->events.backend||
        !c.transfers->backend||c.controller->events.samples!=c.transfers->backend||
        !c.controller->memoryGeneration||c.controller->memoryGeneration!=c.transfers->memoryGeneration||
        !c.controller->events.sampleGeneration||c.accesses>c.accessBudget||
        c.operationsCompleted==std::numeric_limits<std::size_t>::max()||
        c.controllerEvents>c.controllerCapacity||c.eventEvents>c.eventCapacity||c.transferEvents>c.transferCapacity||
        (!c.controllerJournal&&c.controllerCapacity)||(!c.eventJournal&&c.eventCapacity)||
        (!c.transferJournal&&c.transferCapacity)||c.active)return false;
    return true;
}
template<class T>T* tail(T* p,std::size_t offset) { return p?p+offset:nullptr; }
struct Active {
    bool& value;
    explicit Active(bool& v):value(v) { value=true; }
    ~Active() { value=false; }
};
}
int spuInitializeIo(void* opaque,const Nba97VoicePatlMemory* memory,
    const Nba97SpuInitializeEvent* e,Nba97SpuTransferValue* out) {
    if(!opaque||!memory||!e||!out||(!memory->spans&&memory->count))return 0;
    auto& c=*static_cast<SpuInitializeIoContext*>(opaque);
    if(!valid(c))return 0;
    Active active(c.active);
    if(e->kind==NBA97_SPU_INITIALIZE_CONTROLLER&&e->address==0x8007f708u) {
        c.completion=c.controller->backend->run(*c.controller,*memory,NBA97_INTERRUPT_INITIALIZE_7F708,
            0,0,{0,0},tail(c.controllerJournal,c.controllerEvents),c.controllerCapacity-c.controllerEvents,
            c.controllerProgress,c.accessBudget-c.accesses);
        c.accesses+=c.controllerProgress.accesses;c.controllerEvents+=c.controllerProgress.events;
        if(c.completion!=1&&c.completion!=NBA97_INTERRUPT_TRANSFERRED)return 0;
        *out=c.controllerProgress.returned;++c.operationsCompleted;
        return c.completion==NBA97_INTERRUPT_TRANSFERRED?NBA97_SPU_INITIALIZE_TRANSFERRED:1;
    }
    if(e->kind==NBA97_SPU_INITIALIZE_EVENTS) {
        Nba97SpuEvents owner{*memory,SpuEventBackend::io,&c.controller->events,c.accessBudget-c.accesses};
        c.completion=nba97_spu_events(&owner,NBA97_SPU_EVENTS_INITIALIZE_7E4C4,0,0,0,0,
            tail(c.eventJournal,c.eventEvents),c.eventCapacity-c.eventEvents,&c.eventProgress);
        c.accesses+=c.eventProgress.accesses;c.eventEvents+=c.eventProgress.events;
        if(c.completion!=1)return 0;
        *out=c.eventProgress.returned;++c.operationsCompleted;return 1;
    }
    if(e->kind==NBA97_SPU_INITIALIZE_PIO) {
        Nba97SpuTransfer owner{*memory,SpuSampleBackend::io,c.transfers,c.accessBudget-c.accesses};
        c.completion=nba97_spu_transfer(&owner,NBA97_SPU_PIO_7D334,e->argument[0],e->argument[1],0,
            tail(c.transferJournal,c.transferEvents),c.transferCapacity-c.transferEvents,&c.transferProgress);
        c.accesses+=c.transferProgress.accesses;c.transferEvents+=c.transferProgress.events;
        if(c.completion!=1)return 0;
        *out=c.transferProgress.returned;++c.operationsCompleted;return 1;
    }
    if(e->kind==NBA97_SPU_INITIALIZE_DEVICE_READ||e->kind==NBA97_SPU_INITIALIZE_DEVICE_WRITE) {
        // CPU allocation generation is distinct from the event-registry
        // generation. A live register alias can program FIFO or DMA here; its
        // request must retain the actual CPU allocation identity, just as PIO.
        if(e->kind==NBA97_SPU_INITIALIZE_DEVICE_WRITE&&
            (e->address==SpuSampleBackend::Fifo||e->address==SpuSampleBackend::Control||e->address==SpuSampleBackend::Chcr)) {
            auto result=c.transfers->backend->writeDevice(*memory,e->address,e->width,e->value,c.transfers->memoryGeneration);
            if(result)return 1;
            return 0; // These are owned sample slots; a refusal cannot fall through.
        }
        // Status observations belong to the same sample owner and its explicit
        // platform callback. Unknown RAM/registers are not fallback candidates.
        if(e->kind==NBA97_SPU_INITIALIZE_DEVICE_READ&&e->address==SpuSampleBackend::Status&&e->width==2) {
            Nba97SpuTransferEvent status{};status.kind=NBA97_SPU_TRANSFER_DEVICE_READ;
            status.pc=e->pc;status.address=e->address;status.width=e->width;
            return SpuSampleBackend::io(c.transfers,memory,&status,out);
        }
        Nba97InterruptEvent device{};
        device.kind=e->kind==NBA97_SPU_INITIALIZE_DEVICE_READ?NBA97_INTERRUPT_DEVICE_READ:NBA97_INTERRUPT_DEVICE_WRITE;
        device.pc=e->pc;device.address=e->address;device.width=e->width;device.value=e->value;
        return InterruptControllerBackend::io(c.controller,memory,&device,out);
    }
    if(e->kind==NBA97_SPU_INITIALIZE_CONTROLLER||e->kind==NBA97_SPU_INITIALIZE_DIAGNOSTIC)
        return c.external?c.external(c.externalUser,memory,e,out):0;
    return 0;
}
}
