#ifndef NBA97_VOICE_CHANNELS_H
#define NBA97_VOICE_CHANNELS_H
#include "voice_handles.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97VoiceChannels {
    Nba97VoiceStopState* stop; /* SAME71A68 mask/kind/paired/stream fields. */
    Nba97MusicVoice* voices; /* SAME F06B8 physical24 voices. */
    uint8_t* finished; /* SAME E45E7 used by the stream completion owner. */
    uint32_t state[24]; /* F0D58+i*12 word+4. */
    uint8_t transient[24]; /* F0D58+i*12 byte+2. */
    uint32_t busy, stream_maintenance, keyon_mask, auxiliary_on, auxiliary_off;
    /* C6D54,C6D50,C6D38,C6D40,C6D44 */
    uint8_t stream_pending, transfer_pending, auxiliary_enabled;
    /* C6D2C,C6D2A,C6D2E */
    uint32_t hardware_result; /* D9D0C retains the last status bit pattern. */
} Nba97VoiceChannels;
enum Nba97VoiceChannelCall {
    NBA97_CHANNEL_SAMPLE_7AEE4=1,
    NBA97_CHANNEL_STREAM_72954,
    NBA97_CHANNEL_STATUS_7BFA0,
    NBA97_CHANNEL_AUXILIARY_7E684,
    NBA97_CHANNEL_KEY_6F858
};
/* Required synchronous platform boundary. Return1 only after the operation
 * was performed; STATUS writes its actual signed source result as32 bits.
 * a0/a1 retain original arguments. Callbacks may mutate live fields, including
 * masks; original post-call rereads and clears must be preserved. Allocations
 * and the three borrowed pointers must stay valid throughout this call. */
typedef int (*Nba97VoiceChannelInvoke)(void*,enum Nba97VoiceChannelCall,
    uint32_t a0,uint32_t a1,uint32_t* result);
enum Nba97VoiceChannelResult {
    NBA97_CHANNEL_COMPLETE=1,NBA97_CHANNEL_ARGUMENT=0,
    NBA97_CHANNEL_IO_REFUSED=-1,NBA97_CHANNEL_UNOWNED_PAIR=-2
};
/* Complete702B0..70884 CPU owner, including916AC's active clear. Return is
 * native completion, not the unused original register. Refusal preserves all
 * preceding mutations/effects and is not a resumable cursor. Paired bytes24+
 * refuse when a paired STATE dereference is reached; bit shifts still use
 * low5 bits as original. No source active-clear occurs on KEY request alone.
 * A nonzero stop->changing returns with busy==1: deliberate original quirk. */
int nba97_voice_channels_service(Nba97VoiceChannels*,Nba97VoiceChannelInvoke,void*);

#ifdef __cplusplus
}
#endif
#endif
