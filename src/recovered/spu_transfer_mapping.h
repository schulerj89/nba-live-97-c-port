#ifndef NBA97_SPU_TRANSFER_MAPPING_H
#define NBA97_SPU_TRANSFER_MAPPING_H
#include "spu_transfer.h"
#include "voice_mapping.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97SpuTransferMappingProgress {
    size_t accesses,events,operations_completed;
    enum Nba97SpuTransferOperation last_operation;
    int completion;
    Nba97SpuTransferProgress last;
} Nba97SpuTransferMappingProgress;
typedef struct Nba97SpuTransferMapping {
    Nba97SpuTransferIo io;
    void* io_context;
    Nba97VoiceMappingInvoke platform;
    void* platform_context;
    size_t access_budget;
    Nba97SpuTransferEvent* journal;
    size_t journal_capacity;
    Nba97SpuTransferMappingProgress progress;
} Nba97SpuTransferMapping;
/* Install after the SPU heap bridge, as its platform callback. Reached
 * TRANSFER/TEST_EVENT execute the actual recovered CPU owner in the SAME
 * caller registry. Other operations require platform and forward unchanged.
 * io must own device/sample/event operations; this bridge does not copy DMA
 * bytes, invent registration, advance a scheduler or deliver completion.
 * A final unknown source return refuses because mapping must branch on it.
 * Zero progress for a new outer operation only. Access/journal limits are
 * cumulative across callbacks, including refused prefixes, without rollback
 * or resumability. Context/progress/journal must not alias mapped storage. */
int nba97_spu_transfer_mapping_invoke(void*,const Nba97VoicePatlMemory*,
    const Nba97VoiceMappingEvent*,uint32_t* result);
#ifdef __cplusplus
}
#endif
#endif
