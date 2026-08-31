#ifndef NBA97_GAME_FONT_LOADER_H
#define NBA97_GAME_FONT_LOADER_H
#include "game_text_objects.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Full2E528 CPU producer, using retained original-address mappings. It requires
 * preexisting B2048/style and style+8/+C descriptor/map allocations. It does
 * not allocate a replacement font or initialize unrelated style/pool fields.
 * Region metadata/lifetimes remain fixed; bytes/knownness may alias and may
 * change synchronously at callbacks. Original stack/code aliases are outside
 * this bounded domain. All reached knownness values must be canonical0/1. */
enum Nba97GameFontResult {
    NBA97_FONT_COMPLETE=1,NBA97_FONT_ARGUMENT=0,NBA97_FONT_RESOURCE=-1,
    NBA97_FONT_UNKNOWN=-2,NBA97_FONT_ALIGNMENT_TRAP=-3,
    NBA97_FONT_LIMIT=-4,NBA97_FONT_IO_REFUSED=-5
};
enum Nba97GameFontEventKind {
    NBA97_FONT_LOAD_ATTEMPT_941C8=1,NBA97_FONT_RELEASE_90698,
    NBA97_FONT_UPLOAD_CHAIN_94540,NBA97_FONT_UPLOAD_946B8
};
typedef struct Nba97GameFontEvent {
    int kind;
    uint32_t resource; /* LOAD: original filename address; others: resource. */
    uint32_t flags; /* LOAD flags=0x20. */
    int32_t x,y,clut_x,clut_y;
} Nba97GameFontEvent;
/* Return1 only after the actual operation completes; no successful default.
 * LOAD writes *loaded (including sourceNULL); original29BFC retries NULL with
 * unchanged filename/flags, bounded by step_budget. Other events ignore it.
 * Upload events must execute actual94540/946B8 effects (the frozen image-upload
 * owner can do this), not only copy pixels. RELEASE requires actual90698
 * allocation semantics. Callbacks may mutate retained bytes, not metadata,
 * scratch, progress, or the event. Event pointers cannot escape the callback. */
typedef int (*Nba97GameFontIo)(void*,const Nba97GameFontEvent*,uint32_t* loaded);
/* Original stack+18 packet and stack+40 name. Caller supplies incoming bytes
 * AND per-byte knownness; no invented source stack address is published.
 * 9C328 writes only packet[3]/[7].2EA80 copies packet[0..3], so untouched low24
 * provenance is retained, including unknownness. Invalid liveSHPP name index
 * writes zero through A4014's branch tail A4038..A4044.
 * Contents update in source order; metadata/lifetime fixed and disjoint from
 * retained regions. The original initialized256-byte palette grid is local. */
typedef struct Nba97GameFontScratch {
    uint8_t packet[40],packet_known[40],name[4],name_known[4];
} Nba97GameFontScratch;
typedef struct Nba97GameFontProgress {
    size_t steps,callbacks_completed,glyphs_written;
    uint32_t stopped_address;
    uint8_t stopped_in_scratch; /* 1=packet offset,2=name offset. */
} Nba97GameFontProgress;
typedef struct Nba97GameFontContext {
    Nba97GameTextMemory memory;
    size_t step_budget;
    Nba97GameFontIo io;
    void* user;
} Nba97GameFontContext;

/* Numeric decoding2E468 preserves original byte masking and invalid=>0; no
 * locale conversion or generalized hex parser. Result is source signed16. */
int32_t nba97_game_font_decode(uint32_t high,uint32_t low);
/* Independent pureSHPP helpers. Unsigned index>=live count returnsNULL for
 * entry and writes a zero word for name. Counts/offsets are raw32;
 * no signature/equal-size/forward-only normalization is imposed. */
int nba97_game_font_shpp_count(Nba97GameFontContext*,uint32_t resource,
    uint32_t* count,Nba97GameFontProgress*);
int nba97_game_font_shpp_entry(Nba97GameFontContext*,uint32_t resource,uint32_t index,
    uint32_t* entry,Nba97GameFontProgress*);
int nba97_game_font_shpp_name(Nba97GameFontContext*,uint32_t resource,uint32_t index,
    uint32_t destination,Nba97GameFontProgress*);
int nba97_game_font_load(Nba97GameFontContext*,uint32_t filename,
    uint32_t spacing,uint32_t kerning,uint32_t clut_x,uint32_t clut_y,
    uint32_t recolor,Nba97GameFontScratch*,Nba97GameFontProgress*);

#ifdef __cplusplus
}
#endif
#endif
