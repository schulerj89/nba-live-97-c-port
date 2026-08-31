#pragma once
#include "spu_initialize_backend.hpp"
#include "recovered/audio_startup.h"
#include "recovered/spu_heap.h"

namespace nba97 {
// Borrowed composition only. RAM, device/event/controller owners and allocation
// generations are the same ones bound to initialization. Rebuild all temporary
// pointers after cloning the retained owners and RAM together. No factory,
// source-memory defaults, extra device cache or prepared callback table.
struct AudioStartupIoContext {
    SpuInitializeIoContext* initialize=nullptr;
    Nba97SpuInitializeEvent* initializeJournal=nullptr;
    std::size_t initializeCapacity=0,initializeEvents=0;
    Nba97SpuHeapStore* heapJournal=nullptr;
    std::size_t heapCapacity=0,heapStores=0;
    // Immediate lower initialization CPU + heap accesses only. The outer audio
    // CPU and the nested controller/event/PIO owners have separate budgets and
    // receipts; their accesses are not silently double counted here.
    std::size_t accessBudget=0,accesses=0,operationsCompleted=0;
    Nba97SpuInitializeProgress initializeProgress{};
    Nba97SpuHeapProgress heapProgress{};
    int completion=0;
    bool active=false;
};
// Calls actual7E6EC and7E940; common-setter device accesses use the existing
// initialization device bridge, including its exact CPU generation for requests.
// Nonlocal initialization return2 terminates the outer CPU run. Refusals retain
// all reached lower effects/journals; counters may be reset explicitly for a
// NEW invocation, never as authorization to replay a refused source prefix.
// PARAMETER_STORE/RAM_STORE are CPU journal entries, not callback work.
// Registration only stores a source callback token: this bridge neither invokes
// the music timer nor supplies cadence, voice state or hardware synthesis.
int audioStartupIo(void*,const Nba97VoicePatlMemory*,const Nba97AudioStartupEvent*,Nba97SpuTransferValue*);
}
