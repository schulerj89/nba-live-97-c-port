#ifndef NBA97_GAME_SETUP_H
#define NBA97_GAME_SETUP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Card order differs from the resident field order: quarter, mode, style, level.
 * 8003F43C/80039DF8 establish 4,3,3,3 choices. c16a is not selectable. */
int nba97_setup_step(uint8_t choices[4], unsigned card, int direction);
#ifdef __cplusplus
}
#endif
#endif
