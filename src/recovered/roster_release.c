#include "roster_release.h"

int nba97_release_prepare_free_agents(const uint16_t free_agents[100],
                                     Nba97ReleasePosition *position) {
    int first=0, top;
    if(!free_agents || !position) return 0; /* Native provider guard. */
    /* 5721C..57267: stop at the FIRST -1, not the occupied-slot count. */
    while(first<100 && free_agents[first]!=UINT16_MAX) ++first;
    if(first==100) return 0; /* Native bound; original card prevents entry. */
    /* 57268..57297: signed(count-4), clamp to0..94, not count-5. */
    top=first-4;
    if(top<0) top=0;
    else if(top>94) top=94;
    position->cursor=(uint8_t)first;
    position->top=(uint8_t)top;
    return 1;
}

int nba97_release_available(const uint16_t table[535], int16_t mode,
                            uint8_t restriction) {
    /* 57B6C..57B8F: nonzero restriction is relevant ONLY in mode2. */
    if(mode==2 && restriction) return 0;
    if(!table) return 0; /* Native safety, no original instruction credit. */
    /* 57B90..57BAF: signed halfword -1 at byte offset0x42C, then return.
       Keep the exact sentinel test even for malformed noncompact input. */
    return table[534]==UINT16_MAX;
}

int nba97_release_begin(Nba97TradeScreen *screen,const uint16_t table[535],
        int16_t donor,int16_t mode,const int8_t eligible[16],uint8_t donor_cursor,uint8_t donor_top) {
    Nba97ReleasePosition position;
    uint8_t cursor[2],top[2];
    int16_t normalized;
    if(!table || !nba97_release_prepare_free_agents(table+435,&position)) return 0;
    /* 57298..572B0: typed per-screen state replaces saved selector globals.
       Native slots remain relative; constructor descriptors use right base15. */
    cursor[0]=donor_cursor;cursor[1]=position.cursor;
    top[0]=donor_top;top[1]=position.top;
    normalized=nba97_reorder_normalize_team(donor,mode,eligible?eligible[0]:0);
    /* 572B4..572D8: donor/kind1,29/kind0, callback57084,NULL. */
    return nba97_roster_editor_begin(screen,table,normalized,29,mode,eligible,cursor,top,17,
        nba97_release_callback,0);
    /* 572DC..572EF: native ABI replaces register/stack return; shared frame
       pump carries selector result through nba97_trade_result, signed int16. */
}

int nba97_release_advance(Nba97TradeScreen *s) {
    if(!s || s->frontend_state!=17 || s->cursor[1]>=100 || s->top[1]>94 ||
       s->cursor[1]<s->top[1] || s->cursor[1]>=s->top[1]+6) return 0;
    /*56FF4..5702F: compare relative cursor-top; absolute109 is relative94.*/
    if(s->cursor[1]-s->top[1]>=4 && s->top[1]!=94) {
        /*57030..5705B: temporarily operate right list at its bottom row,
          3A914 scrolls one row, then39574(0,9). Native continuation blocks
          input for nine ticks; no emulated selector or busy waiting.*/
        s->release_restore_cursor=s->cursor[1];
        ++s->top[1];s->cursor[1]=(uint8_t)(s->top[1]+5);
        s->release_scroll_remaining=9;
    } else {
        /*5705C..57083: increment even99->100. This is an original cursor
          sentinel, NOT a100th array element; bind guards it explicitly.*/
        ++s->cursor[1];
    }
    return 1;
}
static Nba97TradeEvent release_notice(Nba97TradeScreen *s,Nba97RosterNotice kind,
                                     uint32_t address,int16_t subject) {
    s->notice=(Nba97RosterDecision){0,kind,address,subject};s->latch=0;
    return NBA97_TRADE_NOTICE;
}
Nba97TradeEvent nba97_release_callback(Nba97TradeScreen *s,uint16_t raw,
                                      const Nba97TradeData *data) {
    Nba97RosterList lists[2];Nba97RosterCompaction compact;
    if(!s) return NBA97_TRADE_INVALID;
    /*57084..570E3: exact masks; shared54B94 handles both child requests.*/
    if(raw!=0x800) return nba97_roster_editor_child(s,raw);
    /*570E4..57123: empty donor confirm is silent, before all providers.*/
    if(s->selected[0]==UINT16_MAX) {s->latch=0;return NBA97_TRADE_IDLE;}
    if(!nba97_roster_editor_providers(s,data)) return NBA97_TRADE_INVALID;
    /*57124..57187: nonzero mode, enabled injuries, then player injury.*/
    if(s->mode && data->injuries_enabled && data->injuries[s->selected[0]])
        return release_notice(s,NBA97_ROSTER_NOTICE_INJURED,0x800aebea,(int16_t)s->selected[0]);
    /*57188..571F3: exact equality and precedence, NOT <=8 / >=100.*/
    if(s->counts[s->team[0]]==8)
        return release_notice(s,NBA97_ROSTER_NOTICE_MINIMUM,0x800aeb54,s->team[0]);
    if(s->counts[29]==100)
        return release_notice(s,NBA97_ROSTER_NOTICE_EMPTY,0x800aec1e,-1);
    /*Separate native descriptor guard; no invented original instruction credit.*/
    if(s->cursor[1]>=100 || s->top[1]>94 || s->cursor[1]<s->top[1] ||
       s->cursor[1]>=s->top[1]+6 || s->working[435+s->cursor[1]]!=UINT16_MAX)
        return NBA97_TRADE_INVALID;
    lists[0]=(Nba97RosterList){s->working+s->team[0]*15,15,s->team[0],15,1,0,s->cursor[0],s->top[0]};
    lists[1]=(Nba97RosterList){s->working+435,100,29,100,0,15,
        (uint8_t)(15+s->cursor[1]),(uint8_t)(15+s->top[1])};
    compact=(Nba97RosterCompaction){s->counts,data->positions,data->injuries,data->preference,
        data->player_count,s->mode,data->injuries_enabled};
    /*571F4..5721B: mutation, BOTH-list refresh, then post-release advance.
      No explicit cue call: shared selector supplies one confirm cue6.*/
    if(nba97_roster_mutate(lists,s->counts,&s->changes,nba97_roster_compact,&compact)!=1)
        return NBA97_TRADE_INVALID;
    nba97_roster_editor_bind(s);
    nba97_release_advance(s);
    return NBA97_TRADE_SWAPPED;
}
