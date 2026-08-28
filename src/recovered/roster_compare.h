#ifndef NBA97_ROSTER_COMPARE_H
#define NBA97_ROSTER_COMPARE_H
#include "reorder_children.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Normal Re-order child only. It borrows a validated 535-slot draft table;
 * it never owns/copies player records or modifies any roster. Special-season
 * eligibility and Trade writeback are deliberately outside this API. */
typedef struct Nba97Compare {
    uint16_t player[2];
    uint8_t team[2], slot[2];
    uint8_t active_side, layer, top, initialized;
} Nba97Compare;
typedef enum Nba97CompareEvent {
    NBA97_COMPARE_NO_CHANGE, NBA97_COMPARE_PLAYER, NBA97_COMPARE_TEAM,
    NBA97_COMPARE_SIDE, NBA97_COMPARE_SCROLL, NBA97_COMPARE_LAYER,
    NBA97_COMPARE_INVALID
} Nba97CompareEvent;

int nba97_compare_begin(Nba97Compare*, const Nba97ReorderChild*, const uint16_t *table);
int nba97_compare_begin_teams(Nba97Compare*,const int16_t teams[2],const uint8_t slots[2],const uint16_t *table);
Nba97CompareEvent nba97_compare_input(Nba97Compare*, const uint16_t *table, uint16_t mask);
unsigned nba97_compare_stat_count(const Nba97Compare*);

/* 59928 -> 39574(0,2) -> 59808, then selector sound. Text is retained
 * independently from the requested portrait identity. Host calls presented
 * only for completed logical presentations, not elapsed catch-up ticks.
 * Original asynchronous CD portrait completion is deliberately not modeled. */
typedef struct Nba97CompareRefresh {
    Nba97Compare text;
    uint8_t remaining, cue;
} Nba97CompareRefresh;
int nba97_compare_refresh_begin(Nba97CompareRefresh*, const Nba97Compare*);
Nba97CompareEvent nba97_compare_refresh_input(Nba97Compare*, Nba97CompareRefresh*,
    const uint16_t *table, uint16_t mask);
/* Scroll3AB64 invokes3A650 once per group: old/old, new/old, new/new.
 * This projection requires original glyph heights <=14: clip2BBA0 hides the
 * incoming row completely;2B5AC translates one full row in one update, and
 * lifetime0 removes the outgoing row before geometry. Not a general tween.
 * Labels belong to group0. Only actual completed presentations advance it. */
unsigned nba97_compare_refresh_top(const Nba97CompareRefresh*, unsigned side);
/* 0 while waiting/idle, cue1..4 once when callback completes; -1 invalid. */
int nba97_compare_refresh_presented(Nba97CompareRefresh*, const Nba97Compare*);

/* Normal frontend (state35) selector pacing from3AE4C/3E38C.
 * Scope: context+720==0, controller0. The normal polling branch records the
 * accepted input at3AFD0..3B034, then the COMMON tail records it again at
 * 3B0B0..3B104. Fresh/reversed input ends at2; a repeated input advances4
 * (two separately capped +2 passes), up to48. Do not collapse to one pass.
 * post_frames includes the mandatory one-frame input poll after the delay;
 * the separate two-frame player callback must finish before consuming these.
 * After59928/59808/3B26C,2C6B0 initializes the selected value object's flags
 * to zero and2C244(copy=2) retains only0xC7. Thus its2C610 wait is clear.
 * Generic callbacks use the fixed wait below. Compare scroll geometry has
 * duration1, already complete after its three post-delay presentations. */
typedef struct Nba97CompareRepeat {
    uint16_t previous_mask;
    uint8_t counter, post_frames;
} Nba97CompareRepeat;
int nba97_compare_repeat_request(Nba97CompareRepeat*, uint16_t mask);
/* Pass state BEFORE navigation. Returns total post/poll presentations:
 * 1 for top-Up (5A1EC null callback), 4 for dispatched scroll (delay3+poll1),
 * 0 for invalid/busy. Bottom Down dispatches even when it cannot scroll.
 * Both endpoints are silent; neither has internal scroll presentations. */
int nba97_compare_scroll_request(Nba97CompareRepeat*, const Nba97Compare*, uint16_t mask);
/*3D930 generic selector callback: mask intersects3E50;59F20 handles only
 * exact team/layer/side masks for Compare, clearing the cue otherwise.
 * Even a silent/no-op callback pumps five frames, then one input poll.
 * Shares the normal two-pass counter history with directional requests. */
int nba97_compare_callback_mask(uint16_t mask);
int nba97_compare_callback_request(Nba97CompareRepeat*, uint16_t mask);
int nba97_compare_repeat_presented(Nba97CompareRepeat*);
void nba97_compare_repeat_idle(Nba97CompareRepeat*);
/* 2C610: display-object animation predicate, NOT a button-release test.
 * Raw byte fields from object+3B,+26,+27,+29,+2A. The latter pair is signed.
 * A pending0x10 channel replaces (does not OR) the pending0x08 result.
 * Caller must finish this barrier BEFORE the mandatory input-poll frame.
 * Player-cycle rebuild clears these wait bits; other animation-object
 * lifecycles remain separate from the pacing adapter. */
unsigned nba97_compare_animation_pending(uint8_t flags, uint8_t progress8,
    uint8_t limit8, uint8_t progress16, uint8_t limit16);
/*2C244(copy=2), only the flags projection, not geometry/color copying. */
uint8_t nba97_compare_rebuilt_text_flags(uint8_t previous_flags);
#ifdef __cplusplus
}
#endif
#endif
