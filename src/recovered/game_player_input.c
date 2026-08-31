#include "game_player_input.h"
#include <string.h>
static const uint8_t offsets[]={0,4,16,20,22,24,26,96,100,164,186,188,190,192,196,216,217,228};
static const uint8_t widths[]={4,2,4,2,2,2,1,2,2,2,2,2,2,2,2,1,1,2};
static const uint32_t globals[]={0xfdb9c,0xfe918,0xfdbcc,0xfdb94,0xfe8e2,0xfe8cc,0xfdb90,0xfdb7c,0xfe880,0xfdbd4,0xfdbd2,0xd8eec,0xfc99c,0xfa378};
static const uint8_t control_offsets[]={0x26,0x2a,0x2e,0x30,0x32,0x34,0x38,0x3c};
unsigned nba97_game_input_entity_offset(unsigned f){return f<NBA97_INPUT_ENTITY_COUNT?offsets[f]:0;}
unsigned nba97_game_input_entity_width(unsigned f){return f<NBA97_INPUT_ENTITY_COUNT?widths[f]:0;}
unsigned nba97_game_input_global_address(unsigned f){return f<NBA97_INPUT_GLOBAL_COUNT?0x80000000u+globals[f]:0;}
unsigned nba97_game_input_global_width(unsigned f){return f<11?2:f<13?4:f==13?1:0;}
unsigned nba97_game_input_controller_offset(unsigned f){return f<NBA97_INPUT_CONTROL_COUNT?control_offsets[f]:0;}
unsigned nba97_game_input_controller_width(unsigned f){return f<6?2:f==6?4:f==7?1:0;}
static int32_t s16(uint32_t x){return x<32768u?(int32_t)x:(int32_t)x-65536;}
static uint32_t low(uint32_t x,unsigned width){return width==4?x:x&((1u<<(width*8))-1u);}
static int valid_value(Nba97GamePeriodValue v,unsigned width){return v.known<=1&&(v.known||!v.word)&&v.word==low(v.word,width);}
static int valid_ref(Nba97GamePeriodReference r){return r.known<=1&&(r.known||!r.record);}
static int valid_state(const Nba97GamePlayerInputState* s){
    unsigned i,f;if(!s||s->player_count>24)return 0;
    for(i=0;i<11;++i){
        for(f=0;f<NBA97_INPUT_ENTITY_COUNT;++f)if(!valid_value(s->entity[i][f],widths[f]))return 0;
        if(!valid_value(s->player_reference[i],4)||!valid_ref(s->entity_table[i]))return 0;
    }
    for(f=0;f<NBA97_INPUT_GLOBAL_COUNT;++f)if(!valid_value(s->global[f],nba97_game_input_global_width(f)))return 0;
    for(i=0;i<8;++i)for(f=0;f<NBA97_INPUT_CONTROL_COUNT;++f)
        if(!valid_value(s->controller[i][f],nba97_game_input_controller_width(f)))return 0;
    for(i=0;i<24;++i)if(!valid_value(s->player1d[i],1))return 0;
    return valid_ref(s->reference_fdc34)&&valid_ref(s->team_fdc40);
}
static int read_value(Nba97GamePeriodValue v,uint32_t* out){if(!v.known)return NBA97_INPUT_UNRESOLVED;*out=v.word;return NBA97_INPUT_OK;}
#define TRY(op) do{int result_=(op);if(result_!=NBA97_INPUT_OK)return result_;}while(0)
#define E(f,v) TRY(read_value(s->entity[entity][NBA97_INPUT_##f],&(v)))
#define G(f,v) TRY(read_value(s->global[NBA97_INPUT_##f],&(v)))
static int reference(Nba97GamePeriodReference ref,unsigned* out){
    if(!ref.known)return NBA97_INPUT_UNRESOLVED;
    if(ref.record>=11)return NBA97_INPUT_REFERENCE;*out=ref.record;return NBA97_INPUT_OK;
}
static int table(Nba97GamePlayerInputState* s,uint32_t slot,unsigned* entity){
    /* Original signed positive indices are unchecked. Eleven is owned storage,
     * not a repaired original bound or a modulo-player mapping. */
    if(slot>=11)return NBA97_INPUT_REFERENCE;return reference(s->entity_table[slot],entity);
}
static void store(Nba97GamePlayerInputState* s,Nba97GameInputReceipt* r,unsigned kind,
                  unsigned record,unsigned field,Nba97GamePeriodValue v){
    Nba97GameInputEvent* e=&r->event[r->count++];e->kind=(uint8_t)kind;
    e->record=(uint8_t)record;e->field=(uint8_t)field;e->value=v;
    if(kind==0)s->entity[record][field]=v;else if(kind==1)s->global[field]=v;else s->controller[record][field]=v;
}
static void assign(Nba97GamePlayerInputState* s,Nba97GameInputReceipt* r,unsigned kind,
                   unsigned record,unsigned field,uint32_t word){
    unsigned width=kind==0?widths[field]:kind==1?nba97_game_input_global_width(field):nba97_game_input_controller_width(field);
    Nba97GamePeriodValue v={0,1};v.word=low(word,width);store(s,r,kind,record,field,v);
}
static int call(Nba97GamePlayerInputState* s,Nba97GameInputReceipt* r,Nba97GameInputCallback callback,
                void* context,unsigned owner,uint32_t pc,unsigned entity,unsigned count,
                uint32_t a1,uint32_t a2,uint32_t* result){
    Nba97GameInputEvent* event=&r->event[r->count++];Nba97GamePeriodValue returned={0,0};int code;
    event->kind=3;event->call.owner=(uint8_t)owner;event->call.callsite=pc;event->call.entity=(uint8_t)entity;
    event->call.argument_count=(uint8_t)count;event->call.argument_known=(uint8_t)((1u<<count)-1u);
    event->call.argument[0]=a1;event->call.argument[1]=a2;r->stopped_pc=pc;
    if(!callback)return NBA97_INPUT_PENDING;
    code=callback(context,s,&event->call,&returned);
    if(code!=1)return code==0?NBA97_INPUT_PENDING:NBA97_INPUT_CALLBACK_FAILED;
    if(!valid_state(s)||!valid_value(returned,4))return NBA97_INPUT_ARGUMENT;
    event->completed=1;event->return_v0=returned;
    if(result)TRY(read_value(returned,result));r->stopped_pc=0;return NBA97_INPUT_OK;
}
#define CALL(owner,pc,e,n,a,b,out) TRY(call(s,r,callback,context,NBA97_INPUT_CALL_##owner,pc,e,n,a,b,out))
static int complete(Nba97GameInputReceipt* r){r->completed=1;r->stopped_pc=0;return NBA97_INPUT_OK;}
int nba97_game_player_input(Nba97GamePlayerInputState* s,unsigned entity,
    Nba97GamePeriodReference controller,uint32_t mask,uint32_t mapped,
    Nba97GameInputCallback callback,void* context,Nba97GameInputReceipt* r){
    uint32_t a,b,c,actor,possessor,side,result;unsigned other;
    if(!r||entity>=11||!valid_state(s)||!valid_ref(controller))return NBA97_INPUT_ARGUMENT;
    memset(r,0,sizeof *r);r->captured_team=s->team_fdc40;r->controller_argument=controller;
    r->logical_mask=mask;r->mapped_mask=mapped;
    if(mask&0x100u)store(s,r,1,0,NBA97_INPUT_FDB9C,s->entity[entity][NBA97_INPUT_04]);
    if(mask&0x80u){E(04,a);assign(s,r,1,0,NBA97_INPUT_FDB9C,a+128u);}
    if((mapped&0x3000u)&&mask){r->play_call_pending=1;r->stopped_pc=0x800617d0;return NBA97_INPUT_PLAY_CALL_PENDING;}
    if(mask&0x200u){
        G(FE918,a);if(!a){G(FDBCC,a);E(00,b);if(b==(uint32_t)s16(a))CALL(6CD50,0x80061ce8,entity,0,0,0,0);}
        /* Source rereads side/claim after6CD50; no cached pre-call claim. */
        E(D9,a);G(FDB94,b);if(a!=(uint32_t)s16(b)){CALL(612E4,0x80061d14,entity,1,1,0,0);return complete(r);}
    }
    if(mask&0x30u){
        G(FDBCC,a);E(00,b);
        if(b==(uint32_t)s16(a)){
            E(BA,a);if(a>=10){
                G(FE8E2,a);if(s16(a)<=0){
                    G(FE8CC,a);if(!a){
                        TRY(read_value(s->player_reference[entity],&a));if(a>=s->player_count)return NBA97_INPUT_REFERENCE;
                        TRY(read_value(s->player1d[a],&a));if(a>=75){
                            G(FDB90,a);if(s16(a)<128){
                                CALL(5BDD8,0x80061dc4,entity,1,(mask&0x20u)?1u:0xffffffffu,0,&result);
                                if(result){assign(s,r,0,entity,NBA97_INPUT_D8,1);return complete(r);}
                            }
                        }
                    }
                }
            }
        }
    }
    if(mask&0x800u){
        G(FDBCC,a);E(00,b);
        if(b!=(uint32_t)s16(a)){CALL(612E4,0x80061e3c,entity,1,0,0,0);return complete(r);}
        E(1A,a);if(a==17||a==20)return complete(r);
        G(FE8CC,a);if(a)return complete(r);
        CALL(610FC,0x80061e2c,entity,0,0,0,0);return complete(r);
    }
    G(FE8CC,a);if(a)return complete(r);G(FDB7C,a);if(a)return complete(r);
    E(1A,actor);if(actor==20)return complete(r);
    if(mask&0x20u){
        G(FDBCC,a);if(s16(a)>=0){
            TRY(table(s,a,&other));E(D9,b);TRY(read_value(s->entity[other][NBA97_INPUT_D9],&c));
            if(b==c){TRY(read_value(s->entity[other][NBA97_INPUT_04],&a));
                if(s16(a)<0&&actor<7){CALL(5B258,0x80061edc,other,1,entity,0,0);return complete(r);}
            }
        }
    }
    if(mask&0x10u){
        G(FDBCC,a);if(s16(a)>=0){
            TRY(table(s,a,&other));E(D9,b);TRY(read_value(s->entity[other][NBA97_INPUT_D9],&c));
            if(b==c){TRY(read_value(s->entity[other][NBA97_INPUT_04],&a));if(s16(a)<0){
                TRY(read_value(s->entity[other][NBA97_INPUT_1A],&a));
                if(a==11){CALL(5C008,0x80061f4c,other,0,0,0,0);return complete(r);}
            }}
        }
    }
    E(10,a);if(a)return complete(r);E(18,a);if(a)return complete(r);
    E(60,a);if(a&3u)return complete(r);E(64,a);if(a&3u)return complete(r);
    G(FDB90,a);if(a==130){
        E(D9,a);G(FE880,b);if(a!=(uint32_t)s16(b))return complete(r);
        G(FDBCC,a);E(00,b);if(b==(uint32_t)s16(a))return complete(r);
    }
    G(FDB94,a);E(D9,b);
    if((uint32_t)s16(a)!=b){
        G(FDBCC,a);if(s16(a)>=0){G(FDBD4,a);if(!a){
            if(mask&0x10u){CALL(5699C,0x80062030,entity,1,43,0,0);
                store(s,r,0,entity,NBA97_INPUT_A4,s->entity[entity][NBA97_INPUT_C0]);}
            if(mask&0x40u)CALL(5699C,0x80062050,entity,1,42,0,0);
        }}
    }
    G(FDBCC,possessor);E(00,a);
    if(a==(uint32_t)s16(possessor)){
        if(!(mask&0x40u))return complete(r);E(1A,a);if(a==13||a==14)return complete(r);
        CALL(5ADB8,0x80062094,entity,0,0,0,&result);if(!(result&255u))return complete(r);
        CALL(5C008,0x800620a8,entity,0,0,0,0);return complete(r);
    }
    if(!(mask&0x20u))return complete(r);
    E(BE,a);if(a<41){G(FDBD2,a);if(s16(a)<0){
        G(FDB94,side);E(D9,a);
        /* Original A0 possessor value is captured at6205C, rather than reread
         * here. No call intervenes on this path. Signed16 claim != byte side. */
        if((uint32_t)s16(side)!=a||s16(possessor)<0){
            G(FDBD4,a);
            if(!a){CALL(6A2E4,0x80062158,entity,1,0,0,&result);if(result==1)return complete(r);}
            else{
                TRY(reference(s->reference_fdc34,&other));E(BC,a);TRY(read_value(s->entity[other][NBA97_INPUT_BC],&b));
                if((((uint32_t)s16(a)-(uint32_t)s16(b)-320u)&1023u)<385u){
                    CALL(6A144,0x80062148,entity,0,0,0,0);return complete(r);
                }
            }
        }
    }}
    /* Original fallback differs from6A2E4: phase81 keeps EXISTING velocities.
     * A rejected actual jump may already have consumed shared RNG; callbacks
     * must preserve it rather than rolling back an originalv0==0 return. */
    assign(s,r,0,entity,NBA97_INPUT_C4,600);G(FDB90,a);
    if(a==129){
        CALL(56B78,0x8006218c,entity,1,77,0,0);CALL(56CE0,0x8006219c,entity,2,78,0,0);
        CALL(56CE0,0x800621f0,entity,2,79,0,0);
    }else{
        E(14,a);E(16,b);
        /* Arithmetic shift floors negative odd signed velocities. */
        assign(s,r,0,entity,NBA97_INPUT_14,(uint32_t)(s16(a)>=0?s16(a)/4:-1-((-1-s16(a))/4)));
        assign(s,r,0,entity,NBA97_INPUT_16,(uint32_t)(s16(b)>=0?s16(b)/4:-1-((-1-s16(b))/4)));
        CALL(56B78,0x800621d0,entity,1,68,0,0);CALL(56CE0,0x800621e0,entity,2,69,0,0);
        CALL(56CE0,0x800621f0,entity,2,70,0,0);
    }
    return complete(r);
}
int nba97_game_input_direction(Nba97GamePeriodValue* out,const Nba97GamePlayerInputState* s,
                              uint32_t direction,uint32_t mode){
    uint32_t value,camera,flip;Nba97GamePeriodValue result={0,1};
    if(!out||!valid_state(s))return NBA97_INPUT_ARGUMENT;
    if((direction&65535u)==8){result.word=8;*out=result;return NBA97_INPUT_OK;}
    if(mode&255u){G(D8EEC,camera);value=direction+camera;}
    else{
        G(FC99C,camera);
        if(camera==1||camera==4||camera==5){
            G(FA378,flip);value=direction+(camera==5?10u:4u);if(flip)value+=4;
        }else if(camera==2){G(D8EEC,camera);G(FA378,flip);value=direction+camera+1u;if(flip)value+=6;}
        else{G(D8EEC,camera);value=direction+camera;}
    }
    result.word=value&7u;*out=result;return NBA97_INPUT_OK;
}
int nba97_game_input_edge(Nba97GamePlayerInputState* s,unsigned controller,uint32_t mapped,Nba97GameInputReceipt* r){
    Nba97GamePeriodValue previous,mode,returned;uint32_t changed,edge,direction,raw;unsigned entity;
    Nba97GameInputEvent* event;
    if(!r||controller>=8||!valid_state(s))return NBA97_INPUT_ARGUMENT;
    memset(r,0,sizeof *r);r->mapped_mask=mapped;previous=s->controller[controller][NBA97_INPUT_CONTROL_30];
    assign(s,r,2,controller,NBA97_INPUT_CONTROL_2E,mapped);assign(s,r,2,controller,NBA97_INPUT_CONTROL_30,mapped);
    TRY(read_value(previous,&raw));changed=(uint32_t)s16(raw)^mapped;
    assign(s,r,2,controller,NBA97_INPUT_CONTROL_32,changed);edge=changed&mapped;
    assign(s,r,2,controller,NBA97_INPUT_CONTROL_34,edge);
    /* Literal direction priority for contradictory logical directions:
     * bit8 beats bit4, bit1 beats bit2; do not normalize host button input. */
    if(mapped&8u)direction=(mapped&1u)?5u:(mapped&2u)?7u:6u;
    else if(mapped&4u)direction=(mapped&1u)?3u:(mapped&2u)?1u:2u;
    else direction=(mapped&1u)?4u:(mapped&2u)?0u:8u;
    mode=s->controller[controller][NBA97_INPUT_CONTROL_3C];
    assign(s,r,2,controller,NBA97_INPUT_CONTROL_38,direction);
    event=&r->event[r->count++];event->kind=3;event->call.owner=NBA97_INPUT_CALL_7A498;
    event->call.controller=(uint8_t)controller;event->call.argument_count=2;event->call.callsite=0x8007018c;
    event->call.argument[0]=direction;event->call.argument_known=1;r->stopped_pc=0x8007018c;
    /* Helper's neutral8 branch does not inspect its potentially unknown a1. */
    if(direction!=8)TRY(read_value(mode,&raw));else raw=mode.word;
    event->call.argument[1]=raw;event->call.argument_known=(uint8_t)(1u+(mode.known?2u:0u));
    TRY(nba97_game_input_direction(&returned,s,direction,raw));
    event->return_v0=returned;event->completed=1;r->stopped_pc=0;
    assign(s,r,2,controller,NBA97_INPUT_CONTROL_2A,returned.word);
    assign(s,r,2,controller,NBA97_INPUT_CONTROL_2A,returned.word==8?1024u:returned.word<<7);
    if(mapped&0x400u){
        TRY(read_value(s->controller[controller][NBA97_INPUT_CONTROL_26],&raw));
        if(s16(raw)>=0){TRY(table(s,raw,&entity));assign(s,r,0,entity,NBA97_INPUT_E4,10);}
    }
    r->edge_mask.word=edge;r->edge_mask.known=1;return complete(r);
}
