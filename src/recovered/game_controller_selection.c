#include "game_controller_selection.h"
#include <limits.h>
#include <string.h>

static int32_t signed_word(uint32_t v) {
    return v<=INT32_MAX ? (int32_t)v:-1-(int32_t)(UINT32_MAX-v);
}
static uint32_t shift_right(uint32_t v,unsigned n) {
    return (v>>n) | ((v&0x80000000u) ? (UINT32_MAX<<(32-n)):0);
}

int32_t nba97_game_selection_distance(int32_t x,int32_t y) {
    uint32_t a=(uint32_t)x,b=(uint32_t)y,smaller,larger,value;
    /* Source negation is SUBU: INT_MIN remains negative. Do not use abs(). */
    if(signed_word(a)<0) a=0u-a;
    if(signed_word(b)<0) b=0u-b;
    if(signed_word(b)<signed_word(a)) {smaller=b;larger=a;}
    else {smaller=a;larger=b;}
    if(signed_word(smaller<<1)<signed_word(larger)) value=shift_right(smaller,2);
    else value=shift_right(smaller+shift_right(smaller,1),2);
    return signed_word(value+larger);
}

static int entity_ref(const Nba97GameSelectionInput* input,uint32_t index,
                      unsigned* record) {
    if(index>=11) return NBA97_SELECTION_OUTSIDE_STORAGE;
    if(input->entity_table[index]==NBA97_SELECTION_UNKNOWN_REF) return NBA97_SELECTION_UNRESOLVED;
    *record=input->entity_table[index];
    return NBA97_SELECTION_OK;
}

int nba97_game_controller_selection(Nba97GameSelectionEffects* out,
                                    const Nba97GameSelectionInput* input) {
    Nba97GameSelectionInput source;
    Nba97GameSelectionEffects next;
    unsigned i;
    if(!out || !input) return NBA97_SELECTION_INVALID;
    memcpy(&source,input,sizeof(source));
    if(source.incoming_s6.known>1 || (!source.incoming_s6.known && source.incoming_s6.word) ||
       (source.ball>=11 && source.ball!=NBA97_SELECTION_UNKNOWN_REF)) return NBA97_SELECTION_INVALID;
    for(i=0;i<8;++i)
        if(source.controller[i].selected.known>1 ||
           (!source.controller[i].selected.known && source.controller[i].selected.word) ||
           (source.controller_table[i]>=8 && source.controller_table[i]!=NBA97_SELECTION_UNKNOWN_REF))
            return NBA97_SELECTION_INVALID;
    for(i=0;i<11;++i)
        if(source.entity_table[i]>=11 && source.entity_table[i]!=NBA97_SELECTION_UNKNOWN_REF)
            return NBA97_SELECTION_INVALID;

    memset(&next,0,sizeof(next));
    for(i=0;i<8;++i) {
        next.selected[i].word=source.controller[i].selected.word;
        next.selected[i].known=source.controller[i].selected.known;
    }
    for(i=0;i<11;++i) next.claim[i]=source.entity[i].claim;
    next.tail_state=source.tail_state;
    next.scratch_s6.word=source.incoming_s6.word;
    next.scratch_s6.known=source.incoming_s6.known;

    for(i=0;i<8;++i) {
        unsigned controller=source.controller_table[i],start,local,target,side;
        int32_t best=800;
        int result,already_claimed=0;
        Nba97GameSelectionWrite* write;
        if(controller==NBA97_SELECTION_UNKNOWN_REF) return NBA97_SELECTION_UNRESOLVED;
        if(source.controller[controller].team_base<0) continue;
        side=source.controller[controller].team_base==0 ? 0:5;
        result=entity_ref(&source,side,&start);
        if(result!=NBA97_SELECTION_OK) return result;
        for(local=0;local<5;++local) {
            if(start+local>=11) return NBA97_SELECTION_OUTSIDE_STORAGE;
            if(next.claim[start+local]==(int16_t)i) {already_claimed=1;break;}
        }
        /* Original bug/quirk: existing claim does not repair selected+26. */
        if(already_claimed) continue;
        for(local=0;local<5;++local) {
            unsigned entity=start+local;
            if(next.claim[entity]<0) {
                uint32_t dx,dy;
                int32_t distance;
                if(source.ball==NBA97_SELECTION_UNKNOWN_REF) return NBA97_SELECTION_UNRESOLVED;
                dx=(uint32_t)source.entity[source.ball].x-(uint32_t)source.entity[entity].x;
                dy=(uint32_t)source.entity[source.ball].y-(uint32_t)source.entity[entity].y;
                distance=nba97_game_selection_distance(signed_word(shift_right(dx,8)),
                                                       signed_word(shift_right(dy,8)));
                if(distance<=best) {
                    best=distance;
                    next.scratch_s6.word=side+local;
                    next.scratch_s6.known=1;
                }
            }
        }
        /* Original bug: no accepted candidate leaves s6 stale, including the
         * incoming register value for the first failed search. No fallback. */
        if(!next.scratch_s6.known) return NBA97_SELECTION_UNRESOLVED;
        /* SLL s6,2 discards the high two bits before the source table access. */
        result=entity_ref(&source,next.scratch_s6.word&0x3fffffffu,&target);
        if(result!=NBA97_SELECTION_OK) return result;
        next.selected[controller].word=(uint16_t)next.scratch_s6.word;
        next.selected[controller].known=1;
        next.selected_written[controller]=1;
        next.claim[target]=(int16_t)i;
        next.claim_written[target]=1;
        write=&next.writes[next.write_count++];
        write->raw_s6=next.scratch_s6.word;
        write->selected_word=(uint16_t)next.scratch_s6.word;
        write->logical_controller=(uint8_t)i;
        write->controller_record=(uint8_t)controller;
        write->entity_record=(uint8_t)target;
    }
    if(source.tail_state!=0) {
        unsigned entity;
        int result=entity_ref(&source,(uint32_t)(int32_t)source.tail_entity,&entity);
        /* Source reads the reference/claim before testing tail_state<9. */
        if(result!=NBA97_SELECTION_OK) return result;
        if(next.claim[entity]<0 && source.tail_state<9) {
            next.call_7a36c=(uint8_t)(source.tail_state>=2);
            next.tail_state=1;
            next.tail_state_written=1;
        }
    }
    memcpy(out,&next,sizeof(next));
    return NBA97_SELECTION_OK;
}
