#ifndef NBA97_GAME_IMAGE_UPLOAD_H
#define NBA97_GAME_IMAGE_UPLOAD_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* One enclosing retained allocation, not a detached image record. known is
 * either size bytes (0 unknown,1 known), or NULL when ALL bytes are known.
 * Writes establish knownness. Reached values>1 refuse as ARGUMENT before a
 * read or write; each whole access is checked, without allocation preflight.
 * Unknown payload bytes need not be fabricated.
 * address_mod4 is the original allocation address's low two bits, independently
 * of its native pointer alignment; mark address_mod4_known only with provenance.
 * Metadata/allocation lifetime stays fixed through a call. Contents and known
 * bytes may alias other views and may change synchronously in the backend. */
typedef struct Nba97GameImageMemory {
    uint8_t* data;
    uint8_t* known;
    size_t size;
    uint8_t address_mod4,address_mod4_known;
} Nba97GameImageMemory;
typedef struct Nba97GameImageReference {
    Nba97GameImageMemory* memory;
    int64_t offset;
} Nba97GameImageReference;
typedef struct Nba97GameImagePlacement { int32_t x,y,clut_x,clut_y; } Nba97GameImagePlacement;
typedef struct Nba97GameImageRect { int16_t x,y,w,h; } Nba97GameImageRect;
typedef struct Nba97GameImageUploadState {
    uint32_t pending_d7b14;
    uint8_t pending_known;
} Nba97GameImageUploadState;
typedef struct Nba97GameImageTransfer {
    Nba97GameImageRect rect;
    Nba97GameImageReference source; /* Raw little-endian16-bit words. */
    uint8_t through_944f4; /* 1 => pending_d7b14 written AFTER this returns1. */
    /* Positive pre-SDK rectangle footprint, source9AC7C's CPU32-bit rounding.
     * SDK later clamps dimensions against signed16 C55C4/C55C6; the backend
     * must prove no clamp is required or explicitly own the clamped domain.
     * Zero/negative dimensions leave footprint_known=0: not an empty transfer.
     * CPU padding is consumed but is NOT an extra GPU rectangle pixel. */
    uint8_t footprint_known;
    uint32_t pixel_words,cpu_words;
} Nba97GameImageTransfer;

/* Actual9971C boundary. Return1 only after consuming/copying the requested raw
 * VRAM words. Do not repack indexed pixels or normalize this rectangle again.
 * This call's source can lie before the current image within the same owner.
 * The backend must check positive/in-range supported rectangle dimensions,
 * allocation bounds and knownness of consumed bytes. Unsupported GPU domains,
 * unknown data, or unowned bytes must refuse; never supply default zero pixels.
 * For the supported positive rectangle domain there are pixel_words=w*h GPU
 * LE16 words, but cpu_words=ceil(w*h/2) LE32 source words are consumed. Require
 * the padding halfword for an odd pixel count; do not upload it as a pixel.
 * Check original source alignment independently of the native pointer. CPU
 * LW requires32-bit alignment; exact16-word DMA blocks skip CPU loads and
 * their misaligned DMA behavior is unproved, not a demonstrated CPU trap.
 * GPU maskmode/wrap/overlap and device timing are not closed.
 * It may mutate retained image bytes/knownness and upload state synchronously;
 * it may not mutate reference metadata, the request, or progress counters.
 * No resource reference/request may outlive the owning allocation.
 * Return values other than1 mean native refusal, not the original SDK result. */
typedef int (*Nba97GameImageTransferIo)(void*,const Nba97GameImageTransfer*);
typedef struct Nba97GameImageUploadProgress {
    size_t headers_visited,uploads_completed;
    int64_t stopped_offset;
    uint8_t temporary_height_active;
} Nba97GameImageUploadProgress;
enum Nba97GameImageUploadResult {
    NBA97_IMAGE_COMPLETE=1,NBA97_IMAGE_ARGUMENT=0,
    NBA97_IMAGE_RESOURCE=-1,NBA97_IMAGE_UNKNOWN=-2,
    NBA97_IMAGE_IO_REFUSED=-3,NBA97_IMAGE_HEADER_LIMIT=-4,
    NBA97_IMAGE_ALIGNMENT_TRAP=-5,NBA97_IMAGE_FORMAT_UNRESOLVED=-6
};

/* A3BF8's unambiguous format-byte branches:23/40/41/42/43/44 after mask77.
 * The remaining formats reach A3C34 BEQ with JRra in its delay slot. Neither
 * the intended default1/72->8 nor a generic interpreter's result proves the
 * actual R3000 behavior. Return FORMAT_UNRESOLVED with *bits unchanged there.
 * Format44's earlier direct branch does unambiguously return1. */
int nba97_game_image_bits(uint8_t format_byte,uint32_t* bits);
/* 94440 deliberately ORs height with1 whenever width is odd. */
void nba97_game_image_prepare_rect(Nba97GameImageRect*);

/* Full946B8/94540/944F4/94440/A3BF8 CPU owner. Signed header links, wrapped
 * arithmetic, low16 coordinates, source header writes and original malformed
 * dimensions are retained. Budget applies to94540 header visits, not uploads;
 * it is a native bound on original unbounded/cyclic chains, not cycle repair.
 * All arguments/placements are copied by value before the first callback.
 * Reached unknown/out-of-bounds/alignment/backend/limit failures retain exact
 * completed prefixes. A refused inner upload leaves946B8's temporary height
 * changed: restoration is a later source instruction, NOT automatic cleanup.
 * A host transaction must stage the allocations and VRAM backend together.
 * Progress is diagnostic, not a resumable cursor. Entry argument errors leave
 * it unchanged; malformed metadata reached later retains the completed prefix.
 * memory=NULL,offset=0 denotes source NULL;94540 returns normally
 * for NULL, while946B8 immediately requires a format-byte read and refuses.
 */
int nba97_game_image_upload(Nba97GameImageUploadState*,Nba97GameImageReference,
    Nba97GameImagePlacement,size_t header_budget,Nba97GameImageTransferIo,void*,
    Nba97GameImageUploadProgress*);
int nba97_game_image_upload_chain(Nba97GameImageUploadState*,Nba97GameImageReference,
    Nba97GameImagePlacement,size_t header_budget,Nba97GameImageTransferIo,void*,
    Nba97GameImageUploadProgress*);
/* Direct944F4 owner for already prepared raw rectangles. Mutates *rect with
 * 94440 before the callback; pending word becomes known1 only after success. */
int nba97_game_image_upload_rect(Nba97GameImageUploadState*,Nba97GameImageReference,
    Nba97GameImageRect*,Nba97GameImageTransferIo,void*,Nba97GameImageUploadProgress*);

#ifdef __cplusplus
}
#endif
#endif
