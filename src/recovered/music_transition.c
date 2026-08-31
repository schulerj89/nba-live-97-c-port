#include "music_transition.h"

int nba97_music_transition_begin(Nba97MusicTransition* s, Nba97MusicRouting* route,
    Nba97MusicInputs* inputs, uint32_t resource, Nba97MusicTransitionInvoke call,
    void* context) {
    if (!s || !route || !inputs || !call) return 0;
    inputs->selection_blocked = 0;
    if ((resource == 0x1fu && !s->transition_guard) || resource == 0x24u || inputs->pause) {
        if (!call(context, NBA97_MUSIC_TRANSITION_FINISHED, 0, 0, 0)) {
            call(context, NBA97_MUSIC_TRANSITION_FADE, route->voice, 50, UINT32_MAX);
            route->phase = 4;
        }
        inputs->selection_blocked = 1; /* even if already finished */
    }
    /* Source re-reads ED2AC after the FINISHED/FADE callees. */
    if (resource == 0x24u || inputs->pause) {
        call(context, NBA97_MUSIC_TRANSITION_RELEASE, s->resource_handle, 0, 0);
        s->resource_handle = 0;
        if (resource == 0x24u) {
            const uint32_t volume = inputs->volume;
            const uint32_t other = s->other_volume;
            s->saved_volume = volume;
            /* Preserve original quirks: other==0 skips reduction altogether;
             * repeated24 entry overwrites the saved value with reduced volume. */
            if (other) {
                const uint32_t half = (other + 1u) >> 1;
                inputs->volume = (uint8_t)(volume < half ? volume : half);
            }
        } else {
            /* Do not restore a newly edited option instead of the saved byte. */
            inputs->volume = (uint8_t)s->saved_volume;
        }
    }
    inputs->pause = (uint32_t)(resource == 0x24u);
    return 1;
}

int nba97_music_transition_end(Nba97MusicInputs* inputs, uint32_t resource) {
    if (!inputs) return 0;
    if (resource != 0x24u) inputs->selection_blocked = 0;
    return 1;
}
