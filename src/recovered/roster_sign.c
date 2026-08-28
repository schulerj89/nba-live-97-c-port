#include "roster_sign.h"

int nba97_sign_begin(Nba97TradeScreen *s,const uint16_t table[535],int16_t destination,
        int16_t mode,const int8_t eligible[16],const uint8_t cursor[2],const uint8_t top[2]) {
    /*56F9C..56FB4: signed team argument and shared normalization, once.*/
    const int16_t team=nba97_reorder_normalize_team(destination,mode,eligible?eligible[0]:0);
    /*56FB4..56FF0: team29/kind0, normalized receiver/kind1, exact callbacks;
      signed selector result is retained by the shared native frame pump.*/
    return nba97_roster_editor_begin(s,table,29,team,mode,eligible,cursor,top,14,
        nba97_sign_first,nba97_sign_second);
}
static Nba97TradeEvent notice(Nba97TradeScreen *s,uint32_t address,int16_t subject) {
    s->notice=(Nba97RosterDecision){0,NBA97_ROSTER_NOTICE_EMPTY,address,subject};
    s->latch=0;return NBA97_TRADE_NOTICE;
}
Nba97TradeEvent nba97_sign_first(Nba97TradeScreen *s,uint16_t raw,const Nba97TradeData *data) {
    /*56D6C..56DC8: exact input masks; View/Compare use54B94; unknown clears latch.*/
    if(raw!=0x800) return nba97_roster_editor_child(s,raw);
    /*56DD8..56E20: empty source has its own receiver-name notice.*/
    if(s->selected[0]==UINT16_MAX) return notice(s,0x800aed20,s->team[1]);
    /*56E24..56E3C: delegate56B44, including injury guard and begin-second.*/
    return nba97_roster_editor_first(s,raw,data);
}
Nba97TradeEvent nba97_sign_second(Nba97TradeScreen *s,uint16_t raw,const Nba97TradeData *data) {
    Nba97RosterList lists[2];Nba97RosterValidation rules;Nba97RosterCompaction compact;
    Nba97RosterDecision d;
    /*56E40..56EB0: shared child requests / unknown latch; no mutation.*/
    if(raw!=0x800) return nba97_roster_editor_child(s,raw);
    /*56EB4..56EDC: name resolution deferred to typed notice provider. +11==2
      skips signing but still returns via55314. Not folded into confirm.*/
    if(s->selector_action==2) {nba97_roster_editor_finish_second(s,0);return NBA97_TRADE_CANCEL_PICK;}
    if(!nba97_roster_editor_providers(s,data)) return NBA97_TRADE_INVALID; /*host guard*/
    lists[0]=(Nba97RosterList){s->working+435,100,29,100,0,0,s->cursor[0],s->top[0]};
    lists[1]=(Nba97RosterList){s->working+s->team[1]*15,15,s->team[1],15,1,
        100,(uint8_t)(100+s->cursor[1]),(uint8_t)(100+s->top[1])};
    rules=(Nba97RosterValidation){s->mode,14,data->injuries_enabled,s->counts,data->injuries,data->player_count};
    /*56EE0..56F18: only -1 triggers the full-team notice (not a bool test).*/
    d=nba97_roster_validate(lists,&rules);
    if(d.result==-1) return notice(s,0x800aec72,s->team[1]);
    /*56F1C..56F50: original absolute cursor minus100; only empty receiver.*/
    if(s->selected[1]!=UINT16_MAX) return notice(s,0x800aed88,-1);
    compact=(Nba97RosterCompaction){s->counts,data->positions,data->injuries,data->preference,
        data->player_count,s->mode,data->injuries_enabled};
    /*56F54..56F98: mutate, cue6, suppress duplicate selector sound, refresh2,
      finish second. Original helper may no-op if caller supplies two empties.*/
    if(nba97_roster_mutate(lists,s->counts,&s->changes,nba97_roster_compact,&compact)<0)
        return NBA97_TRADE_INVALID;
    s->latch=0;nba97_roster_editor_bind(s);nba97_roster_editor_finish_second(s,0);
    return NBA97_TRADE_SWAPPED; /* shared host maps event to one cue6 */
}
int nba97_sign_available(const uint16_t table[535],int16_t mode,uint8_t restriction,
        const int8_t eligible[16]) {
    int i,j,vacancies=0;
    if(!table || (mode==2 && !eligible)) return 0; /*native bounds/provider guard*/
    /*57B00..57B40: restriction BEFORE first free-agent sentinel.*/
    if(mode==2 && restriction) return 0;
    if(table[435]==UINT16_MAX) return 0;
    /*57B44..57B68:4BA78(0) contract; don't count free-agent vacancies.*/
    if(mode==2) {
        for(i=0;i<16;++i) {
            if(eligible[i]<0 || eligible[i]>=29) return 0;
            for(j=0;j<15;++j) vacancies+=table[eligible[i]*15+j]==UINT16_MAX;
        }
    } else for(i=0;i<435;++i) vacancies+=table[i]==UINT16_MAX;
    return vacancies;
}
