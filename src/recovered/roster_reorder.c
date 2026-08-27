#include "roster_reorder.h"
#include "roster_lists.h"
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
    uint16_t counts[30] = {0};
    Nba97RosterList lists[2];
    Nba97RosterValidation rules = {0};
    Nba97RosterDecision decision;
    /* Host safety guards precede all reads. These are not original-MIPS credit. */
    if (slots == NULL || session_changes == NULL || source < 0 ||
        source >= NBA97_TEAM_SLOTS || destination < 0 || destination >= NBA97_TEAM_SLOTS)
        return NBA97_REORDER_INVALID_ARGUMENT;
    lists[0] = (Nba97RosterList){slots, 15, 0, 15, 1, 0, (uint8_t)source, 0};
    lists[1] = (Nba97RosterList){slots, 15, 0, 15, 2, 15, (uint8_t)(15 + destination), 15};
    for (int i = 0; i < 15; ++i) if (slots[i] != UINT16_MAX) ++counts[0];
    rules.counts = counts;
    rules.frontend_state = 12;
    decision = nba97_roster_validate(lists, &rules);
    if (decision.result != 1) {
        /* Public adapter preserves its historical EMPTY result. The shared
         * validator distinguishes silent both-empty from the one-empty modal. */
        return slots[source] == UINT16_MAX || slots[destination] == UINT16_MAX ?
            NBA97_REORDER_EMPTY_SLOT : NBA97_REORDER_SAME_PLAYER;
    }
    nba97_roster_mutate(lists, counts, session_changes, NULL, NULL);
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

static void refresh_header(Nba97ReorderSession *s) {
    /* FUN_80055068, Re-order state 0x0C only: redraw team object 0x73 and
     * the natural-position line. Renderers read the selected working IDs. */
    s->selected_ids[0] = s->slots[s->cursor[0]];
    s->selected_ids[1] = s->slots[s->cursor[1]];
    ++s->header_revision;
}

typedef struct ReorderRefreshAdapter {
    Nba97ReorderSession *session;
    Nba97RosterRefresh *refresh;
} ReorderRefreshAdapter;
static void refresh_sink(void *user, Nba97RosterRefreshEvent event,
                         int page, int object, int32_t player) {
    ReorderRefreshAdapter *adapter = (ReorderRefreshAdapter *)user;
    Nba97ReorderSession *s = adapter->session;
    switch (event) {
    case NBA97_ROSTER_BIND:
        s->row_ids[page][object - page * 15] = (uint16_t)player;
        break;
    case NBA97_ROSTER_REDRAW: ++s->visible_redraws; break;
    case NBA97_ROSTER_PRESENT:
        /* Retained native surface is presented by the host's next frame.
         * Count requests separately: not a claim of PSX vblank equivalence. */
        ++s->presentation_requests;
        ++s->row_revision;
        break;
    case NBA97_ROSTER_HEADER:
        s->descriptor_page = (uint8_t)adapter->refresh->descriptor_page;
        s->selected_ids[0] = (uint16_t)adapter->refresh->selected[0];
        s->selected_ids[1] = (uint16_t)adapter->refresh->selected[1];
        ++s->header_revision;
        break;
    }
}
void nba97_reorder_refresh(Nba97ReorderSession *s) {
    Nba97RosterRefresh refresh = {0};
    ReorderRefreshAdapter adapter = {s, &refresh};
    if (!s) return;
    for (int page = 0; page < 2; ++page)
        refresh.lists[page] = (Nba97RosterList){s->slots, 15, 0, 15, (int16_t)(page + 1),
            (uint8_t)(page * 15), (uint8_t)(page * 15 + s->cursor[page]),
            (uint8_t)(page * 15 + s->top[page])};
    refresh.descriptor_page = s->descriptor_page;
    refresh.visible_rows = 6;
    refresh.sink = refresh_sink;
    refresh.user = &adapter;
    nba97_roster_refresh_lists(&refresh, 2);
}

static void pulse(Nba97ReorderTint *t) {
    static const uint8_t gold[3] = {120, 102, 0};
    static const uint8_t neutral[3] = {128, 128, 128};
    memcpy(t->start, gold, 3);
    memcpy(t->target, gold, 3);
    memcpy(t->alternate, neutral, 3);
    t->duration = 20;
    t->elapsed = 0;
    t->flags = (uint8_t)((t->flags & 0xfb) | 3);
    /* No immediate draw: original applies the color on the next UI update. */
}

static void unpulse(Nba97ReorderTint *t) {
    if (t->flags & 2) {
        /* Verified in BOTH recomp and Ghidra: original writes blue twice,
         * green once, and does NOT replace start red here. Preserve the quirk. */
        t->start[1] = t->target[1];
        t->start[2] = t->target[2];
    }
    memset(t->target, 128, 3);
    t->duration = 8;
    t->elapsed = 0;
    t->flags = (uint8_t)((t->flags & 0xfe) | 2);
}

void nba97_reorder_focus_first(Nba97ReorderSession *s) {
    if (s && s->cursor[0] < 15) pulse(&s->tint[0][s->cursor[0]]);
}

static void tint_frame(Nba97ReorderTint *t) {
    unsigned c;
    if (!(t->flags & 2)) return;
    ++t->elapsed;
    if (t->elapsed > t->duration) {
        if (t->flags & 1) {
            for (c = 0; c < 3; ++c) {
                t->start[c] = t->target[c];
                t->target[c] = t->alternate[c];
                t->alternate[c] = t->start[c];
            }
            t->elapsed = 0;
        } else {
            t->flags &= 0xf8;
            t->elapsed = t->duration;
            t->start[1] = t->target[1];
            t->start[2] = t->target[2];
            return; /* Native single surface retains the previous final color. */
        }
    }
    for (c = 0; c < 3; ++c)
        t->rgb[c] = (uint8_t)(t->start[c] +
            ((int)t->target[c] - (int)t->start[c]) * t->elapsed / t->duration);
}

void nba97_reorder_begin_second(Nba97ReorderSession *s) {
    if (!s || s->phase != NBA97_REORDER_FIRST) return;
    s->descriptor_page = s->active_page = s->object_state = 1;
    s->phase = NBA97_REORDER_REPLACEMENT;
    pulse(&s->tint[1][s->cursor[1]]);
    refresh_header(s);
}

void nba97_reorder_finish_second(Nba97ReorderSession *s, uint8_t object_state) {
    if (!s || s->phase != NBA97_REORDER_REPLACEMENT) return;
    unpulse(&s->tint[1][s->cursor[1]]);
    s->active_page = s->descriptor_page = s->object_state = 0;
    s->phase = NBA97_REORDER_FIRST;
    refresh_header(s);
    if (object_state == 2) {
        /* Nonblocking equivalent of pumping frames until raw held input differs.
         * The abstract action API represents discrete presses; frame-driven hosts
         * must honor nba97_reorder_frame's result before dispatching callbacks. */
        s->waiting_input_change = 1;
        s->held_mask = 0x100;
        s->input_latch = 10;
    }
}

int nba97_reorder_frame(Nba97ReorderSession *s, uint16_t held_mask) {
    unsigned page, row;
    if (!s) return 0;
    for (page = 0; page < 2; ++page)
        for (row = 0; row < NBA97_TEAM_SLOTS; ++row) tint_frame(&s->tint[page][row]);
    if (s->waiting_input_change) {
        if (held_mask == s->held_mask) return 0;
        s->waiting_input_change = 0;
    }
    s->held_mask = held_mask;
    return 1;
}

void nba97_reorder_dismiss_modal(Nba97ReorderSession *s) {
    if (!s || s->modal == NBA97_REORDER_MODAL_NONE) return;
    s->modal = NBA97_REORDER_MODAL_NONE;
    s->input_latch = 0; /* FUN_80040A1C epilogue. */
    s->waiting_input_change = 1;
}

void nba97_reorder_clear_screen_result(Nba97ReorderSession *s) {
    if (!s) return;
    s->screen_result = 0;
    s->child_ids[0] = s->child_ids[1] = UINT16_MAX;
}

static Nba97ReorderEvent child_request(Nba97ReorderSession *s, uint16_t mask) {
    uint16_t a = s->slots[s->cursor[0]], b = s->slots[s->cursor[1]];
    unsigned active = s->phase == NBA97_REORDER_REPLACEMENT ? 1u : 0u;
    if ((mask == 0x10 && (active ? b : a) == UINT16_MAX) ||
        (mask == 0x40 && (a == UINT16_MAX || b == UINT16_MAX))) {
        s->modal = mask == 0x10 ? NBA97_REORDER_MODAL_VIEW_EMPTY : NBA97_REORDER_MODAL_COMPARE_EMPTY;
        return NBA97_REORDER_REJECTED_EMPTY;
    }
    s->screen_result = mask == 0x10 ? 2 : 3;
    s->child_ids[0] = mask == 0x10 ? (active ? b : a) : a;
    s->child_ids[1] = mask == 0x10 ? UINT16_MAX : b;
    return mask == 0x10 ? NBA97_REORDER_REQUEST_VIEW : NBA97_REORDER_REQUEST_COMPARE;
}

int nba97_reorder_begin(Nba97ReorderSession *session, const uint16_t slots[NBA97_TEAM_SLOTS]) {
    Nba97ReorderSession fresh;
    if (!session || !slots) return 0;
    memset(&fresh, 0, sizeof(fresh));
    memcpy(fresh.slots, slots, sizeof(fresh.slots));
    memcpy(fresh.original, slots, sizeof(fresh.original));
    fresh.phase = NBA97_REORDER_FIRST;
    for (unsigned page = 0; page < 2; ++page)
        for (unsigned row = 0; row < NBA97_TEAM_SLOTS; ++row) {
            memset(fresh.tint[page][row].start, 128, 3);
            memset(fresh.tint[page][row].rgb, 128, 3);
        }
    nba97_reorder_clear_screen_result(&fresh);
    nba97_reorder_refresh(&fresh);
    *session = fresh;
    return 1;
}

Nba97ReorderEvent nba97_reorder_input(Nba97ReorderSession *session, Nba97ReorderAction action) {
    unsigned active;
    Nba97ReorderResult result;
    if (!session || session->phase == NBA97_REORDER_CLOSED) return NBA97_REORDER_NO_CHANGE;
    if (session->modal != NBA97_REORDER_MODAL_NONE) return NBA97_REORDER_NO_CHANGE;
    if (session->phase == NBA97_REORDER_DISCARD_PROMPT) {
        if (action == NBA97_REORDER_DISCARD_NO || action == NBA97_REORDER_CANCEL) {
            session->phase = NBA97_REORDER_FIRST;
            return NBA97_REORDER_RESUMED;
        }
        if (action != NBA97_REORDER_DISCARD_YES) return NBA97_REORDER_NO_CHANGE;
        /* FUN_80056254 restores the entry snapshot only after confirmed discard.
         * One team is editable in this slice, so only that team's 15 IDs differ. */
        memcpy(session->slots, session->original, sizeof(session->slots));
        nba97_reorder_refresh(session);
        session->phase = NBA97_REORDER_CLOSED;
        return NBA97_REORDER_EXIT_DISCARDED;
    }
    active = session->phase == NBA97_REORDER_REPLACEMENT ? 1u : 0u;
    if (action == NBA97_REORDER_UP || action == NBA97_REORDER_DOWN) {
        int next = (int)session->cursor[active] + (action == NBA97_REORDER_UP ? -1 : 1);
        if (next < 0 || next >= NBA97_TEAM_SLOTS) return NBA97_REORDER_NO_CHANGE;
        const int has_focus = active || (session->tint[0][session->cursor[0]].flags & 1);
        if (has_focus) unpulse(&session->tint[active][session->cursor[active]]);
        session->cursor[active] = (uint8_t)next;
        if (has_focus) pulse(&session->tint[active][next]);
        if (next < session->top[active]) session->top[active] = (uint8_t)next;
        if (next >= session->top[active] + 6) session->top[active] = (uint8_t)(next - 5);
        refresh_header(session);
        return NBA97_REORDER_MOVED;
    }
    if (action == NBA97_REORDER_CANCEL) {
        if (active) {
            /* Generic FUN_8003D930: cancel turns object state 1 into 2 and
             * synthesizes confirm. FUN_800569BC block 0x80056A24 bypasses
             * validation/swap; FUN_80055314 restores first-list focus.
             * Its state-2 branch sets +0x1B=10. Cursor/scroll memory survives. */
            nba97_reorder_finish_second(session, 2);
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
        if (session->slots[session->cursor[0]] == UINT16_MAX) {
            session->modal = NBA97_REORDER_MODAL_EMPTY;
            return NBA97_REORDER_REJECTED_EMPTY;
        }
        nba97_reorder_begin_second(session);
        return NBA97_REORDER_PICKED;
    }
    result = nba97_reorder_swap(session->slots, session->cursor[0], session->cursor[1], &session->changes);
    if (result != NBA97_REORDER_CHANGED) {
        /* FUN_800569BC block 0x80056A78 clears +0x1B on failed validation. */
        session->input_latch = 0;
        if (result == NBA97_REORDER_EMPTY_SLOT)
            session->modal = NBA97_REORDER_MODAL_EMPTY;
        return result == NBA97_REORDER_EMPTY_SLOT ? NBA97_REORDER_REJECTED_EMPTY : NBA97_REORDER_REJECTED_SAME;
    }
    nba97_reorder_refresh(session);
    nba97_reorder_finish_second(session, 1);
    return NBA97_REORDER_SWAPPED;
}

Nba97ReorderEvent nba97_reorder_first_callback(Nba97ReorderSession *session, uint16_t input_mask) {
    if (!session || session->phase != NBA97_REORDER_FIRST || session->modal) return NBA97_REORDER_NO_CHANGE;
    /* FUN_80054B94 validates IDs, then sets result 2/3 for the OUTER dispatcher.
     * Re-order is state 0x0C, so the shared state-0x11 exception is inapplicable. */
    if (input_mask == 0x10 || input_mask == 0x40) return child_request(session, input_mask);
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
    if (!session || session->phase != NBA97_REORDER_REPLACEMENT || session->modal)
        return NBA97_REORDER_NO_CHANGE;
    if (input_mask == 0x10 || input_mask == 0x40) return child_request(session, input_mask);
    /* 800569F8/80056A0C/80056A14/80056A1C: exact confirm and preservation
     * checks; only non-View/Compare/confirm callback values clear +0x1B. */
    if (input_mask != 0x800) {
        session->input_latch = 0;
        return NBA97_REORDER_NO_CHANGE;
    }
    /* 80056A24: generic cancel state 2 skips validation, swap AND row rebuild.
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
    case NBA97_REORDER_REQUEST_VIEW: return "dispatch-result-2-view";
    case NBA97_REORDER_REQUEST_COMPARE: return "dispatch-result-3-compare";
    default: return "no-change";
    }
}
