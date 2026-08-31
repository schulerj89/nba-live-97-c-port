#include "game_player_initialization.h"
#include <string.h>

static const uint8_t animation_offset[16]={
    0x46,0x48,0x4a,0x4c,0x4e,0x50,0x54,0x58,
    0x5c,0x60,0x64,0x70,0x78,0x94,0x96,0x9a
};
static int valid_animation(const Nba97GameAnimationState* s) {
    unsigned i;
    if(s->height_known>1 || (!s->height_known && s->height10)) return 0;
    for(i=0;i<16;++i) if(!(s->known&(1u<<i)) && s->field[i]) return 0;
    return 1;
}
static void assign(Nba97GameAnimationResetEffects* e,unsigned field,uint16_t value) {
    e->state.field[field]=value;
    e->state.known=(uint16_t)(e->state.known|(1u<<field));
    e->written=(uint16_t)(e->written|(1u<<field));
}
static void copy_field(Nba97GameAnimationResetEffects* e,unsigned to,unsigned from) {
    e->state.field[to]=e->state.field[from];
    e->state.known=(uint16_t)((e->state.known&~(1u<<to))|
        ((e->state.known&(1u<<from)) ? (1u<<to):0));
    e->written=(uint16_t)(e->written|(1u<<to));
}
static void clear_status(Nba97GameAnimationResetEffects* e,uint16_t mask) {
    e->state.field[NBA97_ANIM_9A]=(uint16_t)(e->state.field[NBA97_ANIM_9A]&mask);
    e->written=(uint16_t)(e->written|(1u<<NBA97_ANIM_9A));
}
static int reset_frame_if_needed(Nba97GameAnimationResetEffects* e,unsigned frame,
                                 unsigned time,uint8_t count) {
    /* Count0 forces a reset for every raw unsigned frame, even UNKNOWN. */
    if(count && !(e->state.known&(1u<<frame))) return NBA97_PLAYER_INIT_UNRESOLVED;
    if(!count || e->state.field[frame]>=count) {assign(e,time,0);assign(e,frame,0);}
    return NBA97_PLAYER_INIT_OK;
}

int nba97_game_player_animation_force_reset(Nba97GameAnimationResetEffects* out,
                                           const Nba97GameAnimationResetInput* input) {
    Nba97GameAnimationResetInput source;
    Nba97GameAnimationResetEffects next;
    unsigned request;
    int result;
    if(!out || !input) return NBA97_PLAYER_INIT_ARGUMENT;
    memcpy(&source,input,sizeof(source));
    if(!valid_animation(&source.previous) || source.motion[0].available>2 ||
       source.motion[1].available>2) return NBA97_PLAYER_INIT_ARGUMENT;
    if(!source.previous.height_known) return NBA97_PLAYER_INIT_UNRESOLVED;
    if(source.previous.height10) request=0x25;
    else {
        if(!(source.previous.known&(1u<<NBA97_ANIM_4E))) return NBA97_PLAYER_INIT_UNRESOLVED;
        request=source.previous.field[NBA97_ANIM_4E];
    }
    if(request>=84 || source.motion_index!=request) return NBA97_PLAYER_INIT_MOTION_REFERENCE;
    if(source.motion[0].available==2 || source.motion[1].available==2) return NBA97_PLAYER_INIT_UNRESOLVED;
    if(!source.motion[0].available || !source.motion[1].available) return NBA97_PLAYER_INIT_MOTION_REFERENCE;
    memset(&next,0,sizeof(next));
    next.state=source.previous;

    /* 56F5C(force1), then56AA4. Source upper/secondary channel goes FIRST. */
    assign(&next,NBA97_ANIM_4A,0xffff);assign(&next,NBA97_ANIM_64,0);
    assign(&next,NBA97_ANIM_4C,0xffff);clear_status(&next,0xfff7);
    assign(&next,NBA97_ANIM_78,0xffff);assign(&next,NBA97_ANIM_96,0);
    clear_status(&next,0xfff7);
    assign(&next,NBA97_ANIM_4A,(uint16_t)request);
    if(source.motion[1].flags&1) {
        assign(&next,NBA97_ANIM_5C,0);assign(&next,NBA97_ANIM_54,0);
    } else {
        if(request<21) assign(&next,NBA97_ANIM_4E,(uint16_t)request);
        /* Intentional source quirk: forced reset preserves an in-range frame
         * and its old timing for a non-bit1 clip. No unconditional rewind. */
        result=reset_frame_if_needed(&next,NBA97_ANIM_54,NBA97_ANIM_5C,source.motion[1].count7);
        if(result!=NBA97_PLAYER_INIT_OK) return result;
    }
    assign(&next,NBA97_ANIM_64,source.motion[1].flags);

    /* 56EBC(force1), then5699C, after secondary state has changed. */
    assign(&next,NBA97_ANIM_46,0xffff);assign(&next,NBA97_ANIM_60,0);
    assign(&next,NBA97_ANIM_48,0xffff);clear_status(&next,0xfffb);
    assign(&next,NBA97_ANIM_70,0xffff);assign(&next,NBA97_ANIM_94,0);
    clear_status(&next,0xfffb);
    assign(&next,NBA97_ANIM_46,(uint16_t)request);
    if(source.motion[0].flags&1) {
        assign(&next,NBA97_ANIM_58,0);assign(&next,NBA97_ANIM_50,0);
    } else {
        if(request<21) assign(&next,NBA97_ANIM_4E,(uint16_t)request);
        if(source.motion[0].mode2==2) {
            /* Original does not clamp this copied frame against primary
             * count7. Unequal channel counts can leave it out of range. */
            copy_field(&next,NBA97_ANIM_50,NBA97_ANIM_54);
            copy_field(&next,NBA97_ANIM_58,NBA97_ANIM_5C);
        } else {
            result=reset_frame_if_needed(&next,NBA97_ANIM_50,NBA97_ANIM_58,source.motion[0].count7);
            if(result!=NBA97_PLAYER_INIT_OK) return result;
        }
    }
    assign(&next,NBA97_ANIM_60,source.motion[0].flags);
    memcpy(out,&next,sizeof(next));
    return NBA97_PLAYER_INIT_OK;
}

static void write_header(Nba97GamePlayerInitializationEffects* e,unsigned offset,
                          uint32_t value,unsigned size) {
    unsigned i;
    for(i=0;i<size;++i) {e->header_value[offset+i]=(uint8_t)(value>>(8*i));e->header_written[offset+i]=1;}
}
static void write_entity(Nba97GamePlayerEntityInitialization* e,unsigned offset,
                          uint32_t value,unsigned size,int known) {
    unsigned i;
    for(i=0;i<size;++i) {
        e->value[offset+i]=known ? (uint8_t)(value>>(8*i)):0;
        e->written[offset+i]=1;e->known[offset+i]=(uint8_t)known;
    }
}

int nba97_game_player_initialize(Nba97GamePlayerInitializationEffects* out,
                                 const Nba97GamePlayerInitializationInput* input) {
    static const uint8_t cleared_half[]={0x98,0xe6,0xe4,0xb8,0xb6,0xb4,0xa0,0x4e,0x9c,0x18,0x16,0x14};
    static const uint8_t cleared_byte[]={0xce,0xdf,0xde,0xdd};
    Nba97GamePlayerInitializationInput source;
    Nba97GamePlayerInitializationEffects next;
    unsigned local,i;
    if(!out || !input) return NBA97_PLAYER_INIT_ARGUMENT;
    memcpy(&source,input,sizeof(source));
    if(source.side_word>5) return NBA97_PLAYER_INIT_STORAGE;
    if(source.cumulative_known>1 || source.sum_known>1 ||
       (!source.cumulative_known && source.previous_cumulative48) ||
       (!source.sum_known && (source.previous_b4 || source.header32))) return NBA97_PLAYER_INIT_ARGUMENT;
    if((source.period && !source.cumulative_known) || !source.sum_known) return NBA97_PLAYER_INIT_UNRESOLVED;
    for(local=0;local<5;++local)
        if(source.player_byte_d_known[local]>1 ||
           (!source.player_byte_d_known[local] && source.player_byte_d[local]) ||
           !valid_animation(&source.previous_animation[local])) return NBA97_PLAYER_INIT_ARGUMENT;
    memset(&next,0,sizeof(next));
    write_header(&next,0x48,source.duration+(source.period ? source.previous_cumulative48:0),4);
    for(i=0x52;i<=0x5a;i+=2) write_header(&next,i,0xffff,2);
    write_header(&next,0xc0,0x708,2);write_header(&next,0xc2,2,2);
    write_header(&next,0xa2,0,2);write_header(&next,0x70,0,2);write_header(&next,0x30,0,2);
    write_header(&next,0xb4,(uint16_t)(source.previous_b4+source.header32),2);
    for(local=0;local<5;++local) {
        Nba97GamePlayerEntityInitialization* e=&next.entity[local];
        Nba97GameAnimationResetInput animation_input;
        Nba97GameAnimationResetEffects animation;
        int32_t x=source.formation[local][0],y=source.formation[local][1],angle=source.formation[local][2];
        uint16_t angle_word;
        int result;
        if(!source.player_byte_d_known[local]) return NBA97_PLAYER_INIT_UNRESOLVED;
        e->entity_index=e->table_slot=(uint8_t)(source.side_word+local);
        if(source.direction10>=0) {x=-x;y=-y;angle-=4;}
        write_entity(e,0x4,0xffff,2,1);write_entity(e,0xd9,source.side_word,1,1);
        for(i=0;i<sizeof(cleared_half);++i) write_entity(e,cleared_half[i],0,2,1);
        for(i=0;i<sizeof(cleared_byte);++i) write_entity(e,cleared_byte[i],0,1,1);
        write_entity(e,0x10,0,4,1);write_entity(e,0x9e,0x100,2,1);
        write_entity(e,0,e->entity_index,4,1);
        memset(&animation_input,0,sizeof(animation_input));
        animation_input.previous=source.previous_animation[local];
        /* These writes occur in65B18 before56FFC, so provenance is proven. */
        animation_input.previous.height10=0;animation_input.previous.height_known=1;
        animation_input.previous.field[NBA97_ANIM_4E]=0;
        animation_input.previous.known=(uint16_t)(animation_input.previous.known|(1u<<NBA97_ANIM_4E));
        memcpy(animation_input.motion,source.motion0,sizeof(source.motion0));
        result=nba97_game_player_animation_force_reset(&animation,&animation_input);
        if(result!=NBA97_PLAYER_INIT_OK) return result;
        for(i=0;i<16;++i) if(animation.written&(1u<<i))
            write_entity(e,animation_offset[i],animation.state.field[i],2,(animation.state.known&(1u<<i))!=0);
        write_entity(e,0x9a,source.player_byte_d[local] ? 3:0,2,1);
        if((source.special_center && local==0) ? source.player_byte_d[local]!=0:source.side_word!=0) {
            y=-y;angle=4-angle;
        }
        /* Cast before shifting: the source uses SLL, including negative x/y. */
        write_entity(e,8,(uint32_t)x<<8,4,1);write_entity(e,0xc,(uint32_t)y<<8,4,1);
        angle_word=(uint16_t)(((uint32_t)angle&7u)<<7);
        write_entity(e,0xa8,angle_word,2,1);write_entity(e,0xa6,angle_word,2,1);write_entity(e,0xa2,angle_word,2,1);
        write_entity(e,0x1a,local ? 2:4,1,1);write_entity(e,0xbe,local ? 0x50:0,2,1);
        write_entity(e,6,e->entity_index,2,1);
        for(i=0;i<244;++i) if(e->written[i] && !e->known[i]) ++next.unresolved_written_bytes;
    }
    memcpy(out,&next,sizeof(next));
    return NBA97_PLAYER_INIT_OK;
}
