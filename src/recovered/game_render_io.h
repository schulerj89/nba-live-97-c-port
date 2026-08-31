#ifndef NBA97_GAME_RENDER_IO_H
#define NBA97_GAME_RENDER_IO_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97GameRenderBuffer { uint8_t* data; size_t size; } Nba97GameRenderBuffer;
/* offset retains an image's position in its enclosing owned allocation, so
 * signed header-relative palette offsets can point backward without UB. */
typedef struct Nba97GameRenderImage { Nba97GameRenderBuffer storage; size_t offset; } Nba97GameRenderImage;
typedef struct Nba97GameRenderRect { int16_t x,y,w,h; } Nba97GameRenderRect;
enum Nba97GameRenderIoKind {
    NBA97_RENDER_UPLOAD_946B8=1, NBA97_RENDER_SYNC_994F4,
    NBA97_RENDER_STORE_99780, NBA97_RENDER_MOVE_997E4,
    NBA97_RENDER_SERVICE_8892C
};
typedef struct Nba97GameRenderIoEvent {
    int kind;
    Nba97GameRenderImage image; /* UPLOAD only; same source bytes, no decoding. */
    Nba97GameRenderBuffer destination; /* STORE only; backend must fill it. */
    Nba97GameRenderRect rect; /* STORE/MOVE source rectangle. */
    int32_t x,y,clut_x,clut_y; /* Original full signed argument words. */
} Nba97GameRenderIoEvent;
/* Synchronous real backend boundary. Return1 only after performing the named
 * operation. Uploads must consume/copy bytes before return: scratch and shared
 * CLUTs are overwritten later. STORE writes destination. SERVICE is the actual
 * CD/service wait8892C, not a GPU flush; unresolved service must refuse.
 * Callbacks may mutate live owner state; consumed values are read in source
 * order. They may not resize/free allocations while an owner is running. */
typedef int (*Nba97GameRenderIo)(void* context,const Nba97GameRenderIoEvent* event);
enum Nba97GameRenderResult {
    NBA97_RENDER_COMPLETE=1,NBA97_RENDER_ARGUMENT=0,
    NBA97_RENDER_RESOURCE=-1,NBA97_RENDER_IO_REFUSED=-2,
    NBA97_RENDER_SEARCH_OUTSIDE_OWNER=-3,NBA97_RENDER_TEXT_OVERFLOW=-4
};
#ifdef __cplusplus
}
#endif
#endif
