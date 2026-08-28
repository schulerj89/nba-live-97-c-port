#include "roster_trade.h"
#include "roster_reorder.h"
#include <string.h>

#define trade_first_callback nba97_roster_editor_first
#define trade_child_callback nba97_roster_editor_child
#define bind nba97_roster_editor_bind
#define finish_second nba97_roster_editor_finish_second
#define providers_valid nba97_roster_editor_providers
static Nba97TradeEvent trade_second_callback(Nba97TradeScreen*,uint16_t,const Nba97TradeData*);

Nba97TradeTeams nba97_trade_prepare_teams(int16_t left, int16_t right,
                                       int16_t mode, int8_t context_team) {
    Nba97TradeTeams teams;
    /* Original normalizes right first; keep this call order explicit. */
    teams.right = nba97_reorder_normalize_team(right, mode, context_team);
    teams.left = nba97_reorder_normalize_team(left, mode, context_team);
    if (teams.left == teams.right)
        teams.left = teams.right == 0 ? 3 : 0;
    return teams;
}

static int eligible(const Nba97TradeScreen *s,int team) {
    int i;
    if(s->mode!=2) return 1;
    for(i=0;i<16;++i) if(s->eligible[i]==team) return 1;
    return 0;
}
unsigned nba97_roster_editor_capacity(const Nba97TradeScreen *s,unsigned p) {
    return s && p<2 ? (s->team[p]==29?100:15) : 0;
}
void bind(Nba97TradeScreen *s) {
    int p;
    for(p=0;p<2;++p) s->selected[p]=s->working[s->team[p]*15+s->cursor[p]];
    ++s->row_revision; ++s->presents; /*55AF8 present request, not emulated vblank*/
}
int nba97_trade_dirty(const Nba97TradeScreen *s) {
    return s && memcmp(s->snapshot,s->working,sizeof(s->working))!=0;
}
int nba97_trade_undo_dirty(const Nba97TradeScreen *s) {
    return s && memcmp(s->undo,s->working,sizeof(s->working))!=0;
}
int32_t nba97_trade_result(const Nba97TradeScreen *s) {
    return s ? (int32_t)s->selector_result : 0;
}
uint8_t nba97_trade_event_sound(Nba97TradeEvent event,uint16_t raw) {
    /* 3D930 initializes +1B before invoking the row's callback. Unchanged
       row selection / refused team scans clear it. 55314 overrides a
       cancelled second selection to10 after the input-change barrier. */
    switch(event) {
    case NBA97_TRADE_ROW: return raw==1?3:raw==2?4:0;
    case NBA97_TRADE_TEAM: return raw==8?2:raw==4?1:0;
    case NBA97_TRADE_PICK:
    case NBA97_TRADE_SWAPPED: return raw==0x800?6:0;
    case NBA97_TRADE_CANCEL_PICK: return raw==0x100?10:0;
    case NBA97_TRADE_VIEW: return raw==0x10?6:0;
    case NBA97_TRADE_COMPARE: return raw==0x40?6:0;
    default: return 0;
    }
}
int nba97_trade_begin(Nba97TradeScreen *s,const uint16_t table[535],
        int16_t left,int16_t right,int16_t mode,const int8_t teams[16],
        const uint8_t cursor[2],const uint8_t top[2]) {
    Nba97TradeTeams pair;
    pair=nba97_trade_prepare_teams(left,right,mode,teams?teams[0]:0);
    return nba97_roster_editor_begin(s,table,pair.left,pair.right,mode,teams,cursor,top,13,
        trade_first_callback,trade_second_callback);
}
int nba97_roster_editor_begin(Nba97TradeScreen *s,const uint16_t table[535],
        int16_t left,int16_t right,int16_t mode,const int8_t teams[16],
        const uint8_t cursor[2],const uint8_t top[2],uint8_t state,
        Nba97TradeCallback first,Nba97TradeCallback second) {
    Nba97TradeScreen fresh;
    int p,i,hole;
    if(!s || !table || !first || !second || (mode==2 && !teams) || (state!=13 && state!=14)) return 0;
    memset(&fresh,0,sizeof(fresh));fresh.mode=mode;
    fresh.frontend_state=state;
    if(teams) memcpy(fresh.eligible,teams,16);
    fresh.team[0]=left;fresh.team[1]=right;
    /* 56D28..56D4C: two kind1 lists, first56B44 / second56C50.
       Native function pointers replace original stack-passed code addresses. */
    fresh.list_kind[0]=state==14?0:1;fresh.list_kind[1]=1;
    fresh.input_callback[0]=first;
    fresh.input_callback[1]=second;
    for(p=0;p<2;++p) {
        unsigned capacity;
        if(fresh.team[p]<0 || fresh.team[p]>(state==14 && p==0?29:28)) return 0;
        if(fresh.team[p]!=29 && !eligible(&fresh,fresh.team[p])) fresh.team[p]=teams[p]; /*56128*/
        if(fresh.team[p]<0 || fresh.team[p]>29) return 0;
        capacity=nba97_roster_editor_capacity(&fresh,p);
        fresh.cursor[p]=cursor?cursor[p]:0;fresh.top[p]=top?top[p]:0;
        if(fresh.cursor[p]>=capacity || fresh.top[p]>capacity-6 || fresh.cursor[p]<fresh.top[p] ||
           fresh.cursor[p]>=fresh.top[p]+6) return 0;
        for(i=0;i<(int)capacity;++i) {
            memset(fresh.tint[p][i].start,128,3);
            memset(fresh.tint[p][i].rgb,128,3);
        }
    }
    /* Native guards: never expose aliased team writes or malformed slot tables. */
    if(fresh.team[0]==fresh.team[1]) return 0;
    for(p=0;p<30;++p) {
        hole=0;
        for(i=0;i<(p==29?100:15);++i) {
            const uint16_t id=table[p*15+i];
            if(id==UINT16_MAX) hole=1;
            else {if(hole || id>=0x8000) return 0;++fresh.counts[p];}
        }
    }
    memcpy(fresh.snapshot,table,sizeof(fresh.snapshot));
    memcpy(fresh.undo,table,sizeof(fresh.undo));
    memcpy(fresh.working,table,sizeof(fresh.working));
    bind(&fresh);nba97_reorder_tint_pulse(&fresh.tint[0][fresh.cursor[0]]);
    *s=fresh;return 1;
}
int nba97_trade_frame(Nba97TradeScreen *s,uint16_t raw) {
    int p,i;
    if(!s) return 0;
    for(p=0;p<2;++p) for(i=0;i<(int)nba97_roster_editor_capacity(s,p);++i) nba97_reorder_tint_tick(&s->tint[p][i]);
    if(s->waiting) {if(raw==s->held) return 0;s->waiting=0;}
    s->held=raw;return 1;
}
void finish_second(Nba97TradeScreen *s,int cancelled) {
    nba97_reorder_tint_unpulse(&s->tint[1][s->cursor[1]]);
    s->phase=NBA97_TRADE_FIRST;bind(s);
    if(cancelled) {s->waiting=1;s->held=0x100;s->latch=10;}
}
int providers_valid(const Nba97TradeScreen *s,const Nba97TradeData *d) {
    int i;
    if(!d || !d->positions || !d->preference || !d->player_count ||
       (d->injuries_enabled && !d->injuries)) return 0;
    for(i=0;i<25;++i) if(d->preference[i]>4) return 0;
    for(i=0;i<535;++i) if(s->working[i]!=UINT16_MAX && s->working[i]>=d->player_count) return 0;
    return 1;
}
static Nba97TradeEvent reject(Nba97TradeScreen *s,Nba97RosterDecision d) {
    s->notice=d;s->latch=0;
    return d.notice ? NBA97_TRADE_NOTICE : NBA97_TRADE_IDLE;
}
Nba97TradeEvent nba97_trade_input(Nba97TradeScreen *s,uint16_t raw,const Nba97TradeData *data) {
    int p,next,attempt;
    if(!s || s->phase==NBA97_TRADE_CLOSED || s->child || s->waiting || s->notice.notice)
        return NBA97_TRADE_IDLE;
    p=s->phase==NBA97_TRADE_SECOND;
    if(raw==1 || raw==2) {
        next=s->cursor[p]+(raw==1?-1:1);
        if(next<0 || next>=(int)nba97_roster_editor_capacity(s,p)) return NBA97_TRADE_IDLE;
        nba97_reorder_tint_unpulse(&s->tint[p][s->cursor[p]]);
        s->cursor[p]=(uint8_t)next;
        nba97_reorder_tint_pulse(&s->tint[p][next]);
        if(next<s->top[p]) s->top[p]=(uint8_t)next;
        if(next>=s->top[p]+6) s->top[p]=(uint8_t)(next-5);
        bind(s);return NBA97_TRADE_ROW;
    }
    if(raw==8 || raw==4) {
        if(s->team[0]==29) p=1; /*55EF0: Sign always scans the receiver.*/
        next=s->team[p];
        for(attempt=0;attempt<29;++attempt) {
            next=(next+(raw==8?28:1))%29;
            if(next!=s->team[1-p] && eligible(s,next)) break;
        }
        if(attempt==29 || next==s->team[p]) {s->latch=0;return NBA97_TRADE_IDLE;}
        s->team[p]=(int16_t)next;bind(s);return NBA97_TRADE_TEAM;
    }
    if(raw==0x100) {
        if(p) {finish_second(s,1);return NBA97_TRADE_CANCEL_PICK;}
        if(nba97_trade_undo_dirty(s)) return NBA97_TRADE_DISCARD_PROMPT;
        s->phase=NBA97_TRADE_CLOSED;s->selector_result=-1;return NBA97_TRADE_DISCARD;
    }
    if(raw==0x80) {
        if(p) return NBA97_TRADE_IDLE;
        s->phase=NBA97_TRADE_CLOSED;s->selector_result=1;return NBA97_TRADE_ACCEPT;
    }
    if(!s->input_callback[p] || s->list_kind[0]!=(s->frontend_state==14?0:1) || s->list_kind[1]!=1) return NBA97_TRADE_INVALID;
    return s->input_callback[p](s,raw,data);
}
Nba97TradeEvent trade_child_callback(Nba97TradeScreen *s,uint16_t raw) {
    const int p=s->phase==NBA97_TRADE_SECOND;
    if(raw==0x10 || raw==0x40) {
        if((raw==0x10 && s->selected[p]==UINT16_MAX) ||
           (raw==0x40 && (s->selected[0]==UINT16_MAX || s->selected[1]==UINT16_MAX))) {
            return reject(s,(Nba97RosterDecision){0,NBA97_ROSTER_NOTICE_EMPTY,0x800afc22,-1});
        }
        s->selector_result=(int16_t)(raw==0x10?2:3); /*54B94 -> selector ->56D50*/
        s->child=(uint8_t)(raw==0x10?0x24:0x23);
        return raw==0x10?NBA97_TRADE_VIEW:NBA97_TRADE_COMPARE;
    }
    s->latch=0;return NBA97_TRADE_IDLE;
}
Nba97TradeEvent trade_first_callback(Nba97TradeScreen *s,uint16_t raw,const Nba97TradeData *data) {
    if(raw!=0x800) return trade_child_callback(s,raw);
    if(!providers_valid(s,data)) return NBA97_TRADE_INVALID;
    /*56B44 injury gate, then568E4: empty first slot IS allowed in Trade.*/
    if(s->mode && data->injuries_enabled && s->selected[0]!=UINT16_MAX &&
       data->injuries[s->selected[0]])
        return reject(s,(Nba97RosterDecision){0,NBA97_ROSTER_NOTICE_INJURED,0x800aebb2,(int16_t)s->selected[0]});
    s->phase=NBA97_TRADE_SECOND;
    nba97_reorder_tint_pulse(&s->tint[1][s->cursor[1]]);bind(s);
    return NBA97_TRADE_PICK;
}
static Nba97TradeEvent trade_second_callback(Nba97TradeScreen *s,uint16_t raw,const Nba97TradeData *data) {
    int i;
    Nba97RosterList lists[2];
    Nba97RosterValidation rules;
    Nba97RosterCompaction compact;
    Nba97RosterDecision decision;
    if(raw!=0x800) return trade_child_callback(s,raw);
    if(!providers_valid(s,data)) return NBA97_TRADE_INVALID;
    /*56C50 delegates to569BC and the shared validation/mutation helpers.*/
    for(i=0;i<2;++i) lists[i]=(Nba97RosterList){s->working+s->team[i]*15,15,
        s->team[i],15,s->list_kind[i],(uint8_t)(i*15),(uint8_t)(i*15+s->cursor[i]),(uint8_t)(i*15+s->top[i])};
    rules=(Nba97RosterValidation){s->mode,13,data->injuries_enabled,s->counts,data->injuries,data->player_count};
    decision=nba97_roster_validate(lists,&rules);
    if(decision.result!=1) return reject(s,decision);
    compact=(Nba97RosterCompaction){s->counts,data->positions,data->injuries,data->preference,
                                  data->player_count,s->mode,data->injuries_enabled};
    if(nba97_roster_mutate(lists,s->counts,&s->changes,nba97_roster_compact,&compact)!=1)
        return NBA97_TRADE_INVALID;
    bind(s);finish_second(s,0);return NBA97_TRADE_SWAPPED;
}
void nba97_trade_dismiss_notice(Nba97TradeScreen *s,uint16_t held) {
    if(!s) return;
    memset(&s->notice,0,sizeof(s->notice));s->waiting=1;s->held=held;s->latch=0;
}
Nba97TradeEvent nba97_trade_discard_answer(Nba97TradeScreen *s,int discard,uint16_t held) {
    int team,slot;
    if(!s || s->phase!=NBA97_TRADE_FIRST || s->child) return NBA97_TRADE_INVALID;
    if(!discard) {s->waiting=1;s->held=held;return NBA97_TRADE_IDLE;}
    /* 56254 copies the current constructor's snapshot, not the roster from
       before all child visits. Restore derived native counts as well. */
    memcpy(s->working,s->undo,sizeof(s->working));
    memset(s->counts,0,sizeof(s->counts));
    for(team=0;team<30;++team)for(slot=0;slot<(team==29?100:15);++slot)
        if(s->working[team*15+slot]!=UINT16_MAX)++s->counts[team];
    s->changes=0;bind(s);s->phase=NBA97_TRADE_CLOSED;s->selector_result=-1;
    return NBA97_TRADE_DISCARD;
}
int nba97_trade_child_proposal(const Nba97TradeScreen *s,uint16_t mask,
        const int16_t teams[2],const uint8_t slots[2]) {
    int p,first,last;
    if(!s || !teams || !slots || mask==0x100 || (s->child!=0x23 && s->child!=0x24)) return 0;
    /* 5A3FC/5A6F0 both require parent state13 before offering cursor adoption.
       Sign is state14: browsing NBA teams in either child must not replace
       its team29 source, or even change its receiver selection. Intentional
       original restriction, not a missing Sign confirmation dialog. */
    if(s->frontend_state!=13) return 0;
    first=s->child==0x24 ? s->phase==NBA97_TRADE_SECOND : 0;
    last=s->child==0x24 ? first : 1;
    if(s->child==0x23 && teams[0]==teams[1]) return 0;
    for(p=first;p<=last;++p) if(teams[p]<0 || teams[p]>=29 || slots[p]>=15 ||
        s->working[teams[p]*15+slots[p]]==UINT16_MAX) return 0;
    for(p=first;p<=last;++p) if(teams[p]!=s->team[p] || slots[p]!=s->cursor[p]) return 1;
    return 0;
}
int nba97_trade_return_child(Nba97TradeScreen *s,uint16_t mask,
        const int16_t teams[2],const uint8_t slots[2],int adopt) {
    int p,first,last;
    if(!s || (s->child!=0x23 && s->child!=0x24)) return 0;
    if(adopt && !nba97_trade_child_proposal(s,mask,teams,slots)) return 0;
    if(s->frontend_state==14) {
        Nba97TradeScreen fresh;
        /* Original Sign quirk: 40154 reads global left-team+70E (29), not
           receiver+710, before calling56F9C again. 56A94 maps29 to Chicago3
           (or eligible[0] in mode2). 56254 saves phase at+71C;560BC restores
           it through552B4 on re-entry, including second-selection children.
           Confirmed Sign -> View Alston -> Start in no$psx 2026-08-28:
           Charlotte becomes Chicago, cursor/top stay, signing is retained.
           Do NOT "fix" this to restore the old receiver. */
        const int16_t receiver=nba97_reorder_normalize_team(s->team[0],s->mode,s->eligible[0]);
        if(!nba97_roster_editor_begin(&fresh,s->working,29,receiver,s->mode,s->eligible,
                s->cursor,s->top,14,s->input_callback[0],s->input_callback[1])) return 0;
        /* Constructor rebases undo, but must not publish the native save or
           discard its original durable conflict-detection baseline. */
        memcpy(fresh.snapshot,s->snapshot,sizeof(fresh.snapshot));
        fresh.phase=s->phase;
        if(fresh.phase==NBA97_TRADE_SECOND) nba97_reorder_tint_pulse(&fresh.tint[1][fresh.cursor[1]]);
        fresh.row_revision=s->row_revision+1;fresh.presents=s->presents+1;
        fresh.waiting=1;fresh.held=mask;*s=fresh;return 1;
    }
    first=s->child==0x24 ? s->phase==NBA97_TRADE_SECOND : 0;
    last=s->child==0x24 ? first : 1;
    if(adopt) for(p=first;p<=last;++p) {
        nba97_reorder_tint_unpulse(&s->tint[p][s->cursor[p]]);
        s->team[p]=teams[p];s->cursor[p]=slots[p];s->top[p]=(uint8_t)(slots[p]<9?slots[p]:9);
        if(p==0 || s->phase==NBA97_TRADE_SECOND) nba97_reorder_tint_pulse(&s->tint[p][s->cursor[p]]);
    }
    /* Child exit re-enters56CD0: normalize the two requested teams again.
       In particular View can browse onto the opposite list's team. */
    if(adopt && s->frontend_state!=14) {
        Nba97TradeTeams pair=nba97_trade_prepare_teams(s->team[0],s->team[1],s->mode,s->eligible[0]);
        s->team[0]=pair.left;s->team[1]=pair.right;
    }
    /* 3F7C8 child pop ->56CD0 ->56494: local480/current slots and local50=0.
       This includes Select/ignore returns, not only adopted cursor changes.
       Intentional original-game quirk: Cancel cannot undo pre-child trades.
       Verified in no$psx; retain this behavior, do not "fix" it to entry undo.
       Preserve the durable baseline: this operation never writes a save. */
    memcpy(s->undo,s->working,sizeof(s->undo));s->changes=0;
    s->child=0;s->selector_result=0;s->waiting=1;s->held=mask;bind(s);return 1;
}
