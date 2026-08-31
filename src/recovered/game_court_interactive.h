#ifndef NBA97_GAME_COURT_INTERACTIVE_H
#define NBA97_GAME_COURT_INTERACTIVE_H
#include "game_font_loader.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97CourtInteractiveValue {uint32_t word;uint8_t known;} Nba97CourtInteractiveValue;
typedef struct Nba97CourtInteractiveEvent {
    uint32_t pc,address,value,entry,argument[5];
    Nba97CourtInteractiveValue returned;
    uint8_t kind,width,argument_count,completed; /* kind0=store,1=required service */
} Nba97CourtInteractiveEvent;
/* Required real synchronous source calls, never automated input or success
 * stubs:8F224 input/clock,29BFC loader/retry/heap,994F4 sync,90698 release,
 * 99CA4/99ACC display/draw environment,29BDC->A9CC0,AA0BC clear environment,
 * and946B8 image upload. Upload must run the existing full native image owner
 * and actual transfer backend. Return1 only after source effects complete.
 * LOAD must return its actual known nonzero payload; INPUT its actual known
 * result. Other returns are unused and may remain unknown. Event pointers may
 * not escape; metadata/context/journal/progress stay fixed. Retained contents
 * and service state may change synchronously. Refused calls retain effects. */
typedef int (*Nba97CourtInteractiveIo)(void*,const Nba97GameTextMemory*,
    const Nba97CourtInteractiveEvent*,Nba97CourtInteractiveValue*);
typedef struct Nba97CourtInteractiveContext {
    Nba97GameTextMemory memory;size_t access_budget;
    Nba97CourtInteractiveIo io;void* user;
} Nba97CourtInteractiveContext;
typedef struct Nba97CourtInteractiveProgress {
    size_t accesses,events,stores,services_completed,frames_completed,players_completed;
    uint32_t stopped_pc,stopped_address,loaded_resource;
    Nba97GameFontProgress shpp;
    uint8_t interactive_entered,completed;
} Nba97CourtInteractiveProgress;
enum {NBA97_COURT_INTERACTIVE_CONTROL_TARGET=-100};
/* Complete479B8 interval47CB8..484B8: both ordinary resets and the entire
 * controller-driven zcheat.psh loop. Reuses existing nativefont SHPPcount/entry
 * owners; does not copy or reimplement those owners. Actual scratch18==20
 * AND initialinput==E75 enter the loop; only actualouterinput==820 exits it.
 * Native budgets bound execution, never choose inputs, skip rendering, supply
 * resource bytes or silently free on refusal. Count is live; firstheader byte
 * is written as a wholeword. Digit tables260E4/2610C are live source pointers.
 * Their ten original branch destinations are supported per table; other
 * targets refuse at the JR, not a claim that the original would refuse.
 * Signed image widths are reread AFTER upload; player roots/record bytes and
 * scratchflags retain every original reread. Rawbyte200..255 still uses the
 * source's fixed hundreds glyph1 before laterclamping. Increment/decrement
 * stores wrap to8bits BEFORE clamps. Specialselector writes are not bypassed.
 *
 * Existing textmemory contract applies: exact provenance, disjoint source
 * regions, possible native backing aliases, canonical reached knowledge,
 * fixed metadata/lifetimes. Context/progress/journal cannot overlap memory or
 * eachother. Source code/stack aliases excluded. SHPP access counts and refusal
 * semantics follow the existing fonthelper; no extra preflight is imposed.
 * All prefixes survive refusal. Not resumable or transactional; stage memory,
 * input/device/heap/VRAM owners together for atomic host publication.
 * Completion ends BEFORE484B8's sync and player-packet patch interval; it is
 * not whole479B8, realservice implementation, or naturalmatchentry.
 */
int nba97_game_court_interactive(Nba97CourtInteractiveContext*,
    Nba97CourtInteractiveEvent*,size_t capacity,Nba97CourtInteractiveProgress*);
#ifdef __cplusplus
}
#endif
#endif
