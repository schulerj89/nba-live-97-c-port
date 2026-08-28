#ifndef NBA97_FRONTEND_TITLE_H
#define NBA97_FRONTEND_TITLE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native representation of the two flag-0x100 objects registered by31A88.
   Primitive double buffers share these coordinates; they are not two texture
   chunks. Clock, drawing, tag animation and the shared RNG owner live outside. */
typedef struct Nba97TitleMotion {
    int16_t base[2][8];
    int16_t current[2][8];
    uint8_t count;
    uint8_t next;
} Nba97TitleMotion;
uint16_t nba97_frontend_random(uint16_t* state);
int nba97_title_init(Nba97TitleMotion* state, const int16_t base[2][8], unsigned count);
/* Returns changed object0/1, or-1 on an absent second slot. Consumes eight
   random outputs only for an existing object. Never mutates the other slot. */
int nba97_title_step(Nba97TitleMotion* state, uint16_t* random_state);
/* 39574's normal presentation path: unconditional RNG advance, then optional
   32BF0. The separate3282C/36898 callers use title_step directly. */
int nba97_title_selector_step(Nba97TitleMotion* state, uint16_t* random_state, int suppressed);
#ifdef __cplusplus
}
#endif
#endif
