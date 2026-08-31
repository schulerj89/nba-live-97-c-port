#ifndef NBA97_MUSIC_ROUTING_H
#define NBA97_MUSIC_ROUTING_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* FEONLY 2F258/2F330/2F36C. These are source state bits, not PS1 pointers.
 * The native adapter owns resources/handles and maps nonzero opaque tokens.
 * Fields not written by an owner retain their previous values. */
typedef struct Nba97MusicRouting {
    uint32_t previous_a, previous_b; /* DE484, DED08 */
    uint32_t voice, stream;         /* 17268, 2149C */
    uint32_t retire_deadline, phase;/* FDBD4, F97B8 */
    uint32_t current, override;     /* F5D40, FF490 */
    uint32_t generation, updates;   /* ED720, F00C0 */
    uint32_t load_clock, deadline;  /* F1474, F9724 */
    uint32_t load_aux, inhibited;   /* DECF8, ED718 */
    uint32_t stopping, stop_clock;  /* FF2F4, FF304 */
    uint32_t fade_aux;             /* F435C */
} Nba97MusicRouting;

typedef struct Nba97MusicInputs {
    /* ED2AC selects ZTPAUSE; FE31A88 writes it for resource state0x24
     * (View Player). This name is not proof of gameplay Pause reachability. */
    uint32_t pause, selection_blocked; /* ED2AC, F9720 */
    uint16_t guard_a, guard_b;          /* 9327E, 93280 */
    uint8_t volume;                    /* 21D7C, unscaled */
} Nba97MusicInputs;

/* Load the sixteen source filename slots at FE93568, stride13, privately.
 * They contain duplicates and include the pause resource. Do not shuffle,
 * deduplicate, prohibit repeats, or remove pause from ordinary selection. */
typedef struct Nba97MusicResources {
    uint32_t initial, pause, slots[16];
} Nba97MusicResources;

typedef enum Nba97MusicCall {
    NBA97_MUSIC_READY,        /* 392F8 -> nonzero ready */
    NBA97_MUSIC_ALLOCATE,     /* 6C368(14000,2000,20) -> stream */
    NBA97_MUSIC_WAIT_STREAM,  /* 29B64(0,0) */
    NBA97_MUSIC_LOAD,         /* 6C93C(stream,resource) */
    NBA97_MUSIC_CLOCK,        /* 8DA5C -> raw source clock */
    NBA97_MUSIC_CONFIGURE,    /* 6F968(400 or210,400h,0) */
    NBA97_MUSIC_START_STREAM, /* 28C50(stream,10,200,19000h,0) */
    NBA97_MUSIC_VOICE,        /* 6ACAC(15,-1) -> voice */
    NBA97_MUSIC_GAIN,         /* 91748(voice,min(volume*15,127)) */
    NBA97_MUSIC_PUMP,         /* 39260 */
    NBA97_MUSIC_REFILL,       /* 28B8C */
    NBA97_MUSIC_FINISHED,     /* 6FCF0 -> nonzero */
    NBA97_MUSIC_FADE,         /* 7B2BC(voice,ticks,-1) */
    NBA97_MUSIC_BUSY,         /* 6B6A0 -> raw -14/1/3/4, NOT a boolean */
    NBA97_MUSIC_STOP_NOTIFY,  /* 79040 */
    NBA97_MUSIC_DETACH,       /* 28C28 */
    NBA97_MUSIC_RETIRE,       /* 6FAA0 */
    NBA97_MUSIC_FREE          /* 6CDE4(stream) */
} Nba97MusicCall;

/* Called synchronously in original order; only documented arguments matter.
 * Clock is sampled separately at each source call, including after pumps.
 * Adapter callbacks may update owned state/input as original callees do;
 * they must not recursively invoke this routing owner. No hardware/decode,
 * asynchronous lifetime, callback effect, or host timing is supplied here. */
typedef uint32_t (*Nba97MusicInvoke)(void* context, Nba97MusicCall call,
    uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

/* All pointers required; refusal returns0 before mutation or callbacks.
 * Resources, state, inputs and RNG must not overlap. Initial voice is the
 * separate source21EE0 handle, not the zeroed stream voice17268. Init does
 * not draw/reseed RNG; step shares the caller's TITLE/Cool Fact 16-bit RNG. */
int nba97_music_routing_init(Nba97MusicRouting* state,
    const Nba97MusicResources* resources, uint8_t volume, uint32_t initial_voice,
    Nba97MusicInvoke invoke, void* context);
int nba97_music_routing_step(Nba97MusicRouting* state,
    const Nba97MusicInputs* inputs, const Nba97MusicResources* resources,
    uint16_t* frontend_rng, Nba97MusicInvoke invoke, void* context);

#ifdef __cplusplus
}
#endif
#endif
