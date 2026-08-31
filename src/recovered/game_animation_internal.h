#ifndef NBA97_GAME_ANIMATION_INTERNAL_H
#define NBA97_GAME_ANIMATION_INTERNAL_H
#include "game_animation_advance.h"
#include <string.h>
static const uint8_t ai_animation_offset[16]={0x46,0x48,0x4a,0x4c,0x4e,0x50,0x54,0x58,0x5c,0x60,0x64,0x70,0x78,0x94,0x96,0x9a};
static const uint8_t ai_extra_offset[23]={0x14,0x16,0x18,0x52,0x56,0x5a,0x5e,0x62,0x66,0x72,0x74,0x76,0x7a,0x7c,0x7e,0x80,0x9c,0x9e,0xa0,0xc4,0xe4,0xe6,0xea};
static int ai_value_valid(Nba97GamePeriodValue v,unsigned width) {
    return v.known<=1 && (v.known || !v.word) && (width==4 || v.word<(1u<<(8*width)));
}
static int ai_valid(const Nba97GameAnimationActor* s) {
    unsigned i,ch;
    if(s->animation.height_known>1 || (!s->animation.height_known && s->animation.height10) ||
       (s->extra_known>>23) || !ai_value_valid(s->word00,4) ||
       !ai_value_valid(s->previous_height2c,4) || !ai_value_valid(s->actor1a,1)) return 0;
    for(i=0;i<16;++i) if(!(s->animation.known&(1u<<i)) && s->animation.field[i]) return 0;
    for(i=0;i<23;++i) if(!(s->extra_known&(1u<<i)) && s->extra[i]) return 0;
    for(ch=0;ch<2;++ch) {
        if(s->queue_blend_known[ch]&0xf0) return 0;
        for(i=0;i<4;++i) if(!(s->queue_blend_known[ch]&(1u<<i)) && s->queue_blend[ch][i]) return 0;
    }
    return 1;
}
static Nba97GamePeriodValue ai_get(const Nba97GameAnimationActor* s,unsigned offset) {
    unsigned i;Nba97GamePeriodValue v={0,0};
    if(offset==0) return s->word00;
    if(offset==0x2c) return s->previous_height2c;
    if(offset==0x1a) return s->actor1a;
    if(offset==0x10) {v.word=(uint32_t)s->animation.height10;v.known=s->animation.height_known;return v;}
    for(i=0;i<16;++i) if(ai_animation_offset[i]==offset) {v.word=s->animation.field[i];v.known=(uint8_t)((s->animation.known>>i)&1);return v;}
    for(i=0;i<23;++i) if(ai_extra_offset[i]==offset) {v.word=s->extra[i];v.known=(uint8_t)((s->extra_known>>i)&1);return v;}
    if(offset>=0x68 && offset<0x70) {
        unsigned ch=(offset-0x68)/4,slot=offset&3;
        v.word=s->queue_blend[ch][slot];v.known=(uint8_t)((s->queue_blend_known[ch]>>slot)&1);
    }
    return v;
}
static int ai_read(const Nba97GameAnimationActor* s,unsigned offset,uint32_t* value) {
    Nba97GamePeriodValue v=ai_get(s,offset);
    if(!v.known) return NBA97_ANIMATION_UNRESOLVED;
    *value=v.word;return NBA97_ANIMATION_OK;
}
static void ai_put(Nba97GameAnimationEffects* e,unsigned offset,Nba97GamePeriodValue v) {
    unsigned i;
    ++e->store_count;
    for(i=0;i<16;++i) if(ai_animation_offset[i]==offset) {
        e->state.animation.field[i]=(uint16_t)v.word;
        e->state.animation.known=(uint16_t)((e->state.animation.known&~(1u<<i))|(v.known?(1u<<i):0));
        e->animation_written=(uint16_t)(e->animation_written|(1u<<i));return;
    }
    for(i=0;i<23;++i) if(ai_extra_offset[i]==offset) {
        e->state.extra[i]=(uint16_t)v.word;
        e->state.extra_known=(e->state.extra_known&~(1u<<i))|(v.known?(1u<<i):0);
        e->extra_written|=1u<<i;return;
    }
    if(offset>=0x68 && offset<0x70) {
        unsigned ch=(offset-0x68)/4,slot=offset&3;
        e->state.queue_blend[ch][slot]=(uint8_t)v.word;
        e->state.queue_blend_known[ch]=(uint8_t)((e->state.queue_blend_known[ch]&~(1u<<slot))|(v.known?(1u<<slot):0));
        e->queue_blend_written[ch]=(uint8_t)(e->queue_blend_written[ch]|(1u<<slot));
    }
}
static void ai_assign(Nba97GameAnimationEffects* e,unsigned offset,uint32_t value) {
    Nba97GamePeriodValue v={value,1};ai_put(e,offset,v);
}
#define AI_TRY(operation) do {int ai_result_=(operation);if(ai_result_!=NBA97_ANIMATION_OK)return ai_result_;}while(0)
#define AI_READ(e,offset,destination) AI_TRY(ai_read(&(e)->state,offset,&(destination)))
#endif
