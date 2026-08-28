#ifndef NBA97_RECOVERED_ROSTER_REORDER_H
#define NBA97_RECOVERED_ROSTER_REORDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_TEAM_SLOTS = 15 };
typedef enum Nba97ReorderResult {
    NBA97_REORDER_CHANGED,
    NBA97_REORDER_SAME_PLAYER,
    NBA97_REORDER_EMPTY_SLOT,
    NBA97_REORDER_INVALID_ARGUMENT
} Nba97ReorderResult;

/* FUN_80056A94. The special team's identity is a signed byte, not a clamp. */
int16_t nba97_reorder_normalize_team(int16_t requested, int16_t mode,
                                    int8_t context_team);

/* Re-order adapter over the shared roster_lists.c validation/mutation helpers.
 * Caller supplies one team's fixed slots; no cross-team UI implied.
 * Rejections leave both slots and the 16-bit session change counter untouched.
 * The counter is not a persistent dirty flag (it can wrap).
 * No allocation, assets, saves, or rendering in the recovered C layer. */
Nba97ReorderResult nba97_reorder_swap(uint16_t slots[NBA97_TEAM_SLOTS],
                                    int source, int destination,
                                    uint16_t *session_changes);
const char *nba97_reorder_result_name(Nba97ReorderResult result);

typedef enum Nba97ReorderPhase {
    NBA97_REORDER_FIRST, NBA97_REORDER_REPLACEMENT,
    NBA97_REORDER_DISCARD_PROMPT, NBA97_REORDER_CLOSED
} Nba97ReorderPhase;
typedef enum Nba97ReorderAction {
    NBA97_REORDER_UP, NBA97_REORDER_DOWN, NBA97_REORDER_SELECT,
    NBA97_REORDER_CANCEL, NBA97_REORDER_ACCEPT,
    NBA97_REORDER_DISCARD_YES, NBA97_REORDER_DISCARD_NO
} Nba97ReorderAction;
typedef enum Nba97ReorderEvent {
    NBA97_REORDER_NO_CHANGE, NBA97_REORDER_MOVED, NBA97_REORDER_PICKED,
    NBA97_REORDER_SWAPPED, NBA97_REORDER_REJECTED_EMPTY, NBA97_REORDER_REJECTED_SAME,
    NBA97_REORDER_CANCELLED_PICK, NBA97_REORDER_ASK_DISCARD,
    NBA97_REORDER_RESUMED, NBA97_REORDER_EXIT_DISCARDED, NBA97_REORDER_EXIT_ACCEPTED,
    NBA97_REORDER_REQUEST_VIEW, NBA97_REORDER_REQUEST_COMPARE
} Nba97ReorderEvent;

typedef enum Nba97ReorderModal {
    NBA97_REORDER_MODAL_NONE, NBA97_REORDER_MODAL_EMPTY,
    NBA97_REORDER_MODAL_VIEW_EMPTY, NBA97_REORDER_MODAL_COMPARE_EMPTY
} Nba97ReorderModal;

/* Native text-object color state: FUN_8002AB30/8002AC2C/8002AE5C.
 * These are modulation values (128 is neutral), not final RGB pixels. */
typedef struct Nba97ReorderTint {
    uint8_t start[3], alternate[3], target[3], rgb[3];
    uint8_t duration, elapsed, flags;
} Nba97ReorderTint;
/* Shared original text-object animation, also used by choice dialogs. */
void nba97_reorder_tint_pulse(Nba97ReorderTint*);
void nba97_reorder_tint_unpulse(Nba97ReorderTint*);
/*2ADEC arrow flash: fade4, hold10 (+transition updates), return4.
 * Retrigger during hold resets the hold clock; initial start RGB is caller-owned. */
void nba97_reorder_tint_flash(Nba97ReorderTint*);
void nba97_reorder_tint_tick(Nba97ReorderTint*);

typedef struct Nba97ReorderSession {
    uint16_t slots[NBA97_TEAM_SLOTS];
    uint16_t original[NBA97_TEAM_SLOTS];
    uint16_t changes;
    uint8_t cursor[2];
    uint8_t top[2];
    uint8_t input_latch;
    Nba97ReorderPhase phase;
    int accepted;
    uint16_t row_ids[2][NBA97_TEAM_SLOTS], selected_ids[2];
    uint32_t row_revision, header_revision;
    uint32_t visible_redraws, presentation_requests;
    uint8_t active_page, descriptor_page, object_state;
    Nba97ReorderTint tint[2][NBA97_TEAM_SLOTS];
    Nba97ReorderModal modal;
    /* FUN_80054B94 returns 2/3 to the outer dispatcher; it does not run a child. */
    uint8_t screen_result;
    uint16_t child_ids[2];
    uint8_t waiting_input_change;
    uint16_t held_mask;
} Nba97ReorderSession;

/* Owns an isolated team working copy. The host publishes only on ACCEPT.
 * CANCEL in replacement phase never calls the mutation primitive.
 * CANCEL in first phase asks before discarding any completed swaps.
 * Presentation state is consumed by both diagnostic and native screen renderers.
 * Multi-team transaction/menu integration lives in reorder_screen.h. */
int nba97_reorder_begin(Nba97ReorderSession *session, const uint16_t slots[NBA97_TEAM_SLOTS]);
Nba97ReorderEvent nba97_reorder_input(Nba97ReorderSession *session, Nba97ReorderAction action);
/* First-list callback AFTER generic navigation/cancel processing. Exact values,
 * not bit tests. View/Compare validate IDs and return original dispatcher codes. */
Nba97ReorderEvent nba97_reorder_first_callback(Nba97ReorderSession *session, uint16_t input_mask);
/* Replacement callback AFTER generic input processing. Original object state
 * 2 means a synthesized cancel-confirm; every other byte follows validation.
 * In particular raw 0x100 here is NOT a cancel (the generic loop translates it).
 * The outer game-window child screens are outside this callback's ownership. */
Nba97ReorderEvent nba97_reorder_second_callback(Nba97ReorderSession *session,
                                               uint16_t input_mask, uint8_t object_state);
void nba97_reorder_begin_second(Nba97ReorderSession *session);
void nba97_reorder_finish_second(Nba97ReorderSession *session, uint8_t object_state);
void nba97_reorder_refresh(Nba97ReorderSession *session);
/* One original UI update, not elapsed milliseconds. Return 0 while the
 * FUN_8003B194 input-change barrier is waiting. Keep rendering while waiting. */
int nba97_reorder_frame(Nba97ReorderSession *session, uint16_t held_mask);
void nba97_reorder_dismiss_modal(Nba97ReorderSession *session);
void nba97_reorder_clear_screen_result(Nba97ReorderSession *session);
/* Generic screen loop's first-list focus pulse (not callback-owned). */
void nba97_reorder_focus_first(Nba97ReorderSession *session);
const char *nba97_reorder_phase_name(Nba97ReorderPhase phase);
const char *nba97_reorder_event_name(Nba97ReorderEvent event);

#ifdef __cplusplus
}
#endif
#endif
