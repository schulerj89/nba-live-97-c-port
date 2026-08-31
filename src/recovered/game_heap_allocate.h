#ifndef NBA97_GAME_HEAP_ALLOCATE_H
#define NBA97_GAME_HEAP_ALLOCATE_H
#include "game_text_objects.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameHeapValue {uint32_t word;uint8_t known;} Nba97GameHeapValue;
typedef struct Nba97GameHeapArguments {
    uint32_t name,size,flags,unused_argument3;
} Nba97GameHeapArguments;
enum Nba97GameHeapEventKind {
    NBA97_HEAP_STORE=0,NBA97_HEAP_BIOS_A0_1A=1,NBA97_HEAP_RECLAIM_A3074=2
};
typedef struct Nba97GameHeapEvent {
    uint32_t pc,address,value,argument[3];
    Nba97GameHeapValue returned;
    uint8_t kind,width,completed;
} Nba97GameHeapEvent;
/* BIOS_A0_1A: actual BIOS strncpy(destination,basename,12), including its
 * actual byte mutations. No BIOS-ROM footprint/overlap/implicitzeroing claim
 * is made here. The returned value is unused; it may remain unknown.
 * RECLAIM_A3074: actual reclamation/relocation(flags), returnedv0 determines
 * retry. Successful acknowledgment requires completed sourcecalleeeffects,
 * not a fabricated successful value. NULL callback refuses both boundaries.
 * Memory mappings/metadata/lifetimes stay fixed; retained bytes/knownness can
 * mutate synchronously. Do not retain event/returned pointers or alter the
 * journal/progress. Acknowledged unknown reclaim result refuses at its branch.
 */
typedef int (*Nba97GameHeapIo)(void*,const Nba97GameTextMemory*,
    const Nba97GameHeapEvent*,Nba97GameHeapValue* returned);
typedef struct Nba97GameHeapContext {
    Nba97GameTextMemory memory;
    size_t access_budget; /* finite native bound on reached CPUdataaccesses */
    Nba97GameHeapIo io;
    void* user;
} Nba97GameHeapContext;
typedef struct Nba97GameHeapProgress {
    size_t accesses,events,stores,callbacks_completed;
    uint32_t stopped_pc,stopped_address,heap_context,aligned_size;
    Nba97GameHeapValue descriptor;
    uint8_t completed;
} Nba97GameHeapProgress;

/* Complete9027C plusA7098basename,90D40descriptorpop,A54BC/AA06Cguard and
 * 9D93C BIOSthunk; actual BIOS and A3074 remain explicit synchronous callees.
 * Caller supplies proven original-address mappings for heap contexts103D50,
 * descriptorfreeheadEB688, serialC4A8C, sentinels/lists/name/ownedarenas.
 * No initialization, fabricated heapaddresses, hostmalloc or zero-fill.
 * Flags20 means reverse search;40 selects context+C alignmentmask instead
 * of+8 for size, also aligning the search positions. All arithmetic wraps32
 * bits; gap sizes are compared SIGNED, addresses UNSIGNED, masks unchecked.
 * No NULL descriptor check or linked-list consistency repair is inserted.
 * Live rereads and ordered stores preserve source aliases and partialprefix.
 *
 * Sourcecode/stack aliases excluded; incoming arguments are known rawvalues.
 * Source regions cannot overlap; native aliases follow textmemorycontract.
 * Only reached knownness is checked, including every byte of each access.
 * No implicitvalidation of an arena span merelybecause its address is stored.
 * Journal capacity is checked before every ownedstore/callback. Return uses
 * NBA97_TEXT_* results. A stopped prefix is not resumable or rolledback.
 * Stage CPUmemory plus callbackowners and publishonlycomplete for atomicity.
 */
int nba97_game_heap_allocate(Nba97GameHeapContext*,const Nba97GameHeapArguments*,
    Nba97GameHeapEvent*,size_t capacity,Nba97GameHeapProgress*);
#ifdef __cplusplus
}
#endif
#endif
