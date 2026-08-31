#ifndef NBA97_MUSIC_TRANSITION_H
#define NBA97_MUSIC_TRANSITION_H
#include "music_routing.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97MusicTransition {
    uint32_t saved_volume;    /* F8F64, restored through low byte only */
    uint32_t resource_handle; /* F84C8, passed to7760C even when zero */
    uint16_t transition_guard;/* ED270, resource1F exception */
    uint8_t other_volume;     /* 21D7D, original adjacent option byte */
} Nba97MusicTransition;
typedef enum Nba97MusicTransitionCall {
    NBA97_MUSIC_TRANSITION_FINISHED, /*6FCF0, no arguments */
    NBA97_MUSIC_TRANSITION_FADE,     /*7B2BC(voice,50,-1) */
    NBA97_MUSIC_TRANSITION_RELEASE   /*7760C(resource_handle) */
} Nba97MusicTransitionCall;
typedef uint32_t (*Nba97MusicTransitionInvoke)(void*, Nba97MusicTransitionCall,
    uint32_t a0, uint32_t a1, uint32_t a2);

/*31ADC..31BF8 music portion of31A88, AFTER its preceding callee effects.
 * Mutates ED2AC/F9720 through inputs, F97B8 through routing, and21D7C through
 * inputs.volume. Synchronous callbacks may mutate live state; no recursive
 * invocation. All pointers required, refusal returns0 before any mutation.
 * Does not run resource-load/dispatch callees between this arm and31F10. */
int nba97_music_transition_begin(Nba97MusicTransition*, Nba97MusicRouting*,
    Nba97MusicInputs*, uint32_t resource_state, Nba97MusicTransitionInvoke, void*);
/*31F10..31F20, AFTER resource dispatch31F48 has returned. State24 preserves
 * selection_blocked exactly as the dispatch left it; other states clear it.
 * Do not unblock24 automatically or infer gameplay Pause from this owner. */
int nba97_music_transition_end(Nba97MusicInputs*, uint32_t resource_state);

#ifdef __cplusplus
}
#endif
#endif
