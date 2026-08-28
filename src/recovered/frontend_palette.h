#ifndef NBA97_FRONTEND_PALETTE_H
#define NBA97_FRONTEND_PALETTE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_TEAM_PALETTE_COLORS = 160 };
typedef struct Nba97PaletteHalf {
    uint16_t from[NBA97_TEAM_PALETTE_COLORS];
    uint16_t current[NBA97_TEAM_PALETTE_COLORS];
    uint8_t target, next_factor;
} Nba97PaletteHalf;
typedef struct Nba97FrontendPalette {
    Nba97PaletteHalf half[2];
    uint8_t initialized;
} Nba97FrontendPalette;

/* 8002FF40/8002FF80: preserve masked-word signed division, target STP and
 * the original FOUR-bit blue mask 3C00. Factor0..16 is a logical UI frame,
 * not milliseconds. Out-of-range factors are clamped as a native guard. */
uint16_t nba97_frontend_palette_blend(uint16_t from, uint16_t to, unsigned factor);
/* Borrowed contiguous bank of palette_count*160 original CLUT words. */
int nba97_frontend_palette_begin(Nba97FrontendPalette*, const uint16_t *bank,
    unsigned palette_count, unsigned left, unsigned right);
/* Restart only the changed half from its CURRENT blended colours. */
int nba97_frontend_palette_request(Nba97FrontendPalette*, unsigned side,
    unsigned target, unsigned palette_count);
/* Apply one original logical update to both halves; returns changed-half mask
 * (1=left,2=right), zero when settled, -1 for invalid native arguments. */
int nba97_frontend_palette_tick(Nba97FrontendPalette*, const uint16_t *bank,
    unsigned palette_count);

#ifdef __cplusplus
}
#endif
#endif
