#include "music_voice.h"
#include <stddef.h>

static int64_t signed32(uint32_t x) {
    return x < UINT32_C(0x80000000) ? (int64_t)x : (int64_t)x - INT64_C(4294967296);
}
static int32_t signed8(uint8_t x) { return x < 128 ? x : (int32_t)x - 256; }
static int64_t high16(uint32_t x) {
    return signed32(x & UINT32_C(0xffff0000)) / 65536;
}

int nba97_music_voice_fade(Nba97MusicVoice* v, uint32_t ticks, uint32_t target) {
    uint32_t fixed, numerator;
    int64_t duration;
    if (!v || target + 1u >= 129u) return -8;
    duration = signed32(ticks);
    if (duration <= 0) duration = 1;
    fixed = target << 16;
    numerator = (fixed - v->ramp_current) * 3u;
    v->ramp_target = fixed;
    /* Original quirk: integer truncation can leave a zero step indefinitely.
     * -1 targets negative gain and eventually requests stop; do not clamp0. */
    v->ramp_step = (uint32_t)(signed32(numerator) / duration);
    return 0;
}

int nba97_music_voice_gain(Nba97MusicVoice* v, uint32_t gain) {
    if (!v || gain >= 128u) return -8;
    v->ramp_current = gain << 16;
    v->ramp_step = 0; /* Source cancels fade but leaves ramp_target unchanged. */
    return 0;
}

void nba97_music_voice_effective(Nba97MusicVoice* v, uint8_t master,
    Nba97MusicVoiceInvoke call, void* context) {
    uint32_t product;
    if (!v || !call) return;
    product = (uint32_t)signed8(v->authored_gain) * (uint32_t)high16(v->ramp_current);
    product *= (uint32_t)high16(v->envelope_current);
    product *= (uint32_t)signed8(master);
    /* Source signed magic division is truncation by127^3. */
    v->effective_gain = (uint8_t)(signed32(product) / 2048383);
    if (v->gain_map_token)
        v->effective_gain = (uint8_t)call(context, NBA97_VOICE_GAIN_MAP,
            v->gain_map_token, (uint32_t)signed8(v->effective_gain), 0);
}

int nba97_music_voice_service(Nba97MusicVoiceClock* s, Nba97MusicVoice voices[24],
    Nba97MusicVoiceInvoke call, void* context) {
    uint32_t i;
    if (!s || !voices || !call) return 0;
    ++s->services;
    ++s->third_counter;
    call(context, NBA97_VOICE_HARDWARE_SERVICE, 0, 0, 0);
    if (s->optional[0]) call(context, NBA97_VOICE_OPTIONAL, s->optional[0], 0, 0);
    if (signed32(s->third_counter) % 3 != 0) return 1;
    for (i = 1; i < 4; ++i)
        if (s->optional[i]) call(context, NBA97_VOICE_OPTIONAL, s->optional[i], 0, 0);
    for (i = 0; i < 24; ++i) {
        Nba97MusicVoice* v = &voices[i];
        int changed = 0;
        if (v->active != 1 || signed32(v->handle) < 0) continue;
        if (v->ramp_step) {
            changed = 1;
            v->ramp_current += v->ramp_step;
            if ((signed32(v->ramp_step) < 0 && signed32(v->ramp_current) <= signed32(v->ramp_target)) ||
                (signed32(v->ramp_step) >= 0 && signed32(v->ramp_current) >= signed32(v->ramp_target))) {
                v->ramp_current = v->ramp_target;
                v->ramp_step = 0;
            }
            if (signed32(v->ramp_current) < 0)
                call(context, NBA97_VOICE_STOP, v->handle, 0, 0);
        }
        /* Source continues envelope work after requesting stop. */
        if (v->envelope_step) {
            v->envelope_current += v->envelope_step;
            changed = 1;
        }
        if (!v->envelope_ticks) {
            ++v->envelope_index;
            if ((int32_t)v->envelope_index >= signed8(v->envelope_count)) {
                call(context, NBA97_VOICE_STOP, v->handle, 0, 0);
            } else {
                uint32_t target;
                v->envelope_ticks = call(context, NBA97_VOICE_ENVELOPE_WORD,
                    v->envelope_token, v->envelope_index, 0) / 3u;
                target = call(context, NBA97_VOICE_ENVELOPE_WORD,
                    v->envelope_token, v->envelope_index, 1) << 16;
                /* Original bug/quirk: this is UNSIGNED division, even for a
                 * descending envelope. Durations below3 trap, not clamp. */
                if (!v->envelope_ticks) return -1;
                v->envelope_step = (target - v->envelope_current) / v->envelope_ticks;
            }
        }
        --v->envelope_ticks; /* FFFFFFFF is decremented, not an infinite sentinel. */
        if (changed) {
            nba97_music_voice_effective(v, s->master_gain, call, context);
            if (signed32(v->handle) >= 0)
                call(context, NBA97_VOICE_APPLY, i, (uint32_t)signed8(v->effective_gain), 0);
        }
    }
    return 1;
}

int nba97_music_voice_timer(Nba97MusicVoiceClock* s, Nba97MusicVoice voices[24],
    Nba97MusicVoiceInvoke call, void* context) {
    uint32_t target;
    int result;
    if (!s || !voices || !call) return 0;
    if (s->in_service) return 1;
    if (s->lock_depth) { ++s->pending; return 1; }
    s->in_service = 1;
    if (s->rate != s->cached_rate) {
        s->cached_rate = s->rate;
        s->services = 0;
        s->callbacks = 0;
    }
    ++s->callbacks;
    if (!s->rate) return -1; /* Original break7; retain in_service and counters. */
    target = (s->callbacks * 100u) / s->rate;
    /* Original uses <=, not <. Counter wrap can cause a very long catch-up. */
    while (s->services <= target) {
        result = nba97_music_voice_service(s, voices, call, context);
        if (result != 1) return result;
    }
    s->in_service = 0;
    return 1;
}

int nba97_music_stream_status(uint8_t flags, uint8_t pending) {
    if (!(flags & 2)) return -14;
    if (pending) return 4;
    if (!(flags & 1)) return 1;
    return (flags & 4) ? 3 : 1;
}

int nba97_music_hardware_status(int key_on, uint16_t adsr_level) {
    return key_on ? (adsr_level ? 1 : 3) : (adsr_level ? 2 : 0);
}

int nba97_music_voice_complete(Nba97MusicCompletion* s, Nba97MusicVoice* voice,
    uint32_t index, int status) {
    if (!s || !voice || index >= 24) return 0;
    if (s->channel_state == 1 && status == 0) {
        if ((int32_t)index == signed8(s->tracked_voice)) {
            s->tracked_voice = 255;
            s->finished = 1;
        }
        s->transient = 0;
        s->channel_state = 0;
        voice->active = 0; /* 916AC leaves the handle unchanged. */
    }
    return 1;
}
