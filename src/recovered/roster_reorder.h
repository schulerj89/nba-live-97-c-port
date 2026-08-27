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

/* Re-order-only paths of FUN_800556B0 and FUN_800558E0, NOT their full
 * shared Trade/Sign/Release behavior. Caller supplies one team's fixed slots.
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

typedef struct Nba97ReorderSession {
    uint16_t slots[NBA97_TEAM_SLOTS];
    uint16_t original[NBA97_TEAM_SLOTS];
    uint16_t changes;
    uint8_t cursor[2];
    uint8_t top[2];
    uint8_t input_latch;
    Nba97ReorderPhase phase;
    int accepted;
} Nba97ReorderSession;

/* Owns an isolated team working copy. The host publishes only on ACCEPT.
 * CANCEL in replacement phase never calls the mutation primitive.
 * CANCEL in first phase asks before discarding any completed swaps.
 * Rendering/audio effects are deliberately not implemented by this controller. */
int nba97_reorder_begin(Nba97ReorderSession *session, const uint16_t slots[NBA97_TEAM_SLOTS]);
Nba97ReorderEvent nba97_reorder_input(Nba97ReorderSession *session, Nba97ReorderAction action);
/* First-list callback AFTER generic navigation/cancel processing. Exact values,
 * not bit tests. View/Compare return requests; their child screens are pending. */
Nba97ReorderEvent nba97_reorder_first_callback(Nba97ReorderSession *session, uint16_t input_mask);
/* Replacement callback AFTER generic input processing. Original object state
 * 2 means a synthesized cancel-confirm; every other byte follows validation.
 * In particular raw 0x100 here is NOT a cancel (the generic loop translates it).
 * Child-screen requests still require a host consumer, which is pending. */
Nba97ReorderEvent nba97_reorder_second_callback(Nba97ReorderSession *session,
                                               uint16_t input_mask, uint8_t object_state);
const char *nba97_reorder_phase_name(Nba97ReorderPhase phase);
const char *nba97_reorder_event_name(Nba97ReorderEvent event);

#ifdef __cplusplus
}
#endif
#endif
