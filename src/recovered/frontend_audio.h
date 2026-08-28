#ifndef NBA97_RECOVERED_FRONTEND_AUDIO_H
#define NBA97_RECOVERED_FRONTEND_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Recovered fragments of FEONLY FUN_8002F124 and FUN_8002F258. */
uint8_t nba97_frontend_sfx_volume(uint8_t setting);
uint8_t nba97_frontend_music_volume(uint8_t setting);
/* 314A0 deferred start / 31770 immediate start both read 80021D7D. */
uint8_t nba97_frontend_speech_volume(uint8_t setting);
/* 59DB8(1) selects cue5 only if 313C8 actually stopped a voice. */
uint8_t nba97_frontend_fact_stop_sound(int stopped_voice, uint16_t feedback);

#ifdef __cplusplus
}
#endif

#endif
