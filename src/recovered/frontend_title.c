#include "frontend_title.h"
#include <string.h>

uint16_t nba97_frontend_random(uint16_t* state) {
    /* 80029B20..80029B60. SLL17 tests bit14, NOT bit15. Preserve the
       zero-seed fallback, shift, polynomial and final halfword truncation. */
    const uint32_t prior = *state ? *state : 0xa5a5u;
    const uint32_t value = (prior << 1) ^ ((prior & 0x4000u) ? 0x1d87u : 0);
    *state = (uint16_t)value;
    return *state;
}

int nba97_title_init(Nba97TitleMotion* s, const int16_t base[2][8], unsigned count) {
    if (!s || !base || count < 1 || count > 2) return 0;
    memcpy(s->base, base, sizeof(s->base));
    memcpy(s->current, base, sizeof(s->current));
    s->count = (uint8_t)count;
    s->next = 0;
    return 1;
}

int nba97_title_step(Nba97TitleMotion* s, uint16_t* random_state) {
    unsigned i;
    const unsigned object = s->next;
    s->next ^= 1;
    /* 80033070..80033088: absent second object resets phase without draws. */
    if (object >= s->count) return -1;
    for (i=0; i<8; ++i) {
        const uint16_t value = (uint16_t)((uint16_t)s->base[object][i] +
                                        (nba97_frontend_random(random_state) & 3u));
        /* Model SH wrap without implementation-defined unsigned->signed cast. */
        s->current[object][i] = (int16_t)(value < 0x8000u ? (int32_t)value : (int32_t)value - 65536);
    }
    return (int)object;
}

int nba97_title_selector_step(Nba97TitleMotion* s, uint16_t* random_state, int suppressed) {
    (void)nba97_frontend_random(random_state); /* 800395AC, even when suppressed */
    return suppressed ? -1 : nba97_title_step(s, random_state);
}
