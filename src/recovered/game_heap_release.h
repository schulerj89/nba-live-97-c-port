#ifndef NBA97_GAME_HEAP_RELEASE_H
#define NBA97_GAME_HEAP_RELEASE_H
#include "game_text_objects.h"
#ifdef __cplusplus
extern "C" {
#endif
enum Nba97GameHeapReleaseOperation {
    NBA97_HEAP_FIND_90618=0,NBA97_HEAP_RELEASE_PAYLOAD_90698,
    NBA97_HEAP_RELEASE_DESCRIPTOR_906C4,NBA97_HEAP_UNLINK_90714
};
typedef struct Nba97GameHeapReleaseValue {uint32_t word;uint8_t known;} Nba97GameHeapReleaseValue;
typedef struct Nba97GameHeapReleaseStore {uint32_t pc,address,value;} Nba97GameHeapReleaseStore;
typedef struct Nba97GameHeapReleaseContext {
    Nba97GameTextMemory memory;
    size_t access_budget;
} Nba97GameHeapReleaseContext;
typedef struct Nba97GameHeapReleaseProgress {
    size_t accesses,stores;
    uint32_t stopped_pc,stopped_address,descriptor;
    Nba97GameHeapReleaseValue returned;
    uint8_t completed;
} Nba97GameHeapReleaseProgress;
/* Complete90618/90698/906C4/90714/90D28 andA405C/A4088:90 original PCs.
 * No required lower calls remain in this CPU owner. Original heap contexts,
 * descriptors, free head and lock still require proven retained mappings.
 * FIND/PAYLOAD search all16 banks for the exact payload, not pointer-header.
 * DESCRIPTOR invokes the original lock wrapper; UNLINK does not. Sentinel
 * flags, neighbor links and source counts are not repaired. All pointer
 * arithmetic wraps32; source alignment and reached bytes remain explicit.
 * No payload wipe or host free occurs. Actual freed descriptor word0 becomes
 * zero, and its next link joins the free list; other fields stay incoming.
 * The NULL PAYLOAD/DESCRIPTOR branches leave incomingv0 untouched, including
 * its unknownness. Callers must not invent a zero return for those branches.
 * Native access/journal bounds and unknown reads retain their completed prefix;
 * they are not source failure branches, resumable state, or automatic rollback.
 * Returned value is meaningful only on completion. Journal/progress/context
 * storage cannot alias mapped bytes. Metadata/lifetimes stay fixed.
 * Source regions must not overlap; native aliases
 * follow textmemory. Code/active-stack aliases are excluded. Stage all affected
 * retained state together when the host requires atomic publication. */
int nba97_game_heap_release(Nba97GameHeapReleaseContext*,enum Nba97GameHeapReleaseOperation,
    uint32_t address,Nba97GameHeapReleaseValue incomingv0,Nba97GameHeapReleaseStore*,
    size_t capacity,Nba97GameHeapReleaseProgress*);
#ifdef __cplusplus
}
#endif
#endif
