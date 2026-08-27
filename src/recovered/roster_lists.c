#include "roster_lists.h"

static int valid_list(const Nba97RosterList *list) {
    return list && list->slots && list->capacity && list->capacity <= 100 &&
        list->team >= 0 && list->team <= 29 && list->cursor >= list->base &&
        list->cursor - list->base < list->capacity;
}
static int32_t signed_id(uint16_t id) {
    return id < 0x8000 ? (int32_t)id : (int32_t)id - 0x10000;
}

Nba97RosterDecision nba97_roster_validate(const Nba97RosterList lists[2],
                                        const Nba97RosterValidation *rules) {
    Nba97RosterDecision d = {0, NBA97_ROSTER_NOTICE_NONE, 0, -1};
    int32_t first, second;
    int16_t minimum_team = 0;
    /* Native safety only: no original instruction credit. */
    if (!lists || !rules || !rules->counts || !valid_list(&lists[0]) ||
        !valid_list(&lists[1])) return d;
    first = signed_id(lists[0].slots[lists[0].cursor - lists[0].base]);
    second = signed_id(lists[1].slots[lists[1].cursor - lists[1].base]);
    /* 556B0..55750: original mode/injury/state gate, before all other rules.
     * Guard sentinel before array access rather than reading injury[-1]. */
    if (rules->mode && rules->injuries_enabled && rules->frontend_state == 13 && second != -1) {
        if (second < 0 || (size_t)second >= rules->player_count || !rules->injuries) return d;
        if (rules->injuries[second]) {
            d.notice = NBA97_ROSTER_NOTICE_INJURED;
            d.message_address = 0x800aebb2;
            d.subject = (int16_t)second;
            return d;
        }
    }
    /* 55758/558A4: free-agent first list, exact full-team sentinel -1. */
    if (lists[0].team == 29) {
        if (first != -1) d.result = rules->counts[lists[1].team] == 15 ? -1 : 1;
        return d;
    }
    d.result = 1;
    /* 55774..55818: BOTH empty is silent, before kind2's empty modal. */
    if (first == -1) {
        if (second == -1) { d.result = 0; return d; }
        if (rules->counts[lists[1].team] == 8) {
            d.result = 0;
            minimum_team = lists[1].team;
        }
    }
    if (second == -1 && rules->counts[lists[0].team] == 8) {
        d.result = 0;
        minimum_team = lists[0].team;
    }
    /* 55818..55870: Re-order requires two occupied DIFFERENT identities. */
    if (lists[1].kind == 2) {
        if (first != -1 && second != -1) {
            if (first == second) d.result = 0;
            return d;
        }
        d.result = 0;
        d.notice = NBA97_ROSTER_NOTICE_EMPTY;
        d.message_address = 0x800afffa;
        return d;
    }
    /* 55870..558CC: minimum roster dialog uses the affected team's name. */
    if (!d.result) {
        d.notice = NBA97_ROSTER_NOTICE_MINIMUM;
        d.message_address = 0x800aecbe;
        d.subject = minimum_team;
    }
    return d;
}

static int insertion_slot(const Nba97RosterList *list) {
    int selected = list->cursor - list->base, scan = selected;
    while (scan != -1 && list->slots[scan] == UINT16_MAX) --scan;
    return scan == selected ? selected : scan + 1;
}
int nba97_roster_mutate(Nba97RosterList lists[2], uint16_t counts[30],
                       uint16_t *changes, Nba97RosterCompact compact, void *user) {
    int a, b, donor;
    uint16_t first, second;
    if (!lists || !changes || !valid_list(&lists[0]) || !valid_list(&lists[1])) return -1;
    /* 558E0..559DC: independent backwards search, stop before index -1. */
    a = insertion_slot(&lists[0]);
    b = insertion_slot(&lists[1]);
    first = lists[0].slots[a]; second = lists[1].slots[b];
    /* 559DC..55A18: no work for two empty slots. Validation is separate;
     * even identical occupied identities increment if this helper is called. */
    if (first == UINT16_MAX && second == UINT16_MAX) return 0;
    donor = first == UINT16_MAX ? 1 : second == UINT16_MAX ? 0 : -1;
    if (donor >= 0 && (!counts || !compact ||
        lists[donor].capacity != (lists[donor].team == 29 ? 100 : 15))) return -1;
    lists[0].slots[a] = second;
    lists[1].slots[b] = first;
    /* 55A24..55ACC: counts BEFORE compaction, two symmetric transfer paths. */
    if (donor >= 0) {
        --counts[lists[donor].team];
        ++counts[lists[1 - donor].team];
        compact(user, lists[donor].slots, lists[donor].team, (int16_t)(donor ? b : a));
    }
    /* 55AD4..55AE4: halfword wrap, then ordinary native return/ABI. */
    *changes = (uint16_t)(*changes + 1u);
    return 1;
}

static int repair_starter(const Nba97RosterCompaction *c, uint16_t *slots,
                          int team, int removed) {
    uint8_t position[10];
    int bench = 0, rank, row;
    /* 5539C reads the already-decremented count. Deliberately do not extend
     * the scan by one even though the outgoing hole is not compacted yet. */
    for (row = 5; row < c->counts[team] && row < 15; ++row) {
        uint16_t id = slots[row];
        uint8_t p = id < c->player_count ? c->positions[id] : 99;
        if (c->mode && c->injuries_enabled && id < c->player_count && c->injuries[id]) p = 99;
        position[bench++] = p;
    }
    for (rank = 0; rank < 5; ++rank) for (row = 0; row < bench; ++row)
        if (position[row] == c->preference[removed * 5 + rank]) {
            slots[removed] = slots[row + 5];
            slots[row + 5] = UINT16_MAX;
            return row + 5;
        }
    return 0; /* Preserve original no-match return, not an invented fallback. */
}
void nba97_roster_compact(void *context, uint16_t *slots, int16_t team, int16_t removed) {
    const Nba97RosterCompaction *c = (const Nba97RosterCompaction *)context;
    int last = team < 29 ? 14 : 99, row = removed;
    /* Caller supplies valid 15/100 storage and, for starter removal, complete
     * private data providers. A missing provider is a caller error, not a
     * silent fake starter selection. No original data is baked in this code. */
    if (team < 29 && removed < 5) row = repair_starter(c, slots, team, removed);
    for (; row < last; ++row) slots[row] = slots[row + 1];
    slots[row] = UINT16_MAX;
}

int nba97_roster_refresh_lists(Nba97RosterRefresh *s, int16_t selector) {
    int first, last, page, row;
    uint16_t saved;
    if (!s || !s->sink || selector < 0 || selector > 2) return 0;
    first = selector == 2 ? 0 : selector;
    last = selector == 2 ? 1 : selector;
    /* Validate all requested pages before emitting anything. Zero/negative
     * display counts still refresh selected ID, as the original does. */
    for (page = first; page <= last; ++page)
        if (!valid_list(&s->lists[page]) || s->lists[page].count > s->lists[page].capacity ||
            (int)s->lists[page].base + s->lists[page].capacity > 256) return 0;
    saved = s->descriptor_page;
    /* 55AF8..55B88: selector expansion and descriptor selection. */
    for (page = first; page <= last; ++page) {
        const Nba97RosterList *list = &s->lists[page];
        s->descriptor_page = (uint16_t)page;
        /* 55BB0..55C18: bind EVERY object, redraw only [top,top+visible). */
        for (row = 0; row < list->count; ++row) {
            int object = list->base + row;
            s->sink(s->user, NBA97_ROSTER_BIND, page, object, list->slots[row]);
            if (object >= list->top && object < list->top + s->visible_rows)
                s->sink(s->user, NBA97_ROSTER_REDRAW, page, object, list->slots[row]);
        }
        /* 55C34: signed selected ID, NOT unsigned 65535. */
        s->selected[page] = signed_id(list->slots[list->cursor - list->base]);
    }
    /* 55C78: 39574(0,1) presents one frame, not a reset/clear operation. */
    s->sink(s->user, NBA97_ROSTER_PRESENT, s->descriptor_page, 0, 1);
    s->descriptor_page = saved;
    s->sink(s->user, NBA97_ROSTER_HEADER, saved, 0, 0);
    return 1;
}
