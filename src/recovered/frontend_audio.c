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
