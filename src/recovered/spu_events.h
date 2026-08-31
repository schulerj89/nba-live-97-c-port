#ifndef NBA97_SPU_EVENTS_H
#define NBA97_SPU_EVENTS_H
#include "spu_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97SpuEventsOperation {
    NBA97_SPU_EVENTS_INITIALIZE_7E4C4=0,NBA97_SPU_EVENTS_SHUTDOWN_7E81C,
    NBA97_SPU_EVENTS_REGISTER_7E548,NBA97_SPU_EVENTS_DISPATCH_7F630,
    NBA97_SPU_EVENTS_SET_CALLBACK_7FDB8,NBA97_SPU_EVENTS_IRQ_MASK_7F6EC,
    NBA97_SPU_EVENTS_ENTER_7F308,NBA97_SPU_EVENTS_EXIT_7F578,
    NBA97_SPU_EVENTS_OPEN_7F538,NBA97_SPU_EVENTS_ENABLE_7F358,
    NBA97_SPU_EVENTS_CLOSE_7F2F8,NBA97_SPU_EVENTS_DISABLE_7F4F8
};
enum Nba97SpuEventsKind {
    NBA97_SPU_EVENTS_RAM_STORE=0,NBA97_SPU_EVENTS_DEVICE_READ,NBA97_SPU_EVENTS_DEVICE_WRITE,
    NBA97_SPU_EVENTS_ENTER_CRITICAL,NBA97_SPU_EVENTS_EXIT_CRITICAL,
    NBA97_SPU_EVENTS_OPEN_EVENT,NBA97_SPU_EVENTS_ENABLE_EVENT,
    NBA97_SPU_EVENTS_CLOSE_EVENT,NBA97_SPU_EVENTS_DISABLE_EVENT,
    NBA97_SPU_EVENTS_OTHER_DISPATCH
};
enum Nba97SpuEventsCompletion { NBA97_SPU_EVENTS_LIMIT=-4 };
typedef struct Nba97SpuEventsEvent {
    enum Nba97SpuEventsKind kind;
    uint32_t pc,address,width,value,argument[4];
    Nba97SpuTransferValue returned;
    uint8_t completed;
} Nba97SpuEventsEvent;
/* Required native device/BIOS/other-dispatch effects through this SAME RAM
 * registry. Device pointers may alias retained RAM. No successful missing
 * registration/critical/event stub. Return1 after actual execution and supply
 * raw return knownness; unused/undocumented returns may remain unknown.
 * OPEN needs all4 arguments; other operations use only their declared args.
 * OTHER_DISPATCH carries channel/callback in argument0/1; argument2/3 are
 * unavailable zero placeholders, not claims about volatile CPU registers.
 * Callbacks may change RAM bytes/knownness, never metadata/context/journal. */
typedef int (*Nba97SpuEventsIo)(void*,const Nba97VoicePatlMemory*,
    const Nba97SpuEventsEvent*,Nba97SpuTransferValue*);
typedef struct Nba97SpuEvents {
    Nba97VoicePatlMemory memory;
    Nba97SpuEventsIo io;
    void* user;
    size_t access_budget;
} Nba97SpuEvents;
typedef struct Nba97SpuEventsProgress {
    size_t accesses,events,stores,callbacks_completed;
    uint32_t stopped_pc,stopped_address;
    Nba97SpuTransferValue returned;
    uint8_t completed;
} Nba97SpuEventsProgress;
/* Whole178FE instructions. REGISTER takes callback; DISPATCH/SET_CALLBACK
 * take channel/callback. IRQ_MASK takes a new mask; OPEN takes class/spec/mode/
 * handler; ENABLE/CLOSE/DISABLE take handle. No implicit initialization.
 * Actual7FDB8 dispatch is recovered inline; another encoded target requires
 * OTHER_DISPATCH. SDK interrupt-controller initialization remains separate.
 * Preserve published guards, callback/handle stores, ignored source errors,
 * close-before-disable, live rereads, and unbounded source channel arithmetic.
 * Native refusals retain executed prefixes without rollback or resumability.
 * Source-code/active-stack aliases and context/journal/RAM overlap excluded. */
int nba97_spu_events(Nba97SpuEvents*,enum Nba97SpuEventsOperation,
    uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,
    Nba97SpuEventsEvent*,size_t,Nba97SpuEventsProgress*);
#ifdef __cplusplus
}
#endif
#endif
