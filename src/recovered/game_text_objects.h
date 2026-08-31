#ifndef NBA97_GAME_TEXT_OBJECTS_H
#define NBA97_GAME_TEXT_OBJECTS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Retained source allocations. The base is an explicit original address, never
 * synthesized from a native pointer. Regions must not overlap in source address
 * space; native storage/knownness may alias. Metadata and lifetimes stay fixed.
 * known=NULL means all bytes known; otherwise one canonical0/1 per byte. */
typedef struct Nba97GameTextRegion {
    uint32_t base;
    uint8_t* data;
    uint8_t* known;
    size_t size;
} Nba97GameTextRegion;
typedef struct Nba97GameTextMemory {
    Nba97GameTextRegion* region;
    size_t count;
} Nba97GameTextMemory;
enum Nba97GameTextResult {
    NBA97_TEXT_COMPLETE=1,NBA97_TEXT_ARGUMENT=0,NBA97_TEXT_RESOURCE=-1,
    NBA97_TEXT_UNKNOWN=-2,NBA97_TEXT_ALIGNMENT_TRAP=-3,
    NBA97_TEXT_LIMIT=-4,NBA97_TEXT_IO_REFUSED=-5
};
enum Nba97GameTextEventKind {
    NBA97_TEXT_DIAGNOSTIC_99960=1,NBA97_TEXT_PACKET_CLEAR_DISPATCH
};
typedef struct Nba97GameTextEvent {
    int kind;
    uint32_t target,object,count;
} Nba97GameTextEvent;
/* Actual99960 diagnostic / SDK dispatch boundary. For CLEAR, target is loaded
 * from currentC55B8+2C (originally9A97C). Implement actual effects or refuse;
 * there is no successful default. Return1 acknowledges completion, not rawSDK
 * return. Callback may mutate memory synchronously; no retained event pointer.
 * 99960 overwrites objectword with000C567C AFTER successfuldispatch, regardless
 * of the source SDK return. DMA/diagnostics/timeouts are not owned here. */
typedef int (*Nba97GameTextIo)(void*,const Nba97GameTextEvent*);
typedef struct Nba97GameTextProgress {
    size_t steps,callbacks_completed,objects_reset,glyphs_written;
    uint32_t stopped_address;
    uint8_t stopped_in_text; /* 1: native span offset, not an originaladdress. */
} Nba97GameTextProgress;
typedef struct Nba97GameTextSpan {
    const uint8_t* data;
    const uint8_t* known;
    size_t size;
} Nba97GameTextSpan;
typedef struct Nba97GameTextContext {
    Nba97GameTextMemory memory;
    size_t step_budget; /* Native bound on original scans, never source repair. */
    Nba97GameTextIo io;
    void* user;
} Nba97GameTextContext;

/* Source roots: B2048 currentstyle pointer; B204C..58 colorwords; SDKC55B8,
 * C55BC,C55C2. Pointer fields are read live from retained bytes, preserving
 * cachedstyle vs freshglobalreload behavior. The caller supplies their proven
 * mappings and data, including actual font/glyph/pool/bitmap allocations.
 * Refusal retains completed ordered CPUwrites and callbacks. Stage allregions
 * and backend together for hostatomicity. No CPUarray or GPUobject is invented.
 * Output references change only on a completed source return (includingNULL).
 */
int nba97_game_text_reset_group(Nba97GameTextContext*,int32_t group,Nba97GameTextProgress*);
int nba97_game_text_reset_packet(Nba97GameTextContext*,uint32_t object,uint32_t count,Nba97GameTextProgress*);
int nba97_game_text_allocate_packets(Nba97GameTextContext*,int32_t glyph_count,uint32_t* allocation,Nba97GameTextProgress*);
int nba97_game_text_create(Nba97GameTextContext*,int32_t id,uint32_t text,
    int32_t x,int32_t y,uint32_t alignment_mode,uint32_t* object,Nba97GameTextProgress*);
/* Sameowner with a retained byte cursor for a native temporary label string.
 * 30D18 never publishes its text pointer; metrics use onlyrelativebytecount.
 * This avoids inventing an originalstackaddress. Span bytes/knownness may alias
 * region storage and change synchronously, but span metadata/lifetime isfixed.
 * Resource/object/packet pointers still require originaladdress provenance. */
int nba97_game_text_create_span(Nba97GameTextContext*,int32_t id,Nba97GameTextSpan,
    int32_t x,int32_t y,uint32_t alignment_mode,uint32_t* object,Nba97GameTextProgress*);

#ifdef __cplusplus
}
#endif
#endif
