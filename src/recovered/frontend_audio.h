#ifndef NBA97_RECOVERED_FRONTEND_AUDIO_H
#define NBA97_RECOVERED_FRONTEND_AUDIO_H

#include <stddef.h>
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

typedef struct Nba97CursorScalars {
    uint8_t authored_volume, effective_volume;
    uint16_t base_pitch, pitch, left_volume, right_volume;
} Nba97CursorScalars;

/* Bounded cursor projection from 9267C/76334/70E54/72048/71D8C:
 * mono 22050 Hz, centered pan, velocity/envelope/master127, no random gain
 * or maps. The C++ owner validates those tone conditions and owns the table.
 * All volume inputs must be0..127; cents must be-1200..1200; the unsigned-byte
 * table must contain exactly256 entries. Refusal leaves output unchanged.
 * No RNG, allocation or PCM synthesis occurs here. In particular, playback0
 * is a valid scalar input; the caller separately owns the SFX mute branch.
 * Wider pitch/octave paths and complete source owners remain outside scope. */
int nba97_cursor_scalars(Nba97CursorScalars* out, uint8_t program,
                         uint8_t tone, uint8_t playback, int32_t cents,
                         const uint8_t* table, size_t table_bytes);

#ifdef __cplusplus
}
#endif

#endif
