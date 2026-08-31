#ifndef NBA97_GAME_HEAD_CACHE_H
#define NBA97_GAME_HEAD_CACHE_H
#include "game_render_io.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Nba97GameHeadCache {
    uint16_t count[2]; /* Actual teamheader+68; no clamp to12. */
    int16_t lineup[2][12]; /* Actual teamheader+16. */
    int32_t current[2][12]; /* 1046A4/1046E8, signed full words. */
    Nba97GameRenderBuffer bench[2]; /* 1046DC/104740;4220 bytes per bench slot. */
    Nba97GameRenderBuffer scratch; /* Pointer102930;actual SHPP container. */
    int32_t xy[10][2]; /* B2238/B2260 full source argument words. */
} Nba97GameHeadCache;
/* Full38A18 and3875C with actual A3FEC(record0) lookup. Negative arguments
 * process twelve positions, home then away; nonnegative arguments search a
 * single side's cache (home0..11,away>=12) and move to slot0. Original cache
 * searches have NO bound; reaching entry12 explicitly refuses in this native
 * twelve-entry owner, rather than repairing or silently skipping a bad cache.
 * Native buffers must not partially overlap; exact same allocations are valid.
 * Synchronous I/O observes live cache values and original mutation order.
 * No memcpy substitutes for VRAM readback. All returned prefixes are retained.
 * Refusals are not resumable cursors: an outer transaction may stage this state.
 */
int nba97_game_head_cache(Nba97GameHeadCache*,int32_t argument,Nba97GameRenderIo,void*);
#ifdef __cplusplus
}
#endif
#endif
