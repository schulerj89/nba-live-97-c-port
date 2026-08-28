#include "cool_fact_selection.h"

int nba97_fact_refresh(Nba97CoolFacts *s,uint8_t mask) {
    int i,count=0;
    if(!s || (mask&0xe0)) return NBA97_FACT_INVALID;
    s->available_mask=mask;
    for(i=0;i<5;++i) {
        s->flags[i]=(int8_t)((mask&(1u<<i)) ? 1 : -1);
        if(s->flags[i]==1) ++count;
    }
    s->upper=(int8_t)(count-1);
    if(!count) {s->selected=-1;s->draw_mode=0;return NBA97_FACT_NONE;}
    if(count==1) s->selected=-1; /* Allow the only variant to repeat. */
    s->draw_mode=1;
    return NBA97_FACT_DRAW;
}
int nba97_fact_offer_random(Nba97CoolFacts *s,uint32_t random_value) {
    const int candidate=(int)(random_value&7u);
    if(!s || (s->draw_mode!=1 && s->draw_mode!=2)) return NBA97_FACT_INVALID;
    if(s->draw_mode==1) {
        /* Exact 5949C rule: count-based range, NOT a mask lookup. This can
         * select an absent slot in a sparse original group; don't hide it. */
        if(candidate>s->upper || candidate==s->selected) return NBA97_FACT_DRAW;
    } else if(candidate>=5 || s->flags[candidate]!=1) return NBA97_FACT_DRAW;
    s->selected=(int8_t)candidate;s->draw_mode=0;
    return NBA97_FACT_READY;
}
int nba97_fact_prepare(Nba97CoolFacts *s) {
    int i;
    if(!s) return NBA97_FACT_INVALID;
    if(s->draw_mode) return NBA97_FACT_DRAW;
    if(s->selected==-1) return NBA97_FACT_NONE;
    if(s->selected<0 || s->selected>=5) return NBA97_FACT_INVALID; /* Native bounds guard. */
    if(s->flags[s->selected]!=0) return NBA97_FACT_READY;
    for(i=0;i<5;++i) if(s->flags[i]==1) {s->draw_mode=2;return NBA97_FACT_DRAW;}
    return nba97_fact_refresh(s,s->available_mask);
}
int nba97_fact_consume(Nba97CoolFacts *s) {
    if(!s || s->draw_mode || s->selected<0 || s->selected>=5) return NBA97_FACT_INVALID;
    s->flags[s->selected]=0;
    return NBA97_FACT_READY;
}
int nba97_fact_flash_begin(Nba97FactFlash *f) {
    if(!f || f->remaining) return NBA97_FACT_INVALID;
    f->remaining=8;
    return NBA97_FACT_READY;
}
int nba97_fact_flash_visible(const Nba97FactFlash *f) {
    return f && f->remaining && !(f->remaining&1);
}
int nba97_fact_flash_presented(Nba97FactFlash *f,Nba97CoolFacts *s) {
    if(!f || !f->remaining || f->remaining>8 || !s || s->draw_mode ||
       s->selected<0 || s->selected>=5) return NBA97_FACT_INVALID;
    --f->remaining;
    return f->remaining ? NBA97_FACT_NONE : nba97_fact_consume(s);
}
