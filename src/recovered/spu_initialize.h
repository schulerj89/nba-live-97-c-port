#ifndef NBA97_SPU_INITIALIZE_H
#define NBA97_SPU_INITIALIZE_H
#include "spu_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97SpuInitializeOperation {
    NBA97_SPU_INITIALIZE_7E6EC=0,NBA97_SPU_INITIALIZE_MODE_7E3FC,
    NBA97_SPU_INITIALIZE_HARDWARE_7CE18,NBA97_SPU_INITIALIZE_CONTROLLER_7F5D0,
    NBA97_SPU_INITIALIZE_REGISTER_7DD80
};
enum Nba97SpuInitializeKind {
    NBA97_SPU_INITIALIZE_RAM_STORE=0,NBA97_SPU_INITIALIZE_DEVICE_READ,
    NBA97_SPU_INITIALIZE_DEVICE_WRITE,NBA97_SPU_INITIALIZE_DIAGNOSTIC,
    NBA97_SPU_INITIALIZE_CONTROLLER,NBA97_SPU_INITIALIZE_EVENTS,
    NBA97_SPU_INITIALIZE_PIO
};
enum Nba97SpuInitializeCompletion { NBA97_SPU_INITIALIZE_LIMIT=-4,NBA97_SPU_INITIALIZE_TRANSFERRED=2 };
typedef struct Nba97SpuInitializeEvent {
    enum Nba97SpuInitializeKind kind;
    uint32_t pc,address,width,value,argument[2];
    Nba97SpuTransferValue returned;
    uint8_t completed,transferred;
} Nba97SpuInitializeEvent;
/* Required device/lower-owner effects through the SAME retained registry.
 * CONTROLLER's address is the live table target; it has no declared arguments.
 * EVENTS has no arguments. PIO declares source/count; DIAGNOSTIC format/message.
 * Zero unused arguments are unavailable placeholders, not CPU register values.
 * Return1 for a returning effect. Return2 ONLY for a CONTROLLER nonlocal transfer
 * ends this entire run, without executing sound initialization after an IRQ.
 * Unknown unused returns remain unknown. No encoded-address function casts.
 * Callbacks may mutate RAM/knownness but not metadata, owner, progress or journal. */
typedef int (*Nba97SpuInitializeIo)(void*,const Nba97VoicePatlMemory*,
    const Nba97SpuInitializeEvent*,Nba97SpuTransferValue*);
typedef struct Nba97SpuInitialize {
    Nba97VoicePatlMemory memory;Nba97SpuInitializeIo io;void* user;size_t access_budget;
} Nba97SpuInitialize;
typedef struct Nba97SpuInitializeProgress {
    size_t accesses,events,stores,callbacks_completed;
    uint32_t stopped_pc,stopped_address;
    Nba97SpuTransferValue returned;
    uint8_t completed,transferred;
} Nba97SpuInitializeProgress;
/* Original FEONLY five routines,415 PCs. MODE/HARDWARE take a0=mode;
 * INITIALIZE always selects mode0. REGISTER takes index/value/shift-enable.
 * Original unbounded index arithmetic, live pointer aliases, ignored timeouts,
 * redundant device reads and partial writes are retained. Private stack delay
 * loops do not establish native elapsed time. The original16 source bytes are
 * read by the required PIO owner, not supplied or zero-filled here.
 * Returns1 on ordinary completion,2 on a completed controller transfer; both
 * set completed. Only ordinary completion clears the stopped location.
 * Limits/refusals retain a prefix, with no rollback/resume. Active stack/code
 * aliases and overlap of mapped RAM with owner/journal/progress are excluded. */
int nba97_spu_initialize(Nba97SpuInitialize*,enum Nba97SpuInitializeOperation,
    uint32_t a0,uint32_t a1,uint32_t a2,Nba97SpuInitializeEvent*,size_t,
    Nba97SpuInitializeProgress*);
#ifdef __cplusplus
}
#endif
#endif
