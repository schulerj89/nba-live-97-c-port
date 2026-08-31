#ifndef NBA97_SPU_HEAP_H
#define NBA97_SPU_HEAP_H
#include "voice_patl_upload.h"
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97SpuHeapOperation {
    NBA97_SPU_HEAP_INITIALIZE_7E940=0,
    NBA97_SPU_HEAP_ALLOCATE_7EC2C,
    NBA97_SPU_HEAP_FREE_7E56C,
    NBA97_SPU_HEAP_MAINTAIN_7EF44
};
enum Nba97SpuHeapCompletion { NBA97_SPU_HEAP_LIMIT=-4 };
typedef struct Nba97SpuHeapStore { uint32_t pc,address,value; } Nba97SpuHeapStore;
typedef struct Nba97SpuHeap {
    Nba97VoicePatlMemory memory;
    size_t access_budget;
} Nba97SpuHeap;
typedef struct Nba97SpuHeapProgress {
    size_t accesses,stores;
    uint32_t stopped_pc,stopped_address,return_v0;
    uint8_t completed;
} Nba97SpuHeapProgress;

/* Complete FEONLY 7E940/7EC2C/7E56C/7EF44, 422 original instructions.
 * Initialize takes a0=descriptor count,a1=actual descriptor storage address.
 * Allocate takes a0=byte count; free takes a0=encoded SPU address. Other args
 * are unused. SDK globals and descriptors live in the SAME retained registry
 * as the audio upload owner. No default SDK state or invented addresses.
 * This CPU allocator manages descriptor bytes only; it does not write SPU RAM,
 * start a transfer, signal a hardware event, or synthesize playback success.
 * No lower callback remains in these four functions. Native reads/writes use
 * the canonical PATl memory contract, with a reached access budget and ordered
 * store journal. Return codes are NBA97_PATL_* plus NBA97_SPU_HEAP_LIMIT.
 * return_v0 is the original raw register value, meaningful on completion only.
 * Free/maintenance do not replace it with a fabricated conventional status.
 *
 * All source arithmetic wraps32; signed comparisons and arithmetic shifts are
 * explicit. Source counts, masks, free/tail flags, tombstones and aliasing are
 * not repaired. Init stores the supplied count, not count-1, and leaves unused
 * descriptors incoming. Stores beyond an actual owned span refuse at the
 * reached instruction, not at an invented earlier capacity check.
 * Refusals preserve their completed prefix, without rollback or resumability.
 * Metadata/storage lifetimes stay fixed; context/progress/journal cannot alias
 * mapped bytes. Source code and active stack aliases are excluded. */
int nba97_spu_heap(Nba97SpuHeap*,enum Nba97SpuHeapOperation,uint32_t a0,uint32_t a1,
    Nba97SpuHeapStore*,size_t capacity,Nba97SpuHeapProgress*);
#ifdef __cplusplus
}
#endif
#endif
