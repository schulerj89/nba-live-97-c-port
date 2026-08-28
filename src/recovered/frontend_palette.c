#include "frontend_palette.h"
#include <string.h>

static uint16_t part(uint16_t from, uint16_t to, int mask, unsigned factor) {
    const int a = from & mask;
    const int b = to & mask;
    /* C99 signed / truncates toward zero, matching negative-product +15
     * followed by SRA4 at 8002FF54..8002FF60. Do not shift channels first:
     * division followed by mask has different rounding for green/blue. */
    return (uint16_t)((a + ((b-a)*(int)factor)/16) & mask);
}
uint16_t nba97_frontend_palette_blend(uint16_t from, uint16_t to, unsigned factor) {
    if (factor > 16) factor = 16;
    return (uint16_t)(part(from,to,0x001f,factor) |
        part(from,to,0x03e0,factor) | part(from,to,0x3c00,factor) | (to & 0x8000));
}
static int valid(const Nba97FrontendPalette *s, unsigned count) {
    return s && s->initialized && count && count<=256 &&
        s->half[0].target<count && s->half[1].target<count &&
        s->half[0].next_factor<=17 && s->half[1].next_factor<=17;
}
int nba97_frontend_palette_begin(Nba97FrontendPalette *s, const uint16_t *bank,
        unsigned count, unsigned left, unsigned right) {
    Nba97FrontendPalette fresh;
    unsigned side;
    if (!s || !bank || !count || count>256 || left>=count || right>=count) return 0;
    memset(&fresh,0,sizeof(fresh));
    for(side=0;side<2;++side) {
        const unsigned team=side?right:left;
        Nba97PaletteHalf *h=&fresh.half[side];
        h->target=(uint8_t)team; h->next_factor=17;
        memcpy(h->current,bank+team*160,sizeof(h->current));
        memcpy(h->from,h->current,sizeof(h->from));
    }
    fresh.initialized=1; *s=fresh;
    return 1;
}
int nba97_frontend_palette_request(Nba97FrontendPalette *s, unsigned side,
        unsigned target, unsigned count) {
    Nba97PaletteHalf *h;
    if (!valid(s,count) || side>1 || target>=count) return 0;
    h=&s->half[side];
    if (target==h->target) return 1;
    memcpy(h->from,h->current,sizeof(h->from));
    h->target=(uint8_t)target; h->next_factor=0;
    return 1;
}
int nba97_frontend_palette_tick(Nba97FrontendPalette *s, const uint16_t *bank,
        unsigned count) {
    unsigned side, i;
    int changed=0;
    if (!bank || !valid(s,count)) return -1;
    for(side=0;side<2;++side) {
        Nba97PaletteHalf *h=&s->half[side];
        if (h->next_factor==17) continue;
        for(i=0;i<160;++i) h->current[i]=nba97_frontend_palette_blend(
            h->from[i],bank[h->target*160+i],h->next_factor);
        ++h->next_factor;
        changed|=1<<side;
    }
    return changed;
}
