#include "music_routing.h"
#include "frontend_title.h"

static uint32_t gain(uint8_t volume) {
    const uint32_t value = (uint32_t)volume * 15u;
    return value < 128u ? value : 127u;
}
static int signed_less(uint32_t a, uint32_t b) {
    return (a ^ 0x80000000u) < (b ^ 0x80000000u);
}
#define CALL(op,a,b,c,d,e) invoke(context,NBA97_MUSIC_##op,a,b,c,d,e)
#define CALL0(op) CALL(op,0,0,0,0,0)

int nba97_music_routing_init(Nba97MusicRouting* s,
    const Nba97MusicResources* resources, uint8_t volume, uint32_t initial_voice,
    Nba97MusicInvoke invoke, void* context) {
    if (!s || !resources || !invoke) return 0;
    s->previous_a = s->previous_b = UINT32_MAX;
    s->voice = 0;
    s->generation = 0;
    s->stream = 0;
    s->retire_deadline = 0;
    s->phase = 0;
    s->current = resources->initial;
    s->override = 0;
    ++s->generation;
    CALL(GAIN,initial_voice,gain(volume),0,0,0);
    return 1;
}

int nba97_music_routing_step(Nba97MusicRouting* s,
    const Nba97MusicInputs* inputs, const Nba97MusicResources* resources,
    uint16_t* frontend_rng, Nba97MusicInvoke invoke, void* context) {
    uint32_t now;
    unsigned i;
    if (!s || !inputs || !resources || !frontend_rng || !invoke) return 0;
    if (s->voice == UINT32_MAX || inputs->guard_a || inputs->guard_b || s->inhibited)
        return 1;
    ++s->updates;
    switch (s->phase) {
    case 0:
        if (CALL0(READY)) {
            s->stream = CALL(ALLOCATE,0x14000,0x2000,0x20,0,0);
            CALL(WAIT_STREAM,0,0,0,0,0);
            CALL(LOAD,s->stream,s->current,0,0,0);
            s->phase = 2;
            s->load_aux = s->inhibited = s->stopping = s->updates = 0;
            s->load_clock = CALL0(CLOCK);
        }
        break;
    case 1:
        if (!inputs->selection_blocked && CALL0(READY)) {
            if (s->override) {
                s->current = s->override;
                s->override = 0;
            } else if (inputs->pause) {
                s->current = resources->pause;
            } else {
                s->current = resources->slots[nba97_frontend_random(frontend_rng) & 15u];
            }
            ++s->generation;
            s->load_clock = CALL0(CLOCK);
            CALL(WAIT_STREAM,0,0,0,0,0);
            CALL(LOAD,s->stream,s->current,0,0,0);
            s->phase = 2;
        }
        break;
    case 2:
        if (CALL0(READY)) {
            CALL(CONFIGURE,inputs->pause ? 210u : 400u,0x400,0,0,0);
            CALL(START_STREAM,s->stream,10,200,0x19000,0);
            s->voice = CALL(VOICE,15,UINT32_MAX,0,0,0);
            CALL(GAIN,s->voice,gain(inputs->volume),0,0,0);
            for (i = 0; i < 30; ++i) {
                CALL0(PUMP);
                CALL0(REFILL);
            }
            s->deadline = CALL0(CLOCK) + 26500u;
            s->phase = 3;
        }
        break;
    case 3:
        CALL0(PUMP);
        if (CALL0(FINISHED)) s->phase = 4;
        now = CALL0(CLOCK);
        /* Preserve strict signed absolute comparison, including wrap quirks.
         * Do not modernize it to an unsigned elapsed-time comparison. */
        if (signed_less(s->deadline,now)) {
            s->deadline = 0;
            CALL(FADE,s->voice,60,UINT32_MAX,0,0);
            s->phase = 4;
        }
        /* The audited FE6B6A0 returns only -14/1/3/4, so its zero branch
         * is unreachable with that callee. Preserve the owner's comparison;
         * a host must not replace raw status with an is-playing boolean. */
        if (!CALL0(BUSY) && s->stopping) {
            s->phase = 11;
            s->inhibited = 1;
            s->stopping = 0;
            CALL0(STOP_NOTIFY);
        }
        now = CALL0(CLOCK);
        if (!signed_less(now - s->stop_clock,111u) && s->stopping) {
            CALL(FADE,s->voice,100,UINT32_MAX,0,0);
            s->fade_aux = 35;
            s->stop_clock = CALL0(CLOCK);
        }
        break;
    case 4:
        if (!s->retire_deadline) s->retire_deadline = CALL0(CLOCK) + 120u;
        now = CALL0(FINISHED);
        if (!now) {
            const uint32_t clock = CALL0(CLOCK);
            now = (uint32_t)signed_less(s->retire_deadline,clock);
        }
        if (now) {
            s->retire_deadline = 0;
            CALL0(DETACH);
            CALL0(RETIRE);
            s->phase = 1;
        }
        break;
    case 10:
        CALL(FADE,s->voice,100,UINT32_MAX,0,0);
        s->stopping = 1;
        s->phase = 3;
        s->fade_aux = 35;
        s->load_clock = CALL0(CLOCK);
        s->stop_clock = s->load_clock;
        break;
    case 11:
        /* Source enters11 while inhibited. Its caller must clear inhibition
         * before this case runs; automatically clearing it would change source. */
        if (CALL0(READY)) {
            CALL(FREE,s->stream,0,0,0,0);
            s->phase = 12;
            s->stopping = 0;
        }
        break;
    default:
        break;
    }
    return 1;
}

#undef CALL0
#undef CALL
