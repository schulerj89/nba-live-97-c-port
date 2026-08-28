#include "reorder_screen.h"
#include <string.h>

static int eligible(const Nba97ReorderScreen *s, int team) {
    int i;
    if (s->mode != 2) return 1;
    for (i = 0; i < 16; ++i) if (s->eligible_teams[i] == team) return 1;
    return 0;
}

const char *nba97_reorder_screen_help_tag(const Nba97ReorderScreen *s) {
    if (!s || s->selection.descriptor_page > 1) return NULL;
    return s->selection.descriptor_page ? "hel2" : "hel1";
}

void nba97_reorder_screen_markers(const Nba97ReorderScreen *s,
        Nba97ReorderMarker markers[4]) {
    int page, down;
    if (!markers) return;
    memset(markers, 0, sizeof(*markers) * 4);
    if (!s || s->visible_rows != 6) return;
    for (page = 0; page < 2; ++page) for (down = 0; down < 2; ++down) {
        const int index = page + down * 2;
        const int top = s->selection.top[page];
        Nba97ReorderMarker *m = &markers[index];
        m->x = (int16_t)((page ? 270 : 60) - 20 + s->arrow_x[index]);
        m->y = (int16_t)(106 + down * (s->visible_rows - 1) * 16 + s->arrow_y[index]);
        m->glyph = (uint8_t)(down ? 0x8c : 0x8b);
        m->visible = (uint8_t)(top <= 9 && (down ? top + s->visible_rows - 1 < 14 : top != 0));
    }
}

void nba97_reorder_screen_rebind(Nba97ReorderScreen *s) {
    int page, row;
    if (!s) return;
    for (page = 0; page < 2; ++page) for (row = 0; row < 15; ++row) {
        Nba97ReorderRow *r = &s->rows[page * 15 + row];
        r->id = (uint16_t)(page * 15 + row);
        r->player_id = s->selection.row_ids[page][row];
        r->page = (uint8_t)page;
        r->type = 0x33; r->alignment = 1;
        r->x = (int16_t)(page ? 270 : 60);
        r->y = (int16_t)(112 + 16 * (row - s->selection.top[page]));
        r->up = (uint8_t)(row != 0);
        r->down = (uint8_t)(row != 14);
        r->team_scan = 1; /* Both directions bind 80055EF0; callback gates phase. */
    }
}

int nba97_reorder_screen_enter(Nba97ReorderScreen *s,
        const uint16_t table[NBA97_ROSTER_TABLE_SLOTS], int16_t requested,
        int16_t mode, const int8_t teams[16], const int16_t cursor[2],
        const int16_t top[2], uint8_t active) {
    Nba97ReorderScreen fresh;
    int page, team;
    if (!s || !table || (mode == 2 && !teams)) return 0;
    memset(&fresh, 0, sizeof(fresh));
    fresh.mode = mode;
    if (teams) memcpy(fresh.eligible_teams, teams, 16);
    team = nba97_reorder_normalize_team(requested, mode, teams ? teams[0] : 0);
    if (team < 0 || team >= 29) return 0; /* Native guard, not original credit. */
    if (!eligible(&fresh, team)) team = teams[0]; /* 80056128 first-list fallback. */
    if (team < 0 || team >= 29) return 0;
    fresh.team = (int16_t)team; /* The second descriptor follows normalization. */
    memcpy(fresh.snapshot, table, sizeof(fresh.snapshot));
    memcpy(fresh.working, table, sizeof(fresh.working));
    nba97_reorder_begin(&fresh.selection, &table[team * 15]);
    for (page = 0; page < 2; ++page) {
        int c = cursor ? cursor[page] : -1;
        int t = top ? top[page] : -1;
        if (c == -1) c = t = page * 15;
        c -= page * 15; t -= page * 15;
        if (c < 0 || c >= 15 || t < 0 || t > 9 || c < t || c >= t+6) return 0;
        fresh.selection.cursor[page] = (uint8_t)c;
        fresh.selection.top[page] = (uint8_t)t;
    }
    fresh.layout = 13; fresh.visible_rows = 6;
    fresh.list_kind[0] = 1; fresh.list_kind[1] = 2;
    fresh.image_object[0] = 18; fresh.image_object[1] = 19;
    for (page = 0; page < 4; ++page) {
        fresh.arrow_x[page] = 6; fresh.arrow_y[page] = 10;
    }
    fresh.heading_x = 256; fresh.heading_y = 70;
    fresh.first_callback = 0x800568e4u; fresh.second_callback = 0x800569bcu;
    fresh.entry_callback = 0x800560bcu; fresh.exit_callback = 0x80056254u;
    nba97_reorder_refresh(&fresh.selection);
    nba97_reorder_focus_first(&fresh.selection);
    if (active) nba97_reorder_begin_second(&fresh.selection); /* 800560BC */
    nba97_reorder_screen_rebind(&fresh);
    *s = fresh;
    return 1;
}

Nba97ReorderEvent nba97_reorder_screen_input(Nba97ReorderScreen *s, Nba97ReorderAction a) {
    Nba97ReorderEvent event;
    if (!s || s->selection.screen_result) return NBA97_REORDER_NO_CHANGE; /* Child owns input. */
    event = nba97_reorder_input(&s->selection, a);
    memcpy(&s->working[s->team * 15], s->selection.slots, sizeof(s->selection.slots));
    if (event == NBA97_REORDER_EXIT_DISCARDED) {
        memcpy(s->working, s->snapshot, sizeof(s->snapshot));
        s->result = 1;
    } else if (event == NBA97_REORDER_EXIT_ACCEPTED) s->result = 1;
    nba97_reorder_screen_rebind(s);
    return event;
}

int nba97_reorder_screen_scan(Nba97ReorderScreen *s, int direction) {
    int team, attempts;
    uint16_t changes;
    uint8_t cursor[2], top[2];
    if (!s || !direction || s->selection.phase != NBA97_REORDER_FIRST ||
        s->selection.modal || s->selection.waiting_input_change || s->selection.screen_result) return 0;
    team = s->team;
    for (attempts = 0; attempts < 29; ++attempts) {
        team = (team + (direction < 0 ? 28 : 1)) % 29;
        if (team != s->team && eligible(s, team)) break;
    }
    if (attempts == 29) return 0;
    memcpy(&s->working[s->team * 15], s->selection.slots, sizeof(s->selection.slots));
    changes = s->selection.changes;
    memcpy(cursor, s->selection.cursor, sizeof(cursor));
    memcpy(top, s->selection.top, sizeof(top));
    s->team = (int16_t)team;
    /* 80055EF0 -> 80055AF8: Re-order kind 2 follows the first team, preserves
     * both six-row windows, rebinds all IDs, refreshes portraits/palette. */
    nba97_reorder_begin(&s->selection, &s->working[team * 15]);
    s->selection.changes = changes;
    memcpy(s->selection.cursor, cursor, sizeof(cursor));
    memcpy(s->selection.top, top, sizeof(top));
    nba97_reorder_refresh(&s->selection);
    nba97_reorder_focus_first(&s->selection);
    nba97_reorder_screen_rebind(s);
    return 1;
}

void nba97_reorder_screen_save(const Nba97ReorderScreen *s,
        int16_t cursor[2], int16_t top[2], uint8_t *active) {
    int p;
    if (!s || !cursor || !top || !active) return;
    for (p = 0; p < 2; ++p) {
        cursor[p] = (int16_t)(s->selection.cursor[p] + p * 15);
        top[p] = (int16_t)(s->selection.top[p] + p * 15);
    }
    *active = s->selection.active_page;
}

int16_t nba97_reorder_screen_result(const Nba97ReorderScreen *s) {
    if (!s) return 0;
    return s->selection.screen_result ? (int16_t)s->selection.screen_result : s->result;
}
