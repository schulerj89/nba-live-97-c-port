#include "roster_reorder.h"
#include <stddef.h>
#include <string.h>

int16_t nba97_reorder_normalize_team(int16_t requested, int16_t mode,
                                    int8_t context_team) {
    if (requested != 29) return requested;
    return mode == 2 ? (int16_t)context_team : 3;
}

Nba97ReorderResult nba97_reorder_swap(uint16_t slots[NBA97_TEAM_SLOTS],
                                    int source, int destination,
                                    uint16_t *session_changes) {
    uint16_t first, second;
    /* Host safety guards precede all reads. These are not original-MIPS credit. */
    if (slots == NULL || session_changes == NULL || source < 0 ||
        source >= NBA97_TEAM_SLOTS || destination < 0 || destination >= NBA97_TEAM_SLOTS)
        return NBA97_REORDER_INVALID_ARGUMENT;
    first = slots[source];
    second = slots[destination];
    /* FUN_800556B0 list-kind 2: both players must exist and be different.
     * Empty-slot rejection takes precedence over same-player rejection. */
    if (first == UINT16_MAX || second == UINT16_MAX)
        return NBA97_REORDER_EMPTY_SLOT;
    if (first == second) return NBA97_REORDER_SAME_PLAYER;
    /* FUN_800558E0 occupied/occupied path. No compaction or membership change. */
    slots[source] = second;
    slots[destination] = first;
    *session_changes = (uint16_t)(*session_changes + 1u);
    return NBA97_REORDER_CHANGED;
}

const char *nba97_reorder_result_name(Nba97ReorderResult result) {
    switch (result) {
    case NBA97_REORDER_CHANGED: return "changed";
    case NBA97_REORDER_SAME_PLAYER: return "same-player";
    case NBA97_REORDER_EMPTY_SLOT: return "empty-slot";
    case NBA97_REORDER_INVALID_ARGUMENT: return "invalid-argument";
    default: return "unknown";
    }
}

int nba97_reorder_begin(Nba97ReorderSession *session, const uint16_t slots[NBA97_TEAM_SLOTS]) {
    Nba97ReorderSession fresh;
    if (!session || !slots) return 0;
    memset(&fresh, 0, sizeof(fresh));
    memcpy(fresh.slots, slots, sizeof(fresh.slots));
    memcpy(fresh.original, slots, sizeof(fresh.original));
    fresh.phase = NBA97_REORDER_FIRST;
    *session = fresh;
    return 1;
}

Nba97ReorderEvent nba97_reorder_input(Nba97ReorderSession *session, Nba97ReorderAction action) {
    unsigned active;
    Nba97ReorderResult result;
    if (!session || session->phase == NBA97_REORDER_CLOSED) return NBA97_REORDER_NO_CHANGE;
    if (session->phase == NBA97_REORDER_DISCARD_PROMPT) {
        if (action == NBA97_REORDER_DISCARD_NO || action == NBA97_REORDER_CANCEL) {
            session->phase = NBA97_REORDER_FIRST;
            return NBA97_REORDER_RESUMED;
        }
        if (action != NBA97_REORDER_DISCARD_YES) return NBA97_REORDER_NO_CHANGE;
        /* FUN_80056254 restores the entry snapshot only after confirmed discard.
         * One team is editable in this slice, so only that team's 15 IDs differ. */
        memcpy(session->slots, session->original, sizeof(session->slots));
        session->phase = NBA97_REORDER_CLOSED;
        return NBA97_REORDER_EXIT_DISCARDED;
    }
    active = session->phase == NBA97_REORDER_REPLACEMENT ? 1u : 0u;
    if (action == NBA97_REORDER_UP || action == NBA97_REORDER_DOWN) {
        int next = (int)session->cursor[active] + (action == NBA97_REORDER_UP ? -1 : 1);
        if (next < 0 || next >= NBA97_TEAM_SLOTS) return NBA97_REORDER_NO_CHANGE;
        session->cursor[active] = (uint8_t)next;
        if (next < session->top[active]) session->top[active] = (uint8_t)next;
        if (next >= session->top[active] + 6) session->top[active] = (uint8_t)(next - 5);
        return NBA97_REORDER_MOVED;
    }
    if (action == NBA97_REORDER_CANCEL) {
        if (active) {
            /* Generic FUN_8003D930: cancel turns object state 1 into 2 and
             * synthesizes confirm. FUN_800569BC block 0x80056A24 bypasses
             * validation/swap; FUN_80055314 restores first-list focus.
             * Its state-2 branch sets +0x1B=10. Cursor/scroll memory survives. */
            session->phase = NBA97_REORDER_FIRST;
            session->input_latch = 10;
            return NBA97_REORDER_CANCELLED_PICK;
        }
        if (session->changes != 0) {
            session->phase = NBA97_REORDER_DISCARD_PROMPT;
            return NBA97_REORDER_ASK_DISCARD;
        }
        session->phase = NBA97_REORDER_CLOSED;
        return NBA97_REORDER_EXIT_DISCARDED;
    }
    if (action == NBA97_REORDER_ACCEPT) {
        /* Original Start/continue (0x80) exits only at selection state zero. */
        if (active) return NBA97_REORDER_NO_CHANGE;
        session->accepted = 1;
        session->phase = NBA97_REORDER_CLOSED;
        return NBA97_REORDER_EXIT_ACCEPTED;
    }
    if (action != NBA97_REORDER_SELECT) return NBA97_REORDER_NO_CHANGE;
    if (!active) {
        /* FUN_800568E4 block 0x80056948: selected ID load and -1 test. */
        if (session->slots[session->cursor[0]] == UINT16_MAX)
            return NBA97_REORDER_REJECTED_EMPTY;
        session->phase = NBA97_REORDER_REPLACEMENT;
        return NBA97_REORDER_PICKED;
    }
    result = nba97_reorder_swap(session->slots, session->cursor[0], session->cursor[1], &session->changes);
    if (result != NBA97_REORDER_CHANGED) {
        /* FUN_800569BC block 0x80056A78 clears +0x1B on failed validation. */
        session->input_latch = 0;
        return result == NBA97_REORDER_EMPTY_SLOT ? NBA97_REORDER_REJECTED_EMPTY : NBA97_REORDER_REJECTED_SAME;
    }
    session->phase = NBA97_REORDER_FIRST;
    return NBA97_REORDER_SWAPPED;
}

Nba97ReorderEvent nba97_reorder_first_callback(Nba97ReorderSession *session, uint16_t input_mask) {
    if (!session || session->phase != NBA97_REORDER_FIRST) return NBA97_REORDER_NO_CHANGE;
    /* FUN_800568E4 checks 0x10, then 0x40 before checking confirm. These
     * return requests only: FUN_80054B94 availability and child dispatch are
     * intentionally not counted as recovered by these comparisons. */
    if (input_mask == 0x10) return NBA97_REORDER_REQUEST_VIEW;
    if (input_mask == 0x40) return NBA97_REORDER_REQUEST_COMPARE;
    if (input_mask == 0x800) return nba97_reorder_input(session, NBA97_REORDER_SELECT);
    /* All other values, including combined masks, clear the original +0x1B
     * input latch. Navigation and cancellation were handled by the caller. */
    session->input_latch = 0;
    return NBA97_REORDER_NO_CHANGE;
}

Nba97ReorderEvent nba97_reorder_second_callback(Nba97ReorderSession *session,
                                               uint16_t input_mask, uint8_t object_state) {
    /* FUN_800569BC/800569E4: typed arguments replace input/context loads and
     * the native C frame replaces saved MIPS registers. No emulated stack. */
    if (!session || session->phase != NBA97_REORDER_REPLACEMENT)
        return NBA97_REORDER_NO_CHANGE;
    if (input_mask == 0x10) return NBA97_REORDER_REQUEST_VIEW;
    if (input_mask == 0x40) return NBA97_REORDER_REQUEST_COMPARE;
    /* 800569F8/80056A0C/80056A14/80056A1C: exact confirm and preservation
     * checks; only non-View/Compare/confirm callback values clear +0x1B. */
    if (input_mask != 0x800) {
        session->input_latch = 0;
        return NBA97_REORDER_NO_CHANGE;
    }
    /* 80056A24 already accounted: generic cancel state 2 skips mutation.
     * The remaining visual refresh/finish calls are still unimplemented.
     * 80056A7C is represented by ordinary C return to the caller on all paths,
     * without leaking frame/register state or overwriting unrelated data. */
    return nba97_reorder_input(session,
        object_state == 2 ? NBA97_REORDER_CANCEL : NBA97_REORDER_SELECT);
}

const char *nba97_reorder_phase_name(Nba97ReorderPhase phase) {
    switch (phase) {
    case NBA97_REORDER_FIRST: return "first-player";
    case NBA97_REORDER_REPLACEMENT: return "replacement";
    case NBA97_REORDER_DISCARD_PROMPT: return "confirm-discard";
    case NBA97_REORDER_CLOSED: return "closed";
    default: return "invalid";
    }
}
const char *nba97_reorder_event_name(Nba97ReorderEvent event) {
    switch (event) {
    case NBA97_REORDER_MOVED: return "moved";
    case NBA97_REORDER_PICKED: return "picked-first";
    case NBA97_REORDER_SWAPPED: return "swapped";
    case NBA97_REORDER_REJECTED_EMPTY: return "rejected-empty";
    case NBA97_REORDER_REJECTED_SAME: return "rejected-same-player";
    case NBA97_REORDER_CANCELLED_PICK: return "cancelled-replacement";
    case NBA97_REORDER_ASK_DISCARD: return "ask-discard";
    case NBA97_REORDER_RESUMED: return "resumed";
    case NBA97_REORDER_EXIT_DISCARDED: return "exit-discarded";
    case NBA97_REORDER_EXIT_ACCEPTED: return "exit-accepted";
    case NBA97_REORDER_REQUEST_VIEW: return "request-view-child-pending";
    case NBA97_REORDER_REQUEST_COMPARE: return "request-compare-child-pending";
    default: return "no-change";
    }
}
