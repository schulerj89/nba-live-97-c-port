#pragma once
#include "interrupt_controller_backend.hpp"
#include "recovered/spu_initialize.h"

namespace nba97 {
// Borrowed composition of the existing native owners; no second device cache,
// prepared controller readiness, synthetic handle, sample padding or IRQ clock.
// Rebuild these pointers after cloning owners and RAM together. Each journal
// and the cumulative access budget belong to this initialization invocation's
// LOWER controller/event/PIO calls. Outer initialization has its own access
// budget and journal; direct device/diagnostic imports are accounted there.
// Reset explicitly only for a NEW invocation, never to resume a refused prefix.
struct SpuInitializeIoContext {
    InterruptControllerIoContext* controller=nullptr;
    SpuSampleIoContext* transfers=nullptr;
    Nba97SpuInitializeIo external=nullptr;
    void* externalUser=nullptr;
    Nba97InterruptEvent* controllerJournal=nullptr;
    std::size_t controllerCapacity=0,controllerEvents=0;
    Nba97SpuEventsEvent* eventJournal=nullptr;
    std::size_t eventCapacity=0,eventEvents=0;
    Nba97SpuTransferEvent* transferJournal=nullptr;
    std::size_t transferCapacity=0,transferEvents=0;
    std::size_t accessBudget=0,accesses=0,operationsCompleted=0;
    Nba97InterruptProgress controllerProgress{};
    Nba97SpuEventsProgress eventProgress{};
    Nba97SpuTransferProgress transferProgress{};
    int completion=0;
    bool active=false;
};
// The reached controller target7F708 executes the native context wrapper and
// actual recovered controller code. Other live targets/diagnostics require
// external ownership. EVENTS executes7E4C4; PIO executes7D334 with the actual
// source/count. Device effects use the same shared controller/event/sample path.
// PIO service scheduling and status observations remain platform obligations;
// this bridge does not fabricate a finished transfer or readback.
int spuInitializeIo(void*,const Nba97VoicePatlMemory*,const Nba97SpuInitializeEvent*,Nba97SpuTransferValue*);
}
