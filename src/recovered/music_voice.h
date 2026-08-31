#ifndef NBA97_MUSIC_VOICE_H
#define NBA97_MUSIC_VOICE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Selected fields of FE F06B8 + physical_voice*68. Fixed point and counters
 * are unsigned bit patterns so MIPS wrapping never invokes C signed overflow. */
typedef struct Nba97MusicVoice {
    uint32_t handle, ramp_step, ramp_target, ramp_current;
    uint32_t envelope_step, envelope_current, envelope_ticks;
    uint32_t envelope_token, gain_map_token;
    uint8_t active, authored_gain, effective_gain, envelope_index, envelope_count;
} Nba97MusicVoice;

typedef struct Nba97MusicVoiceClock {
    uint32_t in_service, cached_rate, pending, lock_depth; /* C73FC..C7408 */
    uint32_t third_counter, rate, services, callbacks; /* C740C,D9ADC,D9CDC,D9C9C */
    uint32_t optional[4]; /* D9BC4,D9BB8,D9BBC,D9BC0, opaque nonzero tokens */
    uint8_t master_gain; /* D9CEC, signed byte in source */
} Nba97MusicVoiceClock;

typedef enum Nba97MusicVoiceCall {
    NBA97_VOICE_HARDWARE_SERVICE, /* 702B0; may clear voice.active */
    NBA97_VOICE_OPTIONAL,         /* a0=opaque callback token */
    NBA97_VOICE_STOP,             /* 92C34(a0=handle); NOT immediate completion */
    NBA97_VOICE_APPLY,            /* 71600(a0=index,a1=signed gain bits) */
    NBA97_VOICE_ENVELOPE_WORD,    /* return word: a0=token,a1=stage,a2=word0/1 */
    NBA97_VOICE_GAIN_MAP          /* return byte: a0=token,a1=signed index bits */
} Nba97MusicVoiceCall;
typedef uint32_t (*Nba97MusicVoiceInvoke)(void*, Nba97MusicVoiceCall,
    uint32_t a0, uint32_t a1, uint32_t a2);

/* Pure resolved-voice portions of 7B2BC and 91748: the adapter must retain
 * original enabled/handle validation and lock/unlock around these operations.
 * Invalid target/gain returns -8 without mutation; success returns0.
 * fade ticks are signed source bits, NOT120Hz frontend clock ticks. */
int nba97_music_voice_fade(Nba97MusicVoice*, uint32_t ticks, uint32_t target);
int nba97_music_voice_gain(Nba97MusicVoice*, uint32_t gain);
/* 76334, including signed bytes, wrapping products, optional mapping. */
void nba97_music_voice_effective(Nba97MusicVoice*, uint8_t master,
    Nba97MusicVoiceInvoke, void*);

/* Whole7A81C and7A6A8 control flow, with hardware/optional calls explicit.
 * Exactly24 voices and all pointers/callback required. Synchronous callbacks
 * can mutate the supplied state as original callees do. Tables must be valid
 * for every read requested by the original; no guessed envelope is supplied.
 * Returns1 on completion,0 invalid arguments, -1 at original divide-by-zero
 * trap with preceding mutations retained. Never resume after that trap.
 * timer preserves the inclusive extra service and wrapping counters; it does
 * not cap catch-up, repair a zero rate, or impose a host timer policy. */
int nba97_music_voice_service(Nba97MusicVoiceClock*, Nba97MusicVoice voices[24],
    Nba97MusicVoiceInvoke, void*);
int nba97_music_voice_timer(Nba97MusicVoiceClock*, Nba97MusicVoice voices[24],
    Nba97MusicVoiceInvoke, void*);

/* Entire6B6A0 return table. Original never returns0, including after stop. */
int nba97_music_stream_status(uint8_t flags, uint8_t pending);
/* Entire7BFA0 return table after first set voice bit has been selected.
 * The caller handles the no-bit case (-1) and real SPU register sampling. */
int nba97_music_hardware_status(int key_on, uint16_t adsr_level);

/* State1 ending arm of702B0 (70508..705C4), including916AC active clear.
 * This does NOT supply the producer/IRQ transition INTO channel_state1.
 * tracked_voice and finished project global E45E4/E45E7; the channel state
 * and transient byte project F0D58+index*12. Physical index must be0..23. */
typedef struct Nba97MusicCompletion {
    uint32_t channel_state;
    uint8_t transient, tracked_voice, finished;
} Nba97MusicCompletion;
int nba97_music_voice_complete(Nba97MusicCompletion*, Nba97MusicVoice*,
    uint32_t physical_index, int hardware_status);

#ifdef __cplusplus
}
#endif
#endif
