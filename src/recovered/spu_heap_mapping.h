#ifndef NBA97_SPU_HEAP_MAPPING_H
#define NBA97_SPU_HEAP_MAPPING_H
#include "spu_heap.h"
#include "voice_mapping.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97SpuHeapMappingProgress {
    size_t accesses,stores,operations_completed;
    enum Nba97SpuHeapOperation last_operation;
    int completion;
    Nba97SpuHeapProgress last;
} Nba97SpuHeapMappingProgress;
typedef struct Nba97SpuHeapMapping {
    Nba97VoiceMappingInvoke platform;
    void* platform_context;
    size_t access_budget;
    Nba97SpuHeapStore* journal;
    size_t journal_capacity;
    Nba97SpuHeapMappingProgress progress;
} Nba97SpuHeapMapping;

/* Install as Nba97VoiceMapping.call, with this bridge as its context. Only
 * ALLOCATE_7EC2C/FREE_7E56C execute here, through the actual SPU heap CPU owner
 * and the registry supplied by the mapping caller. No duplicate SDK state.
 * All other operations require platform; NULL refuses, never fabricates a
 * transfer, event, stream or service result. Platform receives the exact same
 * registry/event/result pointers, and follows the mapping callback contract.
 *
 * Zero progress before a new outer operation. Budgets and the journal apply
 * cumulatively across its heap callbacks, including failed partial callbacks.
 * Do not reset progress to resume a refused source operation. Retained memory
 * effects are not rolled back. Detailed heap refusal remains in progress even
 * though the mapping callback ABI reports it as an unavailable lower call.
 * Bridge/config/progress/journal storage cannot alias mapped bytes and must
 * remain stable during callbacks. A bridge is not concurrently reusable. */
int nba97_spu_heap_mapping_invoke(void*,const Nba97VoicePatlMemory*,
    const Nba97VoiceMappingEvent*,uint32_t* result);
#ifdef __cplusplus
}
#endif
#endif
