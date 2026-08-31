#include "team_header.h"
#include <string.h>

static int valid_reference(const Nba97TeamHeaderRef* ref) {
    switch(ref->kind) {
    case NBA97_TEAM_REF_UNKNOWN:
    case NBA97_TEAM_REF_NULL:
        return ref->payload==0;
    case NBA97_TEAM_REF_ENTITY:
        return ref->payload<10;
    case NBA97_TEAM_REF_OPAQUE_WORD:
        return 1;
    default:
        return 0;
    }
}

static void copy_reference(Nba97TeamHeaderRef* out,const Nba97TeamHeaderRef* ref) {
    out->kind=ref->kind;
    out->payload=ref->payload;
}

int nba97_team_header_initialize(Nba97TeamHeaderEffects* out,
                                const Nba97TeamHeaderInput* input) {
    Nba97TeamHeaderInput source;
    Nba97TeamHeaderEffects next;
    unsigned side,i;
    if(!out || !input) return 0;
    memcpy(&source,input,sizeof(source));
    if(!((source.side_word==0 && source.opponent_side_word==5) ||
         (source.side_word==5 && source.opponent_side_word==0)) ||
       !valid_reference(&source.table12) || !valid_reference(&source.table24)) return 0;

    /* Initialize only the native effect object, never a source team header. */
    memset(&next,0,sizeof(next));
    side=source.side_word!=0;
    next.metadata_side=(uint8_t)side;
    next.alias_side=(uint8_t)side;
    next.opponent_side=(uint8_t)(1-side);
    if(side) copy_reference(&next.word08,&source.table12);
    else next.word08.kind=NBA97_TEAM_REF_ENTITY; /* This call registered entity0. */
    copy_reference(&next.word0c,side ? &source.table24:&source.table12);
    next.direction10=side ? 0x14e00:-0x14e00;
    next.field34=7;next.field38=7;next.field39=5;
    next.count66=next.count68=(uint16_t)(source.count<12 ? source.count:12);
    next.field62=(uint16_t)(120u-2u*source.rank57);
    next.field72=(uint16_t)((source.rank57+28u)/(source.difficulty>=2 ? 2u:1u));
    next.field74=(uint16_t)(1260u-32u*source.rank54);
    memcpy(next.saved_lineup,source.lineup,sizeof(next.saved_lineup));
    for(i=0;i<12;++i)
        next.status[i]=(uint16_t)(i<next.count66 && i!=source.injury_slot ? 0x7fff:0xfffe);
    for(i=0;i<5;++i) {
        unsigned local=4-i;
        next.entity[i].table_slot=(uint16_t)(source.side_word+local);
        next.entity[i].entity_id=next.entity[i].table_slot;
        next.entity[i].opponent_d6=(uint16_t)(source.opponent_side_word+local);
    }
    memcpy(out,&next,sizeof(next));
    return 1;
}
