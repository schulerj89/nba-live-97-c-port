#ifndef NBA97_SPU_TRANSFER_H
#define NBA97_SPU_TRANSFER_H
#include "voice_patl_upload.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97SpuTransferOperation {
    NBA97_SPU_TRANSFER_7DC90=0,NBA97_SPU_CONTROL_7D9E8,
    NBA97_SPU_PIO_7D334,NBA97_SPU_TEST_EVENT_7F568,
    NBA97_SPU_ISR_7D668,NBA97_SPU_DELIVER_EVENT_7F508
};
enum Nba97SpuTransferEventKind {
    NBA97_SPU_TRANSFER_RAM_STORE=0,NBA97_SPU_TRANSFER_DEVICE_READ,
    NBA97_SPU_TRANSFER_DEVICE_WRITE,NBA97_SPU_TRANSFER_DIAGNOSTIC_83B20,
    NBA97_SPU_TRANSFER_TEST_EVENT_B0_0B,NBA97_SPU_TRANSFER_CALLBACK_7D668,
    NBA97_SPU_TRANSFER_DELIVER_EVENT_B0_07
};
enum Nba97SpuTransferCompletion { NBA97_SPU_TRANSFER_LIMIT=-4 };
typedef struct Nba97SpuTransferValue { uint32_t word;uint8_t known; } Nba97SpuTransferValue;
typedef struct Nba97SpuTransferEvent {
    enum Nba97SpuTransferEventKind kind;
    uint32_t pc,address,width,value,argument[2];
    Nba97SpuTransferValue returned;
    uint8_t completed;
} Nba97SpuTransferEvent;
/* Required actual device access, diagnostic or native event operation. Device
 * events identify reached source pointer accesses, not a guessed hardware
 * address range; the platform must resolve any retained-memory alias through
 * this SAME registry. Reads return known register bits. A diagnostic's unused
 * return may remain unknown; the final PIO diagnostic return stays explicit.
 * Event testing must use the registered event's real pending/consumed state,
 * not WinMM activity, a DMA-start flag, or unconditional successful polling.
 * The ISR callback event uses address=the live encoded callback and carries
 * the source a0 residue F0000000 in argument[0]. It has no declared arguments;
 * argument[1] is unavailable (zero placeholder), not a recovered CPU value.
 * DeliverEvent carries class/spec in argument[0/1]. No callback is optional.
 * Callback may synchronously mutate retained bytes/knownness and device state,
 * but not metadata, context, progress or journal. Return1 only when executed. */
typedef int (*Nba97SpuTransferIo)(void*,const Nba97VoicePatlMemory*,
    const Nba97SpuTransferEvent*,Nba97SpuTransferValue*);
typedef struct Nba97SpuTransfer {
    Nba97VoicePatlMemory memory;
    Nba97SpuTransferIo io;
    void* user;
    size_t access_budget;
} Nba97SpuTransfer;
typedef struct Nba97SpuTransferProgress {
    size_t accesses,events,stores,callbacks_completed;
    uint32_t stopped_pc,stopped_address;
    Nba97SpuTransferValue returned;
    uint8_t completed;
} Nba97SpuTransferProgress;
/* Original FEONLY 7DC90/7D9E8/7D334/7F568/7D668/7F508:519 PCs. TRANSFER and PIO use
 * a0=CPU source,a1=byte count. CONTROL uses a0=command,a1/a2=varargs.
 * TEST_EVENT uses a0=the actual registered handle. DELIVER_EVENT uses a0/a1
 * as class/spec. ISR has no declared arguments. No implicit initialization.
 * ISR execution must follow actual transfer completion and dispatch; this
 * owner does not synthesize an interrupt, register it or complete a transfer.
 * Source timeouts are source returns, distinct from native refused callbacks.
 * Wrapper7DC90 still ignores command timeout returns and reports its count.
 * DMA block rounding, odd PIO halfword reads, and ordered partial effects stay
 * original. Stack-only delay loops are not a native wall-clock timing model.
 * Active stack/code aliases are excluded. A returned value is meaningful only
 * on completion; known0 is explicitly unavailable, not invented zero.
 * Native limits/refusals preserve their completed prefix without rollback or
 * resumability. Journal/context/progress cannot alias mapped storage. */
int nba97_spu_transfer(Nba97SpuTransfer*,enum Nba97SpuTransferOperation,
    uint32_t a0,uint32_t a1,uint32_t a2,Nba97SpuTransferEvent*,size_t capacity,
    Nba97SpuTransferProgress*);
#ifdef __cplusplus
}
#endif
#endif
