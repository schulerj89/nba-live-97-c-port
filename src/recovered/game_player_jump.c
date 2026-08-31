#include "game_player_jump.h"
#include <string.h>
static const uint8_t offsets[24]={0,8,12,16,20,22,24,26,72,76,80,96,100,158,160,164,166,168,186,188,190,192,196,217};
static const uint32_t addresses[11]={0xfe8cc,0xfe8ca,0xfdb90,0x21d93,0xfdb94,0xfdc1e,0xfdc04,0xfdc08,0xfdc0a,0xfdc0c,0xfdc14};
unsigned nba97_game_jump_entity_offset(unsigned f){return f<24?offsets[f]:0;}
unsigned nba97_game_jump_entity_width(unsigned f){return f<4?4:f==7||f==23?1:f<24?2:0;}
unsigned nba97_game_jump_global_address(unsigned f){return f<11?addresses[f]:0;}
unsigned nba97_game_jump_global_width(unsigned f){return f==3?1:f<11?2:0;}
static int valid(Nba97GamePeriodValue v,unsigned width){return v.known<=1&&(v.known||!v.word)&&(width==4||v.word<(1u<<(width*8)));}
static int valid_state(const Nba97GamePlayerJumpState* s){
    unsigned i,f;
    if(s->player_count>24||s->status_count>24)return 0;
    for(i=0;i<11;++i){
        for(f=0;f<24;++f)if(!valid(s->entity[i][f],nba97_game_jump_entity_width(f)))return 0;
        if(!valid(s->player_reference[i],4)||!valid(s->status_reference[i],4))return 0;
    }
    for(i=0;i<s->player_count;++i)if(!valid(s->player[i].byte09,1)||!valid(s->player[i].byte17,1))return 0;
    for(i=0;i<s->status_count;++i)if(!valid(s->status20[i],2))return 0;
    for(f=0;f<11;++f)if(!valid(s->global[f],nba97_game_jump_global_width(f)))return 0;
    return valid(s->rng1edee,2)&&s->ball_fdc48.known<=1&&(s->ball_fdc48.known||!s->ball_fdc48.record);
}
static int32_t s32(uint32_t v){return v<0x80000000u?(int32_t)v:(int32_t)((int64_t)v-0x100000000LL);}
static int32_t s16(uint32_t v){return v<32768?(int32_t)v:(int32_t)v-65536;}
static uint32_t sar(uint32_t v,unsigned n){return(v>>n)|((v&0x80000000u)?(~0u<<(32-n)):0);}
static int read_value(Nba97GamePeriodValue v,uint32_t* out){if(!v.known)return NBA97_JUMP_UNRESOLVED;*out=v.word;return NBA97_JUMP_OK;}
#define TRY(op) do{int result_=(op);if(result_!=NBA97_JUMP_OK)return result_;}while(0)
#define E(f,v) TRY(read_value(s->entity[entity][NBA97_JUMP_##f],&(v)))
#define B(f,v) TRY(read_value(s->entity[ball][NBA97_JUMP_##f],&(v)))
#define G(f,v) TRY(read_value(s->global[NBA97_JUMP_##f],&(v)))
static int player_value(const Nba97GamePlayerJumpState* s,unsigned entity,unsigned byte,uint32_t* out){
    uint32_t ref;TRY(read_value(s->player_reference[entity],&ref));
    if(ref>=s->player_count)return NBA97_JUMP_REFERENCE;
    return read_value(byte==9?s->player[ref].byte09:s->player[ref].byte17,out);
}
int nba97_game_jump_rng(Nba97GameJumpRngEffects* out,const Nba97GamePeriodValue* previous){
    Nba97GameJumpRngEffects e;uint32_t v;
    if(!out||!previous||!valid(*previous,2))return NBA97_JUMP_ARGUMENT;
    TRY(read_value(*previous,&v));memset(&e,0,sizeof(e));
    if(!v){v=0xa5a5;e.write[e.count++]=(uint16_t)v;}
    /* Original SLL17 tests bit14, not bit15. Zero replacement is an actual
     * preceding SH, not an alternate seed that may be privately recreated. */
    v=((v<<1)^((v&0x4000)?0x1d87u:0))&65535;e.write[e.count++]=(uint16_t)v;
    e.state.word=v;e.state.known=1;e.value=(uint16_t)v;memcpy(out,&e,sizeof(e));return NBA97_JUMP_OK;
}
static void store(Nba97GamePlayerJumpState* s,Nba97GameJumpReceipt* r,unsigned kind,unsigned entity,unsigned field,Nba97GamePeriodValue v){
    Nba97GameJumpEvent* e=&r->event[r->count++];
    unsigned width=kind?2:nba97_game_jump_entity_width(field);
    if(width<4)v.word&=(1u<<(width*8))-1;
    e->kind=(uint8_t)kind;e->entity=(uint8_t)entity;e->field=(uint8_t)field;e->value=v;
    if(kind==2)s->rng1edee=v;else if(kind==1)s->global[field]=v;else s->entity[entity][field]=v;
}
static void assign(Nba97GamePlayerJumpState* s,Nba97GameJumpReceipt* r,unsigned entity,unsigned field,uint32_t value){
    Nba97GamePeriodValue v={value,1};store(s,r,0,entity,field,v);
}
#define SET(f,v) assign(s,receipt,entity,NBA97_JUMP_##f,(uint32_t)(v))
static Nba97GameJumpEvent* event_call(Nba97GameJumpReceipt* r,unsigned owner,unsigned entity,uint32_t site,uint32_t a1,uint32_t a2){
    Nba97GameJumpEvent* e=&r->event[r->count++];e->kind=3;e->call.owner=(uint8_t)owner;
    e->call.entity=(uint8_t)entity;e->call.callsite=site;e->call.argument[0]=a1;e->call.argument[1]=a2;
    e->call.argument_count=(uint8_t)(owner==0?0:owner==1||owner==5?2:1);return e;
}
static int boundary(Nba97GamePlayerJumpState* s,Nba97GameJumpReceipt* r,Nba97GameJumpCallback callback,void* context,
                    unsigned owner,unsigned entity,uint32_t site,uint32_t a1,uint32_t a2){
    Nba97GameJumpEvent* e=event_call(r,owner,entity,site,a1,a2);int result;
    if(!callback)return NBA97_JUMP_PENDING;
    result=callback(context,s,&e->call);
    if(result!=1)return result==0?NBA97_JUMP_PENDING:NBA97_JUMP_CALLBACK_FAILED;
    if(!valid_state(s))return NBA97_JUMP_ARGUMENT;
    e->completed=1;return NBA97_JUMP_OK;
}
#define CALL(owner,site,a1,a2) TRY(boundary(s,receipt,callback,context,NBA97_JUMP_CALL_##owner,entity,site,a1,a2))
static int run(Nba97GamePlayerJumpState* s,unsigned entity,uint32_t argument,const Nba97GamePlayerJumpResources* resources,
               Nba97GameJumpCallback callback,void* context,Nba97GameJumpReceipt* receipt){
    uint32_t v,other,rating,ball_height,index,status,angle,row,x,y,denominator;unsigned ball,side;
    int32_t delta,bonus;Nba97GamePeriodReference ball_ref;Nba97GameJumpRngEffects rng;Nba97GameJumpEvent* rng_call;
    G(FE8CC,v);
    if(v==1){G(FE8CA,v);E(00,other);if((uint32_t)s16(v)==other)goto phase_allowed;}
    G(FDB90,v);if(s16(v)>=128&&v!=129)return NBA97_JUMP_OK;
phase_allowed:
    /* Original captures FDC48 before any rating/height rejection. Resolving
     * that pointer's owned target is deferred until its first dereference. */
    ball_ref=s->ball_fdc48;TRY(player_value(s,entity,0x17,&rating));E(BE,v);
    if(argument){if(((rating+(rating>>2))>>2)<v)return NBA97_JUMP_OK;}
    else if(v>40)return NBA97_JUMP_OK;
    if(!ball_ref.known)return NBA97_JUMP_UNRESOLVED;
    if(ball_ref.record>=11)return NBA97_JUMP_REFERENCE;
    ball=ball_ref.record;B(10,ball_height);
    if(s32(ball_height)<0x4800&&argument)return NBA97_JUMP_OK;
    delta=s32(sar(ball_height,8))-s32(sar(rating-50u,3)+83u);
    if(s32(sar(ball_height,8))>=88)goto height_test;
    B(18,v);if(s16(v)<192)goto height_allowed;
height_test:
    if(delta>=0){
        G(FDB90,v);side=v==129?1u:0u;index=(uint32_t)delta>>1;
        /* Source still loads this unchecked table for argument0 even though
         * its rejection branch is disabled. Do not skip an unowned access. */
        B(18,v);
        if(!resources||!resources->threshold[side]||index>=resources->threshold_count[side])return NBA97_JUMP_REFERENCE;
        if(s16(v)>=s16(resources->threshold[side][index])&&argument)return NBA97_JUMP_OK;
    }
height_allowed:
    G(21D93,v);bonus=32;
    if(v){
        TRY(read_value(s->status_reference[entity],&index));if(index>=s->status_count)return NBA97_JUMP_REFERENCE;
        TRY(read_value(s->status20[index],&status));bonus=s32(sar(status<<16,26));
    }
    rng_call=event_call(receipt,NBA97_JUMP_CALL_2AB70,entity,0x8006a48c,0,0);
    TRY(nba97_game_jump_rng(&rng,&s->rng1edee));
    for(index=0;index<rng.count;++index){Nba97GamePeriodValue value={rng.write[index],1};store(s,receipt,2,0,0,value);}
    rng_call->completed=1;
    /* Original consumes the shared RNG even for argument0, where the random
     * rejection is disabled; later failures retain that consumed draw. */
    if(bonus+(int32_t)rating-30<(int32_t)(rng.value&127)&&argument)return NBA97_JUMP_OK;
    E(D9,v);G(FDB94,other);if(v!=(uint32_t)s16(other)||rating<75)goto ordinary;
    G(FDC1E,v);if(v)goto ordinary;
    B(A0,v);if(s16(v)>=129)goto ordinary;
    B(BA,v);if(v>=37)goto ordinary;
    E(BA,v);if(v>=65)goto ordinary;
    E(A8,v);E(BC,angle);if((((uint32_t)s16(v)-(uint32_t)s16(angle)+128u)&1023)>=257)goto ordinary;
    TRY(player_value(s,entity,9,&v));if(v>=95)goto ordinary;
    CALL(5A570,0x8006a55c,(uint32_t)s16(angle),0xffffffffu);
    store(s,receipt,0,entity,NBA97_JUMP_A6,s->entity[entity][NBA97_JUMP_A4]);
    G(FDC04,row);SET(4C,65535);SET(48,65535);SET(64,0);SET(60,0);
    if(!resources||!resources->motion_b86f4||row>=resources->motion_row_count)return NBA97_JUMP_REFERENCE;
    index=row*4;CALL(5699C,0x8006a5a4,resources->motion_b86f4[index],0);
    CALL(56AA4,0x8006a5b0,resources->motion_b86f4[index+1],0);
    store(s,receipt,0,entity,NBA97_JUMP_50,s->global[NBA97_JUMP_FDC0C]);
    CALL(56CE0,0x8006a5d0,resources->motion_b86f4[index+2],resources->motion_b86f4[index+3]);
    store(s,receipt,0,entity,NBA97_JUMP_18,s->global[NBA97_JUMP_FDC14]);
    G(FDC08,v);G(FDC0C,other);G(FDC0A,denominator);v=((v-other)<<9)+240;
    if(!denominator)return NBA97_JUMP_SOURCE_DIVZERO;
    /* Denominator is LHU, so the compiler's INT_MIN/-1 BREAK path cannot
     * occur. Retain signed division/truncation and the source zero trap. */
    SET(1A,19);SET(9E,s32(v)/(int32_t)denominator);
    {Nba97GamePeriodValue zero={0,1};store(s,receipt,1,0,NBA97_JUMP_FDB90,zero);}
    receipt->accepted=1;return NBA97_JUMP_OK;
ordinary:
    B(08,v);E(08,other);x=(uint32_t)(s32(v-other)/20);
    B(0C,v);E(0C,other);y=(uint32_t)(s32(v-other)/20);
    /* SUBU wraps before signed /20; low rating arithmetic-halves before the
     * distance gate. Negative odd values round down, not toward zero. */
    if(rating<66){x=sar(x,1);y=sar(y,1);}
    if(s32(x)<-512||s32(x)>512||s32(y)<-512||s32(y)>512)return NBA97_JUMP_OK;
    SET(14,x);SET(16,y);SET(C4,600);G(FDB90,v);
    if(v==129){
        /* Phase81 quarters the newly calculated signed velocities. The
         * fallback inside61760 has different semantics; do not merge them. */
        E(14,v);E(16,other);SET(14,sar((uint32_t)s16(v),2));SET(16,sar((uint32_t)s16(other),2));
        CALL(56B78,0x8006a724,77,0);CALL(56CE0,0x8006a734,78,0);v=79;
    }else{
        store(s,receipt,0,entity,NBA97_JUMP_A6,s->entity[entity][NBA97_JUMP_C0]);
        CALL(56B78,0x8006a750,74,0);CALL(56CE0,0x8006a760,75,0);v=76;
    }
    CALL(56CE0,0x8006a770,v,0);receipt->accepted=1;return NBA97_JUMP_OK;
}
int nba97_game_player_jump(Nba97GamePlayerJumpState* s,unsigned entity,uint32_t argument,
                          const Nba97GamePlayerJumpResources* resources,Nba97GameJumpCallback callback,
                          void* context,Nba97GameJumpReceipt* receipt){
    int result;if(!s||!receipt||entity>=11||!valid_state(s))return NBA97_JUMP_ARGUMENT;
    memset(receipt,0,sizeof(*receipt));result=run(s,entity,argument,resources,callback,context,receipt);
    if(result==NBA97_JUMP_OK)receipt->completed=1;return result;
}
