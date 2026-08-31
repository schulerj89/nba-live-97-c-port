#ifndef NBA97_GAME_TEXT_POOLS_H
#define NBA97_GAME_TEXT_POOLS_H
#include "game_text_objects.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameTextPoolValue {uint32_t word;uint8_t known;} Nba97GameTextPoolValue;
typedef struct Nba97GameTextPoolArguments {
    uint32_t mode,glyph_capacity,font_count,text_capacity,id_capacity;
    uint32_t name,unused_argument6,packet_capacity;
} Nba97GameTextPoolArguments;
enum Nba97GameTextPoolEventKind {NBA97_TEXT_POOL_STORE=0,NBA97_TEXT_POOL_ALLOCATE_9027C=1};
typedef struct Nba97GameTextPoolEvent {
    uint32_t pc,address,value;
    uint32_t argument[4]; /*9027C: originalname, wrappedsize,flags20,mode1. */
    Nba97GameTextPoolValue returned; /*9027C returns a descriptor, not its word0. */
    uint8_t kind,width,completed;
} Nba97GameTextPoolEvent;

/* Actual9027C allocation boundary; return1 ONLY after the requested operation
 * completes. No callback means IO_REFUSED. Return known=1 with the actual
 * original descriptor address, including zero on actual source failure.
 * Returning a fabricated address/zero-filled allocation is not completion.
 * The callback must own9027C's heap search, descriptor/free-list/name/growth
 * effects, not just malloc bytes. Flag20 selects the source's reverse heap
 * search; it is NOT a zero-fill flag. Original90160 then reads descriptorword0
 * even for a NULL descriptor; no fallback/null check is inserted.
 * Byte/knownness mutations are synchronous. Regions cover actual owned arenas
 * and globals beforehand; mappings/metadata/lifetimes stay fixed during call.
 * No callback may retain event/returned pointers or mutate journal/progress. */
typedef int (*Nba97GameTextPoolIo)(void*,const Nba97GameTextMemory*,
    const Nba97GameTextPoolEvent*,Nba97GameTextPoolValue* descriptor);
typedef struct Nba97GameTextPoolContext {
    Nba97GameTextMemory memory;
    Nba97GameTextPoolIo io;
    void* user;
} Nba97GameTextPoolContext;
typedef struct Nba97GameTextPoolProgress {
    size_t events,stores,callbacks_completed;
    uint32_t stopped_pc,stopped_address,requested_size,return_v0;
    Nba97GameTextPoolValue allocation_descriptor,style;
    uint8_t completed;
} Nba97GameTextPoolProgress;

/* Complete2E200 plus actual90160/901EC andA405C/A4088 wrappers (192insns).
 * Live1029C0 is loaded before/after9027C; actual lock-word1/0 writes preserve
 * aliases and changed lock pointers. Unknown allocator returns are retained
 * through post-call unlock, then refuse at90170's required descriptor read.
 * Source arguments0/2/3 are low8 where used;1/4/7 use signedlow16 sizes.
 * Argument6 is UNUSED. Size/address arithmetic wraps32 bits. No implicit
 * initialization of untouched colors, descriptors, packet bytes or spacing.
 * All loop base-pointer words are reloaded live. B2048 is published BEFORE
 * style+20 and final bitmap clears; refusal may leave that source prefix.
 *
 * Use NBA97_TEXT_* results. Journal includes all owned CPUstores and allocator
 * dispatch/result; callback-internal mutations are its owner's responsibility.
 * Capacity checked before every store/call; finite low8/low16 source loops.
 * Stage the complete memory/allocator owner for host atomicity and publish
 * only completion. A stopped prefix is not resumable and is not rolled back.
 * Source addresses require caller provenance; native pointers are never cast
 * into source values. Retained regions cannot overlap in SOURCE address space;
 * native storage aliases are permitted as in game_text_objects.h. Original
 * code/stack aliases are outside this boundary; incoming args are known raw
 * values, rather than fabricated reads of an unowned stack. */
int nba97_game_text_pools(Nba97GameTextPoolContext*,const Nba97GameTextPoolArguments*,
    Nba97GameTextPoolEvent* journal,size_t capacity,Nba97GameTextPoolProgress*);
#ifdef __cplusplus
}
#endif
#endif
