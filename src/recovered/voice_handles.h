#ifndef NBA97_VOICE_HANDLES_H
#define NBA97_VOICE_HANDLES_H
#include "music_voice.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97VoiceStopChannel {
    uint8_t kind, paired_voice; /* F0D58+i*12 bytes0/1, not voice-table fields. */
} Nba97VoiceStopChannel;
typedef struct Nba97VoiceStopState {
    uint32_t excluded_voice; /* C6D34 */
    uint32_t keyoff_mask; /* C6D3C */
    uint32_t stream_stop; /* C6D58 */
    uint8_t changing, tracked_stream; /* C6D30/E45E4 */
    Nba97VoiceStopChannel channel[24];
} Nba97VoiceStopState;

/* Borrow the SAME24 voices/clock serviced by7A81C/7A6A8. This is not another
 * announcer clock or a WinMM readiness flag. All state is UI/service-thread
 * serialized. Callbacks may change live fields but cannot free these objects.
 * HARDWARE_SERVICE/STOP/APPLY and table reads must perform their actual effects;
 * the existing music_voice callback contract is unchanged. */
typedef struct Nba97VoiceHandles {
    Nba97MusicVoiceClock* clock;
    Nba97MusicVoice* voices;
    Nba97VoiceStopState* stop;
    Nba97MusicVoiceInvoke call;
    void* context;
    uint8_t enabled; /* signed D9BB5; every nonzero value is enabled. */
} Nba97VoiceHandles;
enum Nba97VoiceApiCompletion {
    NBA97_VOICE_API_COMPLETE=1, NBA97_VOICE_API_ARGUMENT=0,
    NBA97_VOICE_API_TIMER_TRAP=-1, NBA97_VOICE_API_UNOWNED_SLOT=-2
};
typedef struct Nba97VoiceApiResult {
    int completion;
    int32_t value; /* original return only when completion==COMPLETE */
} Nba97VoiceApiResult;

/* Complete916CC/92BFC/7B2BC/91748/92C34, including nested7AD08/7AD48.
 * Slots24..31 are outside the original24-voice allocation. On reaching one,
 * refuse with the preceding lock increments retained; do not manufacture the
 * source invalid-handle result or finish a blocked caller. Refusals/traps are
 * not resumable; a host may stage the complete shared state transactionally.
 * Source disabled=-10, invalidhandle=-8; status maps a resolved handle to0
 * and an invalid one to1. A status query may return its pre-unlock result even
 * if pending service invalidates the voice during unlock: preserve this quirk.
 * STOP queues a physical keyoff; it never clears active or finishes a buffer. */
Nba97VoiceApiResult nba97_voice_handle_resolve(Nba97VoiceHandles*,uint32_t handle);
Nba97VoiceApiResult nba97_voice_handle_status(Nba97VoiceHandles*,uint32_t handle);
Nba97VoiceApiResult nba97_voice_handle_fade(Nba97VoiceHandles*,uint32_t handle,uint32_t ticks,uint32_t target);
Nba97VoiceApiResult nba97_voice_handle_gain(Nba97VoiceHandles*,uint32_t handle,uint32_t gain);
Nba97VoiceApiResult nba97_voice_handle_stop(Nba97VoiceHandles*,uint32_t handle);
/* Entire71A68 for owned physical0..23. Native refusal makes no writes. */
int nba97_voice_stop_request(Nba97VoiceStopState*,uint32_t physical_voice);
/* Complete7AD08/7AD48. Unlock decrements without repairing zero depth and
 * drains pending callbacks only when the resulting depth is exactly zero. */
Nba97VoiceApiResult nba97_voice_handle_lock(Nba97VoiceHandles*);
Nba97VoiceApiResult nba97_voice_handle_unlock(Nba97VoiceHandles*);

#ifdef __cplusplus
}
#endif
#endif
