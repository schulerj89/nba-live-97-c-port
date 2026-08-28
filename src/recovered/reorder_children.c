#include "reorder_children.h"
#include <string.h>

Nba97ReorderEvent nba97_reorder_child_begin(Nba97ReorderScreen *s,
        Nba97ReorderChild *child, uint16_t mask) {
    Nba97ReorderEvent event;
    Nba97ReorderChild next;
    Nba97ReorderSession *p;
    if (!s || !child || child->state || (mask != 0x10 && mask != 0x40))
        return NBA97_REORDER_NO_CHANGE;
    p=&s->selection;
    if (p->modal || p->waiting_input_change || p->screen_result ||
        s->team < 0 || s->team >= 29 || p->cursor[0] >= 15 || p->cursor[1] >= 15 ||
        p->top[0] > 9 || p->top[1] > 9 || p->descriptor_page > 1 ||
        (p->phase != NBA97_REORDER_FIRST && p->phase != NBA97_REORDER_REPLACEMENT))
        return NBA97_REORDER_NO_CHANGE;
    event = p->phase == NBA97_REORDER_FIRST ? nba97_reorder_first_callback(p,mask) :
        nba97_reorder_second_callback(p,mask,1);
    if (event != NBA97_REORDER_REQUEST_VIEW && event != NBA97_REORDER_REQUEST_COMPARE)
        return event;
    memset(&next,0,sizeof(next));
    /* 8003F7C8: Re-order result2 pushes24; result3 pushes23. 8005A074
     * selects the active page for View, but both pages for Compare. */
    next.state = (uint8_t)(p->screen_result == 2 ? 0x24 : 0x23);
    next.parent_page = p->descriptor_page;
    next.team=s->team;
    memcpy(next.cursor,p->cursor,sizeof(next.cursor));
    memcpy(next.top,p->top,sizeof(next.top));
    memcpy(next.player_id,p->child_ids,sizeof(next.player_id));
    next.waiting_input_change=1;
    next.held_mask=mask;
    *child=next;
    return event;
}

int nba97_reorder_child_input_ready(Nba97ReorderChild *child, uint16_t raw) {
    if (!child || !child->state) return 0;
    if (child->waiting_input_change) {
        if (raw == child->held_mask) return 0;
        child->waiting_input_change=0;
    }
    return 1;
}

int nba97_reorder_child_return(Nba97ReorderScreen *s, Nba97ReorderChild *child, uint16_t mask) {
    if (!s || !child || (child->state != 0x24 && child->state != 0x23) ||
        (mask != 0x80 && mask != 0x100)) return 0;
    /* 8005A3FC/8005A6F0: only parent13 (Trade) may adopt browsed child
     * selections after confirmation. Parent12 must not adopt them. We retain
     * the editor/draft rather than reconstructing from accepted global data. */
    nba97_reorder_clear_screen_result(&s->selection);
    s->selection.input_latch=0;
    s->selection.waiting_input_change=1;
    s->selection.held_mask=mask;
    memset(child,0,sizeof(*child));
    return 1;
}
