#ifndef NBA97_GAME_SUBSTITUTION_SUPPORT_H
#define NBA97_GAME_SUBSTITUTION_SUPPORT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameClockValue { uint32_t word;uint8_t known; } Nba97GameClockValue;
typedef struct Nba97GameClockEffect {
    Nba97GameClockValue previous164,delta;
} Nba97GameClockEffect;
/* Complete A584C(13 instructions) and A5810(4): sample is the actual current
 * D7A70 word. Writes sample to previous GP+164; returns wrapping sample-old.
 * Unknown prior state produces an unknown delta, not a fabricated zero. The
 * new previous value can still be known. known0 requires word0 metadata.
 * No clock frequency, elapsed units, source sample, or old state is invented.
 * Return1 valid projection,0 bad representation; failure leaves output intact. */
int nba97_game_clock_sample(Nba97GameClockEffect* out,
    const Nba97GameClockValue* previous,const Nba97GameClockValue* sample);

typedef struct Nba97GameWaitLiveState {
    Nba97GameClockValue sample_d7a70,previous164,frame6c; /* frame word is low16. */
} Nba97GameWaitLiveState;
enum Nba97GameWaitOwner { NBA97_WAIT_64914=0,NBA97_WAIT_64964=1 };
enum Nba97GameWaitBoundary { NBA97_WAIT_POLL_70068=0,NBA97_WAIT_QUERY_31CB8,
                           NBA97_WAIT_PUMP_2DD84,NBA97_WAIT_STOP_31C8C };
typedef struct Nba97GameWaitCursor {
    uint8_t owner,stage,terminal; /* terminal0 running,1 completed,2 failed. */
} Nba97GameWaitCursor;
typedef int (*Nba97GameWaitCallback)(void* context,Nba97GameWaitLiveState* state,
    unsigned boundary,Nba97GameClockValue* reply);
enum Nba97GameWaitResult {
    NBA97_WAIT_PROGRESS=1,NBA97_WAIT_COMPLETE=2,NBA97_WAIT_ARGUMENT=0,
    NBA97_WAIT_CALLBACK_REQUIRED=-1,NBA97_WAIT_CALLBACK_FAILED=-2,
    NBA97_WAIT_RETURN_UNKNOWN=-3,NBA97_WAIT_ALREADY_FAILED=-4
};
/* Full64914(20 instructions)/64964(29), including nested64914 and native clock
 * calls. One step performs one clock operation or synchronous external call,
 * making original waiting loops resumable without adding a timeout or spinning
 * the host. Caller refreshes sample_d7a70 from the actual source clock as needed.
 * 70068 consumes full reply word;31CB8 consumes low8. Pump/stop returns ignored.
 * Callbacks must execute actual owners and expose changed live state before
 * returning1. Unfinished boundaries are not successful no-ops. Null callback
 * pauses before that call. Failed/unknown-result calls retain mutations and
 * permanently fail this cursor; do not blindly replay it. Unknown clock writes
 * retain unknown provenance; they do not authorize writing native zero.
 * Cursor/live state must be distinct; neither is a copied MIPS register file. */
int nba97_game_wait_begin(Nba97GameWaitCursor* cursor,unsigned owner);
int nba97_game_wait_step(Nba97GameWaitCursor* cursor,Nba97GameWaitLiveState* state,
                         Nba97GameWaitCallback callback,void* context);

#ifdef __cplusplus
}
#endif
#endif
