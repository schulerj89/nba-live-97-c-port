#include "frontend_audio.h"

static uint8_t scaled_volume(uint8_t setting, uint8_t scale)
{
    unsigned int value = (unsigned int)setting * scale;
    return (uint8_t)(value > 127u ? 127u : value);
}

uint8_t nba97_frontend_sfx_volume(uint8_t setting)
{
    /* FEONLY 0x8002F124: min(frontend SFX setting * 12, 127). */
    return scaled_volume(setting, 12u);
}

uint8_t nba97_frontend_music_volume(uint8_t setting)
{
    /* FEONLY 0x8002F258: min(frontend music setting * 15, 127). */
    return scaled_volume(setting, 15u);
}

uint8_t nba97_frontend_speech_volume(uint8_t setting)
{
    return scaled_volume(setting, 15u);
}

uint8_t nba97_frontend_fact_stop_sound(int stopped_voice, uint16_t feedback)
{
    return (uint8_t)(stopped_voice && feedback ? 5 : 0);
}

int nba97_cursor_scalars(Nba97CursorScalars* out, uint8_t program,
                         uint8_t tone, uint8_t playback, int32_t cents,
                         const uint8_t* table, size_t table_bytes)
{
    Nba97CursorScalars result;
    int32_t index;
    uint32_t factor;
    if (!out || !table || table_bytes != 256u || program > 127u ||
        tone > 127u || playback > 127u || cents < -1200 || cents > 1200)
        return 0;

    /* 9267C truncates authored gain before 76334 applies playback volume.
     * With the fixed cursor velocity/envelope/master, their other factors
     * cancel exactly. Combining these two divisions would change results. */
    result.authored_volume = (uint8_t)((uint32_t)program * tone / 127u);
    result.effective_volume =
        (uint8_t)((uint32_t)result.authored_volume * playback / 127u);
    result.left_volume = (uint16_t)((uint32_t)result.effective_volume * 129u);
    result.right_volume = result.left_volume;

    /* 70E54: (22050 * 0x17C7) >> 16. The validated cents range needs no
     * octave shifts and keeps this signed product within int32_t. C99 signed
     * division truncates toward zero, as 72048 does before selecting a branch.
     * In particular, cents-1..-4 have index0 and use the nonnegative branch. */
    result.base_pitch = 2048;
    index = (cents * 0x369d) / 65536;
    if (index < 0) {
        factor = (uint32_t)table[256 + index] + 256u;
        result.pitch = (uint16_t)(factor * result.base_pitch / 512u);
    } else {
        factor = (uint32_t)table[index] + 256u;
        result.pitch = (uint16_t)(factor * result.base_pitch / 256u);
    }
    *out = result;
    return 1;
}
