#ifndef NBA97_RECOVERED_FRONTEND_AUDIO_H
#define NBA97_RECOVERED_FRONTEND_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Recovered fragments of FEONLY FUN_8002F124 and FUN_8002F258. */
uint8_t nba97_frontend_sfx_volume(uint8_t setting);
uint8_t nba97_frontend_music_volume(uint8_t setting);

#ifdef __cplusplus
}
#endif

#endif
