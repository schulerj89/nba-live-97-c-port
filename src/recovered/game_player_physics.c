#include "game_player_physics.h"
#include <string.h>
static const uint8_t offsets[17]={0,8,12,16,20,22,24,26,36,40,44,156,160,162,194,200,238};
static const uint8_t widths[17]={4,4,4,4,2,2,2,1,4,4,4,2,2,2,2,2,2};
static const uint32_t addresses[14]={0xfdbcc,0xfe8e2,0xfdb6c,0xfe8e0,0xfdb90,0xfe8cc,0xfe8c4,0xfe8bc,0xfdbd4,0xfdb58,0x21d8f,0xfdb94,0xfe882,0xfe910};
unsigned nba97_game_physics_entity_offset(unsigned f){return f<17?offsets[f]:0;}
unsigned nba97_game_physics_entity_width(unsigned f){return f<17?widths[f]:0;}
unsigned nba97_game_physics_global_address(unsigned f){return f<14?addresses[f]:0;}
unsigned nba97_game_physics_global_width(unsigned f){return f==9||f==13?4:f==10?1:f<14?2:0;}
static int valid(Nba97GamePeriodValue v,unsigned width){return v.known<=1&&(v.known||!v.word)&&(width==4||v.word<(1u<<(width*8)));}
static int valid_state(const Nba97GamePlayerPhysicsState* s){
    unsigned i;for(i=0;i<17;++i)if(!valid(s->entity[i],widths[i]))return 0;
    for(i=0;i<14;++i)if(!valid(s->global[i],nba97_game_physics_global_width(i)))return 0;
    return valid(s->team_direction10,4);
}
static int32_t signed32(uint32_t v){return v<0x80000000u?(int32_t)v:(int32_t)((int64_t)v-0x100000000LL);}
static int32_t signed16(uint32_t v){return v<32768?(int32_t)v:(int32_t)v-65536;}
static uint32_t sar(uint32_t v,unsigned shift){return (v>>shift)|((v&0x80000000u)?(~0u<<(32-shift)):0);}
static int read_value(Nba97GamePeriodValue v,uint32_t* result){if(!v.known)return NBA97_PHYSICS_UNRESOLVED;*result=v.word;return NBA97_PHYSICS_OK;}
#define TRY(op) do{int result_=(op);if(result_!=NBA97_PHYSICS_OK)return result_;}while(0)
#define ENTITY(f,v) TRY(read_value(s->entity[NBA97_PHYSICS_##f],&(v)))
#define GLOBAL(f,v) TRY(read_value(s->global[NBA97_PHYSICS_##f],&(v)))
int nba97_game_direction_speed(Nba97GameDirectionEffects* out,uint32_t x,uint32_t y,
                              Nba97GamePeriodValue previous,const Nba97GameDirectionResources* r){
    Nba97GameDirectionEffects e;uint32_t octant=0,large,small,ratio,angle,value;
    if(!out||!valid(previous,2))return NBA97_PHYSICS_ARGUMENT;
    memset(&e,0,sizeof(e));e.angle=previous;
    if(!x&&!y){memcpy(out,&e,sizeof(e));return NBA97_PHYSICS_OK;}
    /* Source negation deliberately wraps INT_MIN; signed ordering below can
     * then select an unexpected major axis and even a zero divisor. */
    if(signed32(x)<0){x=0u-x;octant=2;}
    if(signed32(y)<0){y=0u-y;octant+=4;}
    if(signed32(x)<signed32(y)){value=x;x=y;y=value;++octant;}
    large=x;small=y;
    while(large>65535||small>65535){large>>=8;small>>=8;}
    if(large==small)angle=128;
    else {
        if(!large)return NBA97_PHYSICS_SOURCE_DIVZERO;
        ratio=(small<<16)/large;value=(ratio>>8)+((ratio&128)!=0);
        if(!r||!r->angle_d72b4||value>=r->angle_count)return NBA97_PHYSICS_REFERENCE;
        angle=r->angle_d72b4[value];
    }
    switch(octant){
        case 0:angle=256-angle;break;case 1:break;case 2:angle+=768;break;
        case 3:angle=1024-angle;break;case 4:angle+=256;break;case 5:angle=512-angle;break;
        case 6:angle=768-angle;break;default:angle+=512;break;
    }
    e.write[e.count++]=(uint16_t)angle;e.angle.word=(uint16_t)angle;e.angle.known=1;
    if(e.angle.word==1024){e.write[e.count++]=0;e.angle.word=0;}
    value=sar(y,2);
    if(signed32(y<<1)>=signed32(x))value=sar(y+sar(y,1),2);
    e.magnitude=value+x;memcpy(out,&e,sizeof(e));return NBA97_PHYSICS_OK;
}
static void store(Nba97GamePlayerPhysicsState* s,Nba97GamePhysicsReceipt* r,unsigned kind,unsigned field,Nba97GamePeriodValue v){
    Nba97GamePhysicsEvent* e=&r->event[r->count++];
    unsigned width=kind?nba97_game_physics_global_width(field):widths[field];
    if(width<4)v.word&=(1u<<(width*8))-1;
    e->value=v;e->kind=(uint8_t)kind;e->field=(uint8_t)field;
    if(kind)s->global[field]=v;else s->entity[field]=v;
}
static void assign(Nba97GamePlayerPhysicsState* s,Nba97GamePhysicsReceipt* r,unsigned kind,unsigned field,uint32_t word){
    Nba97GamePeriodValue v={word,1};store(s,r,kind,field,v);
}
#define SET(f,v) assign(s,receipt,0,NBA97_PHYSICS_##f,(uint32_t)(v))
static int call(Nba97GamePlayerPhysicsState* s,Nba97GamePhysicsReceipt* r,
                Nba97GamePhysicsCallback callback,void* context,unsigned owner,uint32_t site,uint32_t arg){
    Nba97GamePhysicsEvent* e=&r->event[r->count++];int result;
    e->kind=2;e->call.owner=(uint8_t)owner;e->call.callsite=site;e->call.argument=arg;e->call.has_argument=(uint8_t)(owner!=NBA97_PHYSICS_CALL_62660);
    if(!callback)return NBA97_PHYSICS_CALLBACK_PENDING;
    result=callback(context,s,&e->call);
    if(result!=1)return result==0?NBA97_PHYSICS_CALLBACK_PENDING:NBA97_PHYSICS_CALLBACK_FAILED;
    if(!valid_state(s))return NBA97_PHYSICS_ARGUMENT;
    e->completed=1;return NBA97_PHYSICS_OK;
}
static int run(Nba97GamePlayerPhysicsState* s,const Nba97GamePlayerPhysicsResources* resources,
               Nba97GamePhysicsCallback callback,void* context,Nba97GamePhysicsReceipt* receipt){
    uint32_t value,other,height,velocity,x,y,tick,direction=0,actor,scaled;int32_t timer;
    Nba97GameDirectionEffects movement;unsigned i;
    GLOBAL(FDBCC,value);ENTITY(00,other);
    if(other==(uint32_t)signed16(value)){
        GLOBAL(FE8E2,value);
        if(value){ENTITY(10,value);if(!value){ENTITY(18,value);if(!value){SET(16,0);SET(14,0);}}}
    }
    /* Copy source coordinates before gravity. This makes the later2C/10
     * comparison equal in owned storage: the source's intendedFF landing
     * marker is normally never written. Do not repair that source bug. */
    store(s,receipt,0,NBA97_PHYSICS_24,s->entity[NBA97_PHYSICS_08]);
    store(s,receipt,0,NBA97_PHYSICS_28,s->entity[NBA97_PHYSICS_0C]);
    store(s,receipt,0,NBA97_PHYSICS_2C,s->entity[NBA97_PHYSICS_10]);
    ENTITY(10,height);ENTITY(18,velocity);
    if(height||velocity){
        GLOBAL(FDB6C,tick);tick=(uint32_t)signed16(tick);
        /* Original SUBU/MULT/ADDU keep low32 bits before the signed height
         * test. Extreme raw ticks can wrap; do not saturate or widen them. */
        velocity=(uint32_t)signed16(velocity)-tick*24u;height+=velocity*tick;
        if(signed32(height)<0){
            ENTITY(1A,actor);
            velocity=actor==20&&signed32(velocity)<-192?sar(0u-velocity,2):0;
            height=0;ENTITY(2C,value);ENTITY(10,other);
            if(value!=other)SET(2C,255);
        }
        SET(18,velocity);SET(10,height);
    }
    ENTITY(14,velocity);GLOBAL(FDB6C,tick);ENTITY(08,x);
    x+=(uint32_t)signed16(velocity)*(uint32_t)signed16(tick);SET(08,x);
    if(signed32(x)>=0x17800){
        direction=7;if(signed32(x)>0x1a000){SET(08,0x1a000);if(signed16(velocity)>0)SET(14,0);}
    }else if(signed32(x)<=-0x17800){
        direction=3;if(signed32(x)<-0x1a000){SET(08,0xfffe6000u);if(signed16(velocity)<0)SET(14,0);}
    }
    ENTITY(16,velocity);GLOBAL(FDB6C,tick);ENTITY(0C,y);
    y+=(uint32_t)signed16(velocity)*(uint32_t)signed16(tick);SET(0C,y);
    if(signed32(y)>=0xc800||signed32(y)<=-0xc800){
        const unsigned table=signed32(y)>=0xc800?0:1;
        if(!resources||!resources->boundary[table]||direction>=resources->boundary_count[table])return NBA97_PHYSICS_REFERENCE;
        direction=(uint32_t)(int32_t)resources->boundary[table][direction];
        if(signed32(y)>0xf000){SET(0C,0xf000);if(signed16(velocity)>0)SET(16,0);}
        else if(signed32(y)<-0xf000){SET(0C,0xffff1000u);if(signed16(velocity)<0)SET(16,0);}
    }
    SET(C2,direction?(direction-1)*128u+1:0);
    if(!direction){
        ENTITY(0C,y);
        if(signed32(y)<=0x3800&&signed32(y)>=-0x3800){
            ENTITY(08,x);
            if(signed32(x)>=0xd800||signed32(x)<=-0xd800){
                ENTITY(1A,actor);TRY(read_value(s->team_direction10,&value));
                value=x^value;
                if(actor!=1){
                    /* Source uses signed XOR<=0, including exact equality. */
                    if(signed32(value)<=0){SET(EE,1);goto movement;}
                }else if(signed32(value)>=0){
                    GLOBAL(FE8E0,value);
                    if(value){
                        GLOBAL(FDB90,value);if(signed16(value)>=128){SET(EE,1);goto movement;}
                        {static const uint8_t blockers[]={NBA97_PHYSICS_FE8CC,NBA97_PHYSICS_FE8C4,NBA97_PHYSICS_FE8BC,NBA97_PHYSICS_FDBD4};
                         for(i=0;i<4;++i){TRY(read_value(s->global[blockers[i]],&value));if(value){SET(EE,1);goto movement;}}}
                        GLOBAL(FDB58,value);
                        if(value){
                            ENTITY(EE,value);GLOBAL(FDB6C,tick);timer=signed16(value)+signed16(tick);
                            if(timer>=300){
                                GLOBAL(21D8F,value);
                                if(value){
                                    GLOBAL(FDB94,value);
                                    if(!value){TRY(call(s,receipt,callback,context,NBA97_PHYSICS_CALL_29590,0x8006d3d0,11));value=5000;}
                                    else{TRY(call(s,receipt,callback,context,NBA97_PHYSICS_CALL_29590,0x8006d3e0,12));value=20000;}
                                    TRY(call(s,receipt,callback,context,NBA97_PHYSICS_CALL_295C8,0x8006d3ec,value));
                                    TRY(call(s,receipt,callback,context,NBA97_PHYSICS_CALL_62300,0x8006d3f4,9));
                                    TRY(call(s,receipt,callback,context,NBA97_PHYSICS_CALL_62660,0x8006d3fc,0));
                                    assign(s,receipt,1,NBA97_PHYSICS_FE882,2);
                                }
                                timer=300;
                            }
                            SET(EE,timer);goto movement;
                        }
                    }
                }
            }
        }
    }
    SET(EE,0);
movement:
    ENTITY(14,x);ENTITY(16,y);
    TRY(nba97_game_direction_speed(&movement,(uint32_t)signed16(x),(uint32_t)signed16(y),s->entity[NBA97_PHYSICS_A2],resources?&resources->direction:0));
    for(i=0;i<movement.count;++i)SET(A2,movement.write[i]);
    SET(A0,movement.magnitude);GLOBAL(FDB6C,tick);ENTITY(C8,value);
    /* Source truncates magnitude to16 bits, reinterprets it as signed, then
     * wraps BOTH products. Large raw velocities can yield negative movement. */
    scaled=(uint32_t)signed16(movement.magnitude&65535)*(uint32_t)signed16(tick);
    scaled*=value;SET(9C,sar(scaled,8));
    ENTITY(1A,actor);
    if(actor==12){GLOBAL(FE910,value);if((value&10)==2)SET(A0,0);} /*9C is not recomputed. */
    return NBA97_PHYSICS_OK;
}
int nba97_game_player_physics(Nba97GamePlayerPhysicsState* s,const Nba97GamePlayerPhysicsResources* resources,
                             Nba97GamePhysicsCallback callback,void* context,Nba97GamePhysicsReceipt* receipt){
    int result;
    if(!s||!receipt||!valid_state(s))return NBA97_PHYSICS_ARGUMENT;
    memset(receipt,0,sizeof(*receipt));result=run(s,resources,callback,context,receipt);
    if(result==NBA97_PHYSICS_OK)receipt->completed=1;
    return result;
}
