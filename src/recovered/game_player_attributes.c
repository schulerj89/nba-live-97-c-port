#include "game_player_attributes.h"
#include <string.h>
static int64_t signed_word(uint32_t v) {return v<UINT32_C(0x80000000)?(int64_t)v:(int64_t)v-INT64_C(4294967296);}
static int32_t arithmetic_half(int32_t n) {return n>=0?n/2:-((-n+1)/2);}
static int publish(Nba97GamePlayerAttributesEffects* out,const Nba97GamePlayerAttributesEffects* next,int result) {
    memcpy(out,next,sizeof(*out));return result;
}
int nba97_game_player_attributes(Nba97GamePlayerAttributesEffects* out,
                                 const Nba97GamePlayerAttributesInput* input) {
    Nba97GamePlayerAttributesInput in;
    Nba97GamePlayerAttributesEffects next;
    unsigned i;
    if(!out || !input)return NBA97_ATTRIBUTES_ARGUMENT;
    memcpy(&in,input,sizeof(in));
    if(!in.players || in.first_known>1 || in.divisor_known>1 || in.flag_known>1 ||
       (!in.first_known && in.first_entity) || (!in.divisor_known && in.divisor64) ||
       (!in.flag_known && in.flag21498))return NBA97_ATTRIBUTES_ARGUMENT;
    for(i=0;i<11;++i)if(in.entity[i].word00_known>1 || in.entity[i].player_known>1 ||
       (!in.entity[i].word00_known && in.entity[i].word00) ||
       (!in.entity[i].player_known && in.entity[i].player_reference))return NBA97_ATTRIBUTES_ARGUMENT;
    memset(&next,0,sizeof(next));next.stopped_entity=255;
    if(!in.first_known)return publish(out,&next,NBA97_ATTRIBUTES_UNRESOLVED);
    for(i=0;i<10;++i) {
        unsigned entity=(unsigned)in.first_entity+i,height_index;
        const Nba97GameAttributeEntity* e;
        const Nba97GameAttributePlayer* p;
        Nba97GameAttributeEntityEffect* effect;
        int32_t rating_divisor,numerator;
        next.stopped_entity=(uint8_t)entity;
        if(entity>=11)return publish(out,&next,NBA97_ATTRIBUTES_REFERENCE);
        e=&in.entity[entity];effect=&next.entity[entity];
        if(!e->player_known || !e->word00_known)return publish(out,&next,NBA97_ATTRIBUTES_UNRESOLVED);
        if(e->player_reference>=in.player_count)return publish(out,&next,NBA97_ATTRIBUTES_REFERENCE);
        p=&in.players[e->player_reference];
        /* Original SLL wraps before address addition, including e.g.80000000
         * aliasing height slot0. Never initialize word00 from table position. */
        height_index=e->word00&UINT32_C(0x3fffffff);
        if(height_index>=11)return publish(out,&next,NBA97_ATTRIBUTES_REFERENCE);
        ++next.visited_entities;
        next.height165f48[height_index]=(uint32_t)p->byte09*624u;
        next.height_written|=(uint16_t)(1u<<height_index);
        effect->field[NBA97_ATTRIBUTE_3A]=(uint16_t)((unsigned)p->byte20<<3);
        effect->written|=1u<<NBA97_ATTRIBUTE_3A;
        rating_divisor=((int32_t)p->byte1e-50)*24/50+12;
        /* Real byte1E values23,24,25 divide by zero. Preserve BREAK1C00 at
         *63F80 and its height/+3A prefix; do not clamp them to a valid rating. */
        if(rating_divisor==0)return publish(out,&next,NBA97_ATTRIBUTES_RATING_DIVIDE_TRAP);
        numerator=(546/rating_divisor)*36;
        if(!in.divisor_known)return publish(out,&next,NBA97_ATTRIBUTES_UNRESOLVED);
        if(in.divisor64==0)return publish(out,&next,NBA97_ATTRIBUTES_RATE_DIVIDE_TRAP);
        /* Numerator is bounded to +/-19656. Neither original INT_MIN/-1
         * diagnostic can fire; signed negative divisors remain supported. */
        effect->field[NBA97_ATTRIBUTE_44]=(uint16_t)((int64_t)numerator/signed_word(in.divisor64));
        effect->written|=1u<<NBA97_ATTRIBUTE_44;
        effect->field[NBA97_ATTRIBUTE_3C]=(uint16_t)(arithmetic_half(99-(int32_t)p->byte1b)+44);
        effect->written|=1u<<NBA97_ATTRIBUTE_3C;
        effect->field[NBA97_ATTRIBUTE_3E]=(uint16_t)(arithmetic_half(99-(int32_t)p->byte14)+32);
        effect->written|=1u<<NBA97_ATTRIBUTE_3E;
        effect->field[NBA97_ATTRIBUTE_40]=(uint16_t)(((int32_t)p->byte1c-50)*15/47);
        effect->written|=1u<<NBA97_ATTRIBUTE_40;
        effect->field[NBA97_ATTRIBUTE_42]=(uint16_t)(((int32_t)p->byte15-50)*15/47);
        effect->written|=1u<<NBA97_ATTRIBUTE_42;
    }
    next.stopped_entity=255;
    if(!in.flag_known)return publish(out,&next,NBA97_ATTRIBUTES_UNRESOLVED);
    if(in.flag21498) {
        next.tail_count=3;next.tail[0]=NBA97_ATTRIBUTE_4D9EC;
        next.tail[1]=NBA97_ATTRIBUTE_35A44;next.tail[2]=NBA97_ATTRIBUTE_38A18;
        return publish(out,&next,NBA97_ATTRIBUTES_TAILS_REQUIRED);
    }
    return publish(out,&next,NBA97_ATTRIBUTES_COMPLETE);
}
