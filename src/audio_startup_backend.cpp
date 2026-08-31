#include "audio_startup_backend.hpp"
#include <limits>

namespace nba97 {
namespace {
bool valid(const AudioStartupIoContext& c) {
    return c.initialize&&!c.active&&c.accesses<=c.accessBudget&&
        c.operationsCompleted!=std::numeric_limits<std::size_t>::max()&&
        c.initializeEvents<=c.initializeCapacity&&c.heapStores<=c.heapCapacity&&
        (c.initializeJournal||!c.initializeCapacity)&&(c.heapJournal||!c.heapCapacity);
}
template<class T>T* tail(T* p,std::size_t offset) { return p?p+offset:nullptr; }
struct Active {
    bool& value;
    explicit Active(bool& v):value(v) { value=true; }
    ~Active() { value=false; }
};
}
int audioStartupIo(void* opaque,const Nba97VoicePatlMemory* memory,
    const Nba97AudioStartupEvent* e,Nba97SpuTransferValue* out) {
    if(!opaque||!memory||!e||!out||(!memory->spans&&memory->count))return 0;
    auto& c=*static_cast<AudioStartupIoContext*>(opaque);
    if(!valid(c))return 0;
    Active active(c.active);
    *out={};
    if(e->kind==NBA97_AUDIO_STARTUP_INITIALIZE&&e->address==0x8007e6ecu) {
        Nba97SpuInitialize owner{*memory,spuInitializeIo,c.initialize,c.accessBudget-c.accesses};
        c.completion=nba97_spu_initialize(&owner,NBA97_SPU_INITIALIZE_7E6EC,0,0,0,
            tail(c.initializeJournal,c.initializeEvents),c.initializeCapacity-c.initializeEvents,&c.initializeProgress);
        c.accesses+=c.initializeProgress.accesses;c.initializeEvents+=c.initializeProgress.events;
        if(c.completion!=1&&c.completion!=NBA97_SPU_INITIALIZE_TRANSFERRED)return 0;
        *out=c.initializeProgress.returned;++c.operationsCompleted;
        return c.completion==NBA97_SPU_INITIALIZE_TRANSFERRED?NBA97_AUDIO_STARTUP_TRANSFERRED:1;
    }
    if(e->kind==NBA97_AUDIO_STARTUP_HEAP&&e->address==0x8007e940u) {
        Nba97SpuHeap owner{*memory,c.accessBudget-c.accesses};
        c.completion=nba97_spu_heap(&owner,NBA97_SPU_HEAP_INITIALIZE_7E940,e->argument[0],e->argument[1],
            tail(c.heapJournal,c.heapStores),c.heapCapacity-c.heapStores,&c.heapProgress);
        c.accesses+=c.heapProgress.accesses;c.heapStores+=c.heapProgress.stores;
        if(c.completion!=1)return 0;
        *out={c.heapProgress.return_v0,1};++c.operationsCompleted;return 1;
    }
    if(e->kind==NBA97_AUDIO_STARTUP_DEVICE_READ||e->kind==NBA97_AUDIO_STARTUP_DEVICE_WRITE) {
        Nba97SpuInitializeEvent device{};
        device.kind=e->kind==NBA97_AUDIO_STARTUP_DEVICE_READ?NBA97_SPU_INITIALIZE_DEVICE_READ:NBA97_SPU_INITIALIZE_DEVICE_WRITE;
        device.pc=e->pc;device.address=e->address;device.width=e->width;device.value=e->value;
        return spuInitializeIo(c.initialize,memory,&device,out);
    }
    return 0;
}
}
