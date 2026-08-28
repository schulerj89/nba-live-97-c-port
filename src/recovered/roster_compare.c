#include "roster_compare.h"
#include <string.h>

static unsigned capacity(unsigned team) { return team == 29 ? 100 : 15; }
static unsigned count(const uint16_t *table, unsigned team) {
    unsigned n=0;
    if (team>29) return 0;
    /* Original 59928 wraps against the derived roster count. Validated normal
     * lists are a contiguous live prefix, followed by signed -1 sentinels. */
    while (n<capacity(team) && table[team*15+n]!=UINT16_MAX) ++n;
    return n;
}
unsigned nba97_compare_stat_count(const Nba97Compare *s) {
    if (!s || !s->initialized || s->layer>3) return 0;
    return s->layer==0 ? 14 : s->layer==1 ? 17 : 24;
}
static int valid(const Nba97Compare *s, const uint16_t *table) {
    unsigned p;
    if (!s || !table || !s->initialized || s->active_side>1 ||
        s->layer>3 || s->top+5u>nba97_compare_stat_count(s)) return 0;
    for(p=0;p<2;++p)
        if(s->team[p]>29 || s->slot[p]>=count(table,s->team[p]) ||
           s->player[p]!=table[s->team[p]*15+s->slot[p]]) return 0;
    return 1;
}
int nba97_compare_begin_teams(Nba97Compare *s,const int16_t teams[2],const uint8_t slots[2],const uint16_t *table) {
    Nba97Compare next;
    unsigned p;
    if(!s || !teams || !slots || !table) return 0;
    memset(&next,0,sizeof(next));next.initialized=1;next.layer=2;
    for(p=0;p<2;++p) {
        if(teams[p]<0 || teams[p]>29 || slots[p]>=capacity(teams[p])) return 0;
        next.team[p]=(uint8_t)teams[p];next.slot[p]=slots[p];
        next.player[p]=table[teams[p]*15+slots[p]];
    }
    if(!valid(&next,table)) return 0;*s=next;return 1;
}
int nba97_compare_begin(Nba97Compare *s, const Nba97ReorderChild *child, const uint16_t *table) {
    Nba97Compare next;
    unsigned p;
    if(!s || !child || !table || child->state!=0x23 || child->team<0 || child->team>=29) return 0;
    memset(&next,0,sizeof(next));
    /* 5A074: two independent identities, active page always zero, even when
     * the parent opened Compare from the replacement-selection page. 5A1EC
     * selects normal layer2 with inclusive limit3; 5A880 shows five rows. */
    next.initialized=1; next.layer=2;
    for(p=0;p<2;++p) {
        next.team[p]=(uint8_t)child->team;
        next.slot[p]=child->cursor[p];
        next.player[p]=child->player_id[p];
    }
    if(!valid(&next,table)) return 0;
    *s=next;
    return 1;
}
Nba97CompareEvent nba97_compare_input(Nba97Compare *s, const uint16_t *table, uint16_t mask) {
    unsigned p,n,team,slot,attempt,old_count;
    if(!valid(s,table)) return NBA97_COMPARE_INVALID;
    p=s->active_side;
    switch(mask) {
    case 0x800: /* 59F20 -> 59A88: Cross toggles the active identity only. */
        s->active_side^=1; return NBA97_COMPARE_SIDE;
    case 1: case 2:
        /* 3AB64 applies the same scroll to BOTH descriptor groups and
         * restores the active side. The secondary arrow is offscreen.
         * 5A1EC clears descriptor0/33 +20 (Up callbacks). 3D930 therefore
         * never enters3AB64 at the top, despite3A6BC's raw-zero-only test.
         * Entry, Up completion and changed-layer extents put both cursors
         * at their group tops; no independent row selection exists here. */
        if(mask==1) { if(!s->top) return NBA97_COMPARE_NO_CHANGE; --s->top; }
        else { if(s->top+5u>=nba97_compare_stat_count(s)) return NBA97_COMPARE_NO_CHANGE; ++s->top; }
        return NBA97_COMPARE_SCROLL;
    case 8: case 4:
        n=count(table,s->team[p]); slot=s->slot[p];
        /* 59928: context+708 is count[29] (6CE+29*2), not a generic
         * season-mode flag. One free agent clears the selector sound latch. */
        if(s->team[p]==29 && n==1) return NBA97_COMPARE_NO_CHANGE;
        slot=mask==8 ? (slot ? slot-1 : n-1) : (slot+1==n ? 0 : slot+1);
        s->slot[p]=(uint8_t)slot;
        s->player[p]=table[s->team[p]*15+slot];
        return NBA97_COMPARE_PLAYER;
    case 0x200: case 0x400:
        /* 59ABC includes team29 iff its first free-agent slot is populated.
         * Preserve the slot, clamp >14 to zero, then walk BACK to an occupied
         * slot on the new team. Bounded guards replace unsafe underflow on
         * malformed data; the draft itself is never written. */
        team=s->team[p];
        for(attempt=0;attempt<30;++attempt) {
            team=mask==0x200 ? (team ? team-1 : 29) : (team==29 ? 0 : team+1);
            if(count(table,team)) break;
        }
        if(attempt==30) return NBA97_COMPARE_INVALID;
        slot=s->slot[p]>14 ? 0 : s->slot[p];
        while(slot && table[team*15+slot]==UINT16_MAX) --slot;
        if(table[team*15+slot]==UINT16_MAX) return NBA97_COMPARE_INVALID;
        s->team[p]=(uint8_t)team; s->slot[p]=(uint8_t)slot;
        s->player[p]=table[team*15+slot];
        return NBA97_COMPARE_TEAM;
    case 0x1000: case 0x2000:
        old_count=nba97_compare_stat_count(s);
        s->layer=(uint8_t)(mask==0x1000 ? (s->layer ? s->layer-1 : 3) : (s->layer==3 ? 0 : s->layer+1));
        /* 59610: object27 redraw is TEXT, not a sound. Reset both top indices
         * only when 594F0 reports a changed descriptor extent. */
        if(old_count!=nba97_compare_stat_count(s)) s->top=0;
        return NBA97_COMPARE_LAYER;
    default: return NBA97_COMPARE_NO_CHANGE; /* Help/exit belong to the host. */
    }
}

int nba97_compare_refresh_begin(Nba97CompareRefresh *r,const Nba97Compare *s) {
    if(!r || !s || !s->initialized || s->active_side>1 || !nba97_compare_stat_count(s)) return 0;
    memset(r,0,sizeof(*r)); r->text=*s; return 1;
}
static int refresh_valid(const Nba97CompareRefresh *r,const Nba97Compare *s) {
    unsigned side;
    if(!r || !s || !s->initialized || !r->text.initialized || s->active_side>1 ||
        s->layer>3 || s->top+5u>nba97_compare_stat_count(s) || r->remaining>2 ||
        (r->remaining ? r->cue<1 || r->cue>4 : r->cue!=0)) return 0;
    if(r->text.active_side!=s->active_side || r->text.layer!=s->layer) return 0;
    if(r->text.top != (int)s->top + (r->remaining==2 && r->cue>=3 ? (r->cue==3 ? 1 : -1) : 0) ||
        r->text.top+5u>nba97_compare_stat_count(s)) return 0;
    if(r->remaining && r->cue>=3 &&
        ((r->cue==3 && s->top+5u>=nba97_compare_stat_count(s)) || (r->cue==4 && !s->top))) return 0;
    for(side=0;side<2;++side) {
        if(r->text.team[side]!=s->team[side]) return 0;
        if((!r->remaining || r->cue>=3 || side!=s->active_side) &&
            (r->text.slot[side]!=s->slot[side] || r->text.player[side]!=s->player[side])) return 0;
    }
    return 1;
}
Nba97CompareEvent nba97_compare_refresh_input(Nba97Compare *s,Nba97CompareRefresh *r,
        const uint16_t *table,uint16_t mask) {
    Nba97CompareEvent event;
    if(!refresh_valid(r,s) || !valid(s,table)) return NBA97_COMPARE_INVALID;
    if(r->remaining) return NBA97_COMPARE_NO_CHANGE;
    event=nba97_compare_input(s,table,mask);
    if(event==NBA97_COMPARE_PLAYER) {
        r->remaining=2; r->cue=(uint8_t)(mask==8 ? 2 : 1);
    } else if(event==NBA97_COMPARE_SCROLL) {
        r->remaining=2; r->cue=(uint8_t)(mask==1 ? 3 : 4);
    } else if(event!=NBA97_COMPARE_INVALID) r->text=*s;
    return event;
}
int nba97_compare_refresh_presented(Nba97CompareRefresh *r,const Nba97Compare *s) {
    int cue;
    if(!refresh_valid(r,s)) return -1;
    if(!r->remaining) return 0;
    if(--r->remaining) {
        if(r->cue>=3) r->text.top=s->top;
        return 0;
    }
    r->text=*s; cue=r->cue; r->cue=0;
    return cue;
}
unsigned nba97_compare_refresh_top(const Nba97CompareRefresh *r,unsigned side) {
    if(!r || side>1) return 0;
    if(side==1 && r->remaining==1 && r->cue>=3)
        return (unsigned)((int)r->text.top+(r->cue==3 ? 1 : -1));
    return r->text.top;
}

void nba97_compare_repeat_idle(Nba97CompareRepeat *r) {
    if(r) memset(r,0,sizeof(*r));
}
unsigned nba97_compare_animation_pending(uint8_t flags, uint8_t progress8,
    uint8_t limit8, uint8_t progress16, uint8_t limit16) {
    unsigned result=(flags&8) && progress8<limit8 ? 8u : 0u;
    /* Explicit sign extension avoids implementation-defined unsigned->signed casts. */
    const int current=progress16<128 ? progress16 : (int)progress16-256;
    const int target=limit16<128 ? limit16 : (int)limit16-256;
    if((flags&16) && current<target) result=16;
    return result;
}
uint8_t nba97_compare_rebuilt_text_flags(uint8_t previous_flags) {
    return (uint8_t)(previous_flags & 0xC7);
}
static void record_normal_poll(Nba97CompareRepeat *r,uint16_t mask) {
    unsigned pass;
    /* context+720==0 takes BOTH record blocks:3AFD0 then3B0B0. The first
     * publishes the new mask before the common block compares it again.
     * Controller0 is fixed by this adapter; idle represents controllerFF. */
    for(pass=0;pass<2;++pass) {
        if(r->previous_mask==mask) { if(r->counter<48) r->counter+=2; }
        else r->counter=0;
        r->previous_mask=mask;
    }
}
int nba97_compare_repeat_request(Nba97CompareRepeat *r,uint16_t mask) {
    unsigned delay;
    if(!r || r->post_frames || r->counter>48 || (r->counter&1) || (mask!=4 && mask!=8)) return 0;
    record_normal_poll(r,mask);
    delay=r->counter<16 ? 7 : r->counter<28 ? 5 : r->counter<38 ? 3 : 1;
    r->post_frames=(uint8_t)(delay+1); /*3AE4C always pumps once before polling.*/
    return (int)delay;
}
int nba97_compare_callback_mask(uint16_t mask) {
    return (mask & 0x3e50)!=0;
}
int nba97_compare_scroll_request(Nba97CompareRepeat *r,const Nba97Compare *before,uint16_t mask) {
    if(!r || !before || !before->initialized || before->active_side>1 ||
        before->layer>3 || before->top+5u>nba97_compare_stat_count(before) ||
        r->post_frames || r->counter>48 || (r->counter&1) || (mask!=1 && mask!=2)) return 0;
    record_normal_poll(r,mask);
    /*5A270/5A27C clear Up callbacks for the first descriptors in BOTH groups.
     * 3D930 skips the entire callback/sound/delay branch when +20 is null.
     * 3AE4C still records this input and pumps once before the next poll.
     * Bottom Down has a callback: it clears the cue, but retains delay3. */
    r->post_frames=(uint8_t)(mask==1 && before->top==0 ? 1 : 4);
    return r->post_frames;
}
int nba97_compare_callback_request(Nba97CompareRepeat *r,uint16_t mask) {
    if(!r || r->post_frames || r->counter>48 || (r->counter&1) || !nba97_compare_callback_mask(mask)) return 0;
    record_normal_poll(r,mask);
    r->post_frames=6; /*3E388:39574(...,5), then3AE4C's one-frame poll.*/
    return 5;
}
int nba97_compare_repeat_presented(Nba97CompareRepeat *r) {
    if(!r || r->counter>48 || (r->counter&1) || r->post_frames>8) return -1;
    if(!r->post_frames) return 0;
    return --r->post_frames==0;
}
