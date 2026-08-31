#ifndef NBA97_GAME_HEAP_INITIALIZE_H
#define NBA97_GAME_HEAP_INITIALIZE_H
#include "game_text_objects.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameHeapInitializeArguments {
    uint32_t descriptor_count,arena,arena_size,gp;
} Nba97GameHeapInitializeArguments;
typedef struct Nba97GameHeapBankArguments {
    uint32_t name,flags,begin,end,alignment,alternate_alignment,reclaim,guard;
} Nba97GameHeapBankArguments;
enum Nba97GameHeapInitializeEventKind {
    NBA97_HEAP_INITIALIZE_STORE=0,NBA97_HEAP_INITIALIZE_FORMAT_9CB7C=1
};
typedef struct Nba97GameHeapInitializeEvent {
    uint32_t pc,address,value,argument[3];
    uint8_t kind,width,completed;
} Nba97GameHeapInitializeEvent;
/* Required actual9CB7C(destination,format,name) formatting operation, including
 * its retained-memory mutations. Its return value is unused. Acknowledgment
 * is not permission to skip the call or invent an empty descriptor name.
 * Callbacks may change retained bytes/knownness synchronously, including live
 * list/global aliases. Mappings/lifetimes/metadata remain fixed; journal and
 * progress storage must not alias mapped bytes or be modified by callbacks. */
typedef int (*Nba97GameHeapInitializeIo)(void*,const Nba97GameTextMemory*,
    const Nba97GameHeapInitializeEvent*);
typedef struct Nba97GameHeapInitializeContext {
    Nba97GameTextMemory memory;
    size_t access_budget;
    Nba97GameHeapInitializeIo io;
    void* user;
} Nba97GameHeapInitializeContext;
typedef struct Nba97GameHeapInitializeProgress {
    size_t accesses,events,stores,callbacks_completed;
    uint32_t stopped_pc,stopped_address,heap_bank,return_v0;
    uint8_t completed;
} Nba97GameHeapInitializeProgress;

/* Complete8FA6C/8FB4C,90CE4/90D40,A4048/A405C/A4088 (169 original PCs).
 * No fabricated source addresses, allocation, blanket zeroing, BIOS or formatter
 * implementation. GP is an explicit proven incoming register value: A4048
 * writes gp+274 but returns the literal D7C3C, even when they differ.
 * Ordinary299C8 inputs:220 descriptors,8010B61C arena,801FD800-arena size;
 * actual948A4/A8 establishes gp=800D79C8. These are provenance obligations,
 * not defaults supplied by the API. Payload begins at8010D87C, not rounded up.
 * Only reached memory accesses validate knownness/alignment. Untouched
 * descriptor fields and all payload bytes retain their incoming state.
 * Source regions cannot overlap; native storage aliases follow textmemory.
 * Code/active-stack aliases are excluded. Arithmetic wraps32 bits; a zero or
 * negative descriptor count still writes one final link. Empty free-list
 * pops dereference NULL+20, and formatter failures retain prior source effects.
 * Native limits/refusals preserve the completed prefix, without rollback or
 * resumability. Stage all mapped and callback state before atomic publication.
 * Returns NBA97_TEXT_*; return_v0 is meaningful only on completion. */
int nba97_game_heap_initialize(Nba97GameHeapInitializeContext*,
    const Nba97GameHeapInitializeArguments*,Nba97GameHeapInitializeEvent*,size_t,
    Nba97GameHeapInitializeProgress*);
int nba97_game_heap_initialize_bank(Nba97GameHeapInitializeContext*,
    const Nba97GameHeapBankArguments*,Nba97GameHeapInitializeEvent*,size_t,
    Nba97GameHeapInitializeProgress*);
#ifdef __cplusplus
}
#endif
#endif
