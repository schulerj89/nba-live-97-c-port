#ifndef NBA97_INTERRUPT_CONTROLLER_H
#define NBA97_INTERRUPT_CONTROLLER_H
#include "spu_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97InterruptOperation {
    NBA97_INTERRUPT_INITIALIZE_7F708=0,NBA97_INTERRUPT_SHUTDOWN_7FAE4,
    NBA97_INTERRUPT_DISPATCH_7F600,NBA97_INTERRUPT_REGISTER_7F9BC,
    NBA97_INTERRUPT_HANDLE_7F7C8,NBA97_INTERRUPT_DMA_INITIALIZE_7FBA8,
    NBA97_INTERRUPT_DMA_SHUTDOWN_7FC0C,NBA97_INTERRUPT_DMA_HANDLE_7FC54,
    NBA97_INTERRUPT_VBLANK_INITIALIZE_7FEEC,NBA97_INTERRUPT_VBLANK_SHUTDOWN_7FF64,
    NBA97_INTERRUPT_VBLANK_HANDLE_7FF9C,NBA97_INTERRUPT_VBLANK_SET_7FFE8,
    NBA97_INTERRUPT_CLEAR_7FB4C,NBA97_INTERRUPT_CLEAR_7FEC0,NBA97_INTERRUPT_CLEAR_80020,
    NBA97_INTERRUPT_ENTER_7F308,NBA97_INTERRUPT_EXIT_7F578,
    NBA97_INTERRUPT_CAPTURE_83B30,NBA97_INTERRUPT_REMOVE_CDROM_7F598,
    NBA97_INTERRUPT_HOOK_7FB78,NBA97_INTERRUPT_PAD_7F518,
    NBA97_INTERRUPT_COUNTER_7FB88,NBA97_INTERRUPT_RETURN_7FB98
};
enum Nba97InterruptKind {
    NBA97_INTERRUPT_RAM_STORE=0,NBA97_INTERRUPT_DEVICE_READ,NBA97_INTERRUPT_DEVICE_WRITE,
    NBA97_INTERRUPT_ENTER_CRITICAL,NBA97_INTERRUPT_EXIT_CRITICAL,
    NBA97_INTERRUPT_CAPTURE_CONTEXT,NBA97_INTERRUPT_REMOVE_CDROM_DRIVER,
    NBA97_INTERRUPT_HOOK_CONTEXT,NBA97_INTERRUPT_CHANGE_CLEAR_PAD,
    NBA97_INTERRUPT_CHANGE_CLEAR_COUNTER,NBA97_INTERRUPT_RETURN_EXCEPTION,
    NBA97_INTERRUPT_DIAGNOSTIC,NBA97_INTERRUPT_OTHER_DISPATCH,NBA97_INTERRUPT_CALLBACK
};
enum Nba97InterruptCompletion { NBA97_INTERRUPT_LIMIT=-4,NBA97_INTERRUPT_TRANSFERRED=2 };
typedef struct Nba97InterruptEvent {
    enum Nba97InterruptKind kind;
    uint32_t pc,address,width,value,argument[4];
    Nba97SpuTransferValue returned;
    uint8_t completed,transferred;
} Nba97InterruptEvent;
/* Required actual effects through the same retained RAM. Return1 for a
 * returning call, or2 for a completed nonlocal control transfer ONLY from
 * RETURN_EXCEPTION or CALLBACK. The latter ends the entire recovered run;
 * execution never resumes in the interrupted initialization call.
 * Return knownness may be unknown when the original caller does not use it.
 * CALLBACK has no declared arguments: zero argument fields are unavailable
 * placeholders. OTHER_DISPATCH declares channel/callback; diagnostics
 * declare format and the reached values. No cast of source address to host
 * function pointer and no successful substitute for an unowned operation.
 * Callbacks may mutate RAM bytes/knownness, not owner/journal/metadata. */
typedef int (*Nba97InterruptIo)(void*,const Nba97VoicePatlMemory*,
    const Nba97InterruptEvent*,Nba97SpuTransferValue*);
typedef struct Nba97InterruptController {
    Nba97VoicePatlMemory memory;Nba97InterruptIo io;void* user;size_t access_budget;
} Nba97InterruptController;
typedef struct Nba97InterruptProgress {
    size_t accesses,events,stores,callbacks_completed;
    uint32_t stopped_pc,stopped_address;
    Nba97SpuTransferValue returned;
    uint8_t completed,transferred;
} Nba97InterruptProgress;
/* DISPATCH/REGISTER take channel/callback; CLEAR takes address/word count;
 * VBLANK_SET takes callback and incoming_v1 (the disabled original branch
 * returns its incoming v1). Other entry arguments follow their BIOS API.
 * Source guards, unchecked indices, live aliases, callback rereads and raw
 * returns remain intact. Known controller/VBlank/DMA dispatch targets execute
 * their recovered code; other targets require the real platform callback.
 * Access/journal limits and a native 64-level inline callback recursion limit
 * retain a completed prefix, without rollback/resume. Incoming_v1 metadata
 * is consumed only on the disabled VBLANK_SET branch.
 * Active-stack/source-code aliases, concurrent metadata changes and overlap
 * between RAM and owner/journal/progress storage are outside this contract. */
int nba97_interrupt_controller(Nba97InterruptController*,enum Nba97InterruptOperation,
    uint32_t a0,uint32_t a1,Nba97SpuTransferValue incoming_v1,
    Nba97InterruptEvent*,size_t,Nba97InterruptProgress*);
#ifdef __cplusplus
}
#endif
#endif
