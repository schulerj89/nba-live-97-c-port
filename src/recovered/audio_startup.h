#ifndef NBA97_AUDIO_STARTUP_H
#define NBA97_AUDIO_STARTUP_H
#include "spu_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97AudioStartupOperation {
    NBA97_AUDIO_STARTUP_700B0=0,NBA97_AUDIO_STARTUP_COMMON_7DEA8,
    NBA97_AUDIO_STARTUP_RESET_73A68,NBA97_AUDIO_STARTUP_REGISTER_8E0E0
};
enum Nba97AudioStartupKind {
    NBA97_AUDIO_STARTUP_RAM_STORE=0,NBA97_AUDIO_STARTUP_PARAMETER_STORE,
    NBA97_AUDIO_STARTUP_DEVICE_READ,NBA97_AUDIO_STARTUP_DEVICE_WRITE,
    NBA97_AUDIO_STARTUP_INITIALIZE,NBA97_AUDIO_STARTUP_HEAP
};
enum Nba97AudioStartupCompletion { NBA97_AUDIO_STARTUP_LIMIT=-4,NBA97_AUDIO_STARTUP_TRANSFERRED=2 };
typedef struct Nba97AudioStartupEvent {
    enum Nba97AudioStartupKind kind;
    uint32_t pc,address,width,value,argument[2];
    Nba97SpuTransferValue returned;
    uint8_t completed,transferred;
} Nba97AudioStartupEvent;
/* INITIALIZE calls 7E6EC without declared arguments; HEAP calls 7E940 with
 * count/address. Device effects use the same retained native device owners.
 * Return 2 only for INITIALIZE's terminal nonlocal controller transfer.
 * Unused argument zeros are placeholders, not original register values.
 * Callbacks may change RAM/knownness, but not metadata or owner/journal/progress.
 * PARAMETER_STORE is a journal-only local-block offset, never a source address. */
typedef int (*Nba97AudioStartupIo)(void*,const Nba97VoicePatlMemory*,
    const Nba97AudioStartupEvent*,Nba97SpuTransferValue*);
typedef struct Nba97AudioStartup {
    Nba97VoicePatlMemory memory;Nba97AudioStartupIo io;void* user;size_t access_budget;
} Nba97AudioStartup;
typedef struct Nba97AudioStartupProgress {
    size_t accesses,events,stores,callbacks_completed;
    uint32_t stopped_pc,stopped_address;
    Nba97SpuTransferValue returned;
    uint8_t completed,transferred,stopped_local;
} Nba97AudioStartupProgress;
/* FEONLY 383 PCs. COMMON takes a0=parameter address; REGISTER takes callback.
 * STARTUP ignores a0 and owns a private 40-byte parameter block; only actual
 * source stores make bytes known. No invented FP/source address or zero fill.
 * COMMON reads live jump tables and supports their eight same-side case-entry
 * targets (including default); other encoded targets explicitly refuse after
 * the retained read prefix. Active stack/code aliases are outside this domain.
 * Original unchecked callback-table fullness, early guard publication, and
 * ignored lower raw returns are preserved. No timer is invoked by registration.
 * Return 1 ordinary, 2 terminal transferred; both completed. Limits/refusals
 * retain prefixes, with no rollback/resume. stopped_local distinguishes a local
 * parameter offset from an absolute address. Mapped RAM must not overlap owner,
 * journal/progress or their metadata. Unused incoming registers are unavailable. */
int nba97_audio_startup(Nba97AudioStartup*,enum Nba97AudioStartupOperation,
    uint32_t a0,Nba97AudioStartupEvent*,size_t,Nba97AudioStartupProgress*);
#ifdef __cplusplus
}
#endif
#endif
