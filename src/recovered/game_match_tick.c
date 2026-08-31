#include "game_match_tick.h"
#include <string.h>

typedef struct Run {Nba97MatchTickContext* in;Nba97MatchTickProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_BODY_OK)return status_;}while(0)
static uint32_t sx16(uint32_t x){return x&0x8000u?(x&0xffffu)|0xffff0000u:x&0xffffu;}
static int32_t s32(uint32_t x){return x<0x80000000u?(int32_t)x:-1-(int32_t)~x;}
static uint32_t mask(unsigned n){return n==4?UINT32_MAX:(1u<<(8*n))-1u;}
static int reserve(Run* r,uint32_t pc,uint32_t address,uint32_t entry){
    r->out->stopped_pc=pc;r->out->stopped_address=address;r->out->stopped_entry=entry;
    if(r->out->operations>=r->in->operation_budget)return NBA97_BODY_JOURNAL_LIMIT;
    ++r->out->operations;return NBA97_BODY_OK;
}
static int access(Run* r,uint32_t pc,uint32_t address,unsigned width,unsigned kind,Nba97PlayerFrameValue* v){
    unsigned i;int status;TRY(reserve(r,pc,address,0));
    if((width==4&&(address&3))||(width==2&&(address&1)))return NBA97_BODY_ALIGNMENT_TRAP;
    status=r->in->access(r->in->user,pc,address,width,kind,v);if(status!=NBA97_BODY_OK)return status;
    if(v->is_reference>1||v->reference.known>1||(!v->reference.known&&(v->reference.allocation||v->reference.offset)))return NBA97_BODY_ARGUMENT;
    if(!v->is_reference&&v->reference.known)return NBA97_BODY_ARGUMENT;
    if(v->is_reference&&(width!=4||(!v->reference.known&&(v->known_mask||v->word))))return NBA97_BODY_ARGUMENT;
    if(v->known_mask&~((1u<<width)-1u)||v->word&~mask(width))return NBA97_BODY_ARGUMENT;
    for(i=0;i<width;++i)if(!(v->known_mask&(1u<<i))&&(v->word&(255u<<(8*i))))return NBA97_BODY_ARGUMENT;
    if(kind==NBA97_FRAME_READ)++r->out->reads;else ++r->out->stores;
    return NBA97_BODY_OK;
}
static int rd(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t* out){
    Nba97PlayerFrameValue v={0};TRY(access(r,pc,address,width,NBA97_FRAME_READ,&v));
    if(v.known_mask!=((1u<<width)-1u))return NBA97_BODY_UNKNOWN;
    *out=v.word;return NBA97_BODY_OK;
}
static int wr(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t word,unsigned kind){
    Nba97PlayerFrameValue v={0};v.word=word&mask(width);v.known_mask=(uint8_t)((1u<<width)-1u);
    return access(r,pc,address,width,kind,&v);
}
static int call(Run* r,uint32_t pc,uint32_t entry,uint32_t a0,uint32_t a1,unsigned count,uint32_t* result){
    Nba97MatchTickCall q;Nba97GamePeriodValue v={0,0};int status;
    TRY(reserve(r,pc,0,entry));if(!r->in->service)return NBA97_MATCH_TICK_SERVICE_REQUIRED;
    q.pc=pc;q.entry=entry;q.args[0]=a0;q.args[1]=a1;q.count=count;
    status=r->in->service(r->in->user,&q,result?&v:0);if(status!=NBA97_BODY_OK)return status;
    ++r->out->services;if(result){if(v.known>1||(!v.known&&v.word))return NBA97_BODY_ARGUMENT;
        if(!v.known)return NBA97_BODY_UNKNOWN;
        *result=v.word;}
    return NBA97_BODY_OK;
}
static int player(Run* r){int status;TRY(reserve(r,0x80068d84,0,0x8006801c));
    if(!r->in->player_update)return NBA97_MATCH_TICK_PLAYER_UPDATE_REQUIRED;
    status=r->in->player_update(r->in->user,0x80068d84);if(status!=NBA97_BODY_OK)return status;
    ++r->out->player_updates;return NBA97_BODY_OK;}
static int ball(Run* r,uint32_t pointer){int status;TRY(reserve(r,0x80068d9c,0,0x8006ef60));
    if(!r->in->ball_simulation)return NBA97_MATCH_TICK_BALL_SIMULATION_REQUIRED;
    status=r->in->ball_simulation(r->in->user,0x80068d9c,pointer);if(status!=NBA97_BODY_OK)return status;
    ++r->out->ball_ticks;return NBA97_BODY_OK;}
static int net(Run* r){int status;TRY(reserve(r,0x8002dda4,0,0x8002dc88));
    if(!r->in->net_transform)return NBA97_MATCH_TICK_NET_TRANSFORM_REQUIRED;
    status=r->in->net_transform(r->in->user,0x8002dda4);if(status!=NBA97_BODY_OK)return status;
    ++r->out->net_transforms;return NBA97_BODY_OK;}
static int frame(Run* r){int status;TRY(reserve(r,0x8002ddb4,0,0x80049018));
    if(!r->in->match_frame)return NBA97_MATCH_TICK_MATCH_FRAME_REQUIRED;
    status=r->in->match_frame(r->in->user,0x8002ddb4);if(status!=NBA97_BODY_OK)return status;
    ++r->out->frame_pumps;return NBA97_BODY_OK;}
static int pump_2dd84(Run* r){uint32_t delta;
    TRY(call(r,0x8002dd8c,0x8007e26c,0,0,1,0));
    TRY(rd(r,0x8002dd98,0x800fdb6c,2,&delta));
    TRY(call(r,0x8002dd9c,0x800798b4,sx16(delta),0,1,0));
    TRY(net(r));TRY(call(r,0x8002ddac,0x80032b10,0,0,0,0));TRY(frame(r));return NBA97_BODY_OK;
}
static int timing(Run* r,uint32_t* s6,uint32_t* s6_known,uint32_t* fp){
    uint32_t v,s0=0; /* s4 is reset at 68F64 on every prior pass. */
    TRY(rd(r,0x80068ec8,0x800fdb92,2,&v));
    if(s32(sx16(v))<2){
        TRY(call(r,0x80068edc,0x800a584c,0,0,0,&s0));s0&=0xffffu;
        if(s32(sx16(s0))==0)s0=4;
        while(s32(sx16(s0))<4){TRY(call(r,0x80068f00,0x800a584c,0,0,0,&v));s0=(s0+v)&0xffffu;}
        if(s0&1u){uint32_t next=(*fp+1u)&0xffffu;*fp=next;if(s32(sx16(next))>=2){s0=(s0+2u)&0xffffu;*fp=(next-2u)&0xffffu;}}
        s0=sx16(s0)>>1;*s6=s0&0xffffu;*s6_known=1;
    }
    if(s32(sx16(s0))>=3){TRY(wr(r,0x80068f70,0x800fdb92,2,2,NBA97_FRAME_WRITE));}
    else {TRY(rd(r,0x80068f7c,0x800fdb92,2,&v));TRY(wr(r,0x80068f88,0x800fdb92,2,v+s0,NBA97_FRAME_WRITE));}
    TRY(rd(r,0x80068f90,0x800fdbde,2,&v));
    /* The compiler carries caller s6 into this store when the random-timing
     * block was skipped. Do not manufacture zero for that unknown register. */
    if(!*s6_known){r->out->stopped_pc=0x80068f98;r->out->stopped_address=0x800fdb6c;r->out->stopped_entry=0;return NBA97_BODY_UNKNOWN;}
    TRY(wr(r,0x80068f98,0x800fdb6c,2,*s6,NBA97_FRAME_WRITE));
    if(v){uint32_t next=(v-*s6)&0xffffu;TRY(wr(r,0x80068fac,0x800fdbde,2,next,NBA97_FRAME_WRITE));
        if(s32(sx16(next))<0)TRY(wr(r,0x80068fc0,0x800fdbde,2,0,NBA97_FRAME_WRITE));}
    return NBA97_BODY_OK;
}
static int simulation(Run* r,uint32_t* ended,uint32_t* s6,uint32_t* s6_known,uint32_t* fp){
    uint32_t v,v2=0,arg;
    *ended=0;
    TRY(rd(r,0x80068c8c,0x800fdb92,2,&v));
    if(s32(sx16(v))<2){
        TRY(timing(r,s6,s6_known,fp));return NBA97_BODY_OK;
    }
    TRY(rd(r,0x80068ca0,0x800fdb8a,2,&v));
    TRY(wr(r,0x80068cac,0x800fdc32,2,0,NBA97_FRAME_WRITE));
    if(v){TRY(rd(r,0x80068cb4,0x80021d82,1,&v2));if(v2){TRY(wr(r,0x80068cc8,0x800fdb6c,2,1,NBA97_FRAME_WRITE));}}
    if(!v||!v2)TRY(wr(r,0x80068cd4,0x800fdb6c,2,2,NBA97_FRAME_WRITE));
    TRY(rd(r,0x80068cd8,0x800fdb92,2,&v));TRY(rd(r,0x80068cdc,0x800fdb6c,2,&v2));
    TRY(wr(r,0x80068ce8,0x800fdb92,2,v-2u,NBA97_FRAME_WRITE));
    TRY(wr(r,0x80068cf0,0x800fdb6e,2,v2<<4,NBA97_FRAME_WRITE));
    TRY(call(r,0x80068cec,0x80067550,0,0,0,0));TRY(call(r,0x80068cf4,0x800675e4,0,0,0,0));
    TRY(rd(r,0x80068cfc,0x800fdb7c,2,&v));
    if(!v){
        TRY(rd(r,0x80068d0c,0x800fe8cc,2,&v));if(!v){TRY(rd(r,0x80068d1c,0x800fe8c4,2,&v));if(!v){
            TRY(rd(r,0x80068d2c,0x800fdb8a,2,&v));
            uint32_t call_pc;
            if(v){TRY(rd(r,0x80068d3c,0x800fdb6c,2,&arg));arg=sx16(arg);call_pc=0x80068d40;}else{
                if(!*s6_known){r->out->stopped_pc=0x80068d38;r->out->stopped_address=r->out->stopped_entry=0;return NBA97_BODY_UNKNOWN;}
                arg=sx16(*s6);call_pc=0x80068d58;
            }
            TRY(call(r,call_pc,0x80067a60,arg,0,1,0));
            /* The nonzero FDB8A arm reloads FDB6C after 67A60; that service is
             * allowed to mutate the live halfword before 67D38 consumes it. */
            if(v){TRY(rd(r,0x80068d48,0x800fdb6c,2,&arg));arg=sx16(arg);}
            TRY(call(r,0x80068d64,0x80067d38,arg,0,1,0));
            TRY(call(r,0x80068d6c,0x80067664,0,0,0,&v));if(v){*ended=1;return NBA97_BODY_OK;}
        }}
        TRY(call(r,0x80068d7c,0x8002de34,0,0,0,0));TRY(player(r));
        TRY(rd(r,0x80068d90,0x800fdc48,4,&arg));TRY(wr(r,0x80068d98,0x800fdc3c,4,arg,NBA97_FRAME_WRITE_POINTER));TRY(ball(r,arg));
    }else{
        TRY(call(r,0x80068dac,0x8007a668,0,0,0,&v));
        if(!(v&0xffu)){TRY(rd(r,0x80068dc0,0x800fdb7c,2,&v));TRY(rd(r,0x80068dc4,0x800fdb6c,2,&v2));v=(v-v2)&0xffffu;
            TRY(wr(r,0x80068dd0,0x800fdb7c,2,v,NBA97_FRAME_WRITE));if(s32(sx16(v))<0)TRY(wr(r,0x80068de4,0x800fdb7c,2,0,NBA97_FRAME_WRITE));}
        else {TRY(call(r,0x80068de8,0x8007001c,0,0,0,&v));if(v)TRY(call(r,0x80068df8,0x8007a680,1,0,1,0));}
    }
    TRY(wr(r,0x80068e04,0x800fdb88,2,0,NBA97_FRAME_WRITE));TRY(call(r,0x80068e00,0x80060ef8,0,0,0,0));
    TRY(call(r,0x80068e08,0x80060fbc,0,0,0,0));TRY(rd(r,0x80068e10,0x800fdb88,2,&v));if(v)TRY(call(r,0x80068e20,0x80060ef8,0,0,0,0));
    TRY(call(r,0x80068e28,0x800747b0,0,0,0,0));TRY(call(r,0x80068e30,0x8006817c,0,0,0,0));TRY(call(r,0x80068e38,0x8006830c,0,0,0,0));
    TRY(rd(r,0x80068e40,0x800fdbae,2,&v));TRY(rd(r,0x80068e44,0x800fdb6c,2,&v2));v=(v-v2)&0xffffu;
    TRY(wr(r,0x80068e58,0x800fdbae,2,v,NBA97_FRAME_WRITE));if(s32(sx16(v))<0){v=(v+60u)&0xffffu;TRY(wr(r,0x80068e64,0x800fdbae,2,v,NBA97_FRAME_WRITE));TRY(call(r,0x80068e60,0x80068504,0,0,0,0));}
    TRY(rd(r,0x80068e68,0x800fdb7c,2,&v));if(!v)TRY(call(r,0x80068e78,0x80076b28,0,0,0,0));
    TRY(rd(r,0x80068e80,0x800fe8c4,2,&v));TRY(wr(r,0x80068e90,0x800fe8c4,2,v&0xfffeu,NBA97_FRAME_WRITE));
    TRY(call(r,0x80068e8c,0x800686b8,0,0,0,0));TRY(call(r,0x80068e94,0x80062bfc,0,0,0,0));TRY(call(r,0x80068e9c,0x80066e84,0,0,0,0));
    TRY(wr(r,0x80068ea8,0x800fe8a8,2,0,NBA97_FRAME_WRITE));TRY(call(r,0x80068ea4,0x80057b18,0,0,0,0));
    TRY(rd(r,0x80068eac,0x800fdb9c,2,&v));++r->out->simulation_steps;
    if(s32(sx16(v))<0){TRY(timing(r,s6,s6_known,fp));*s6_known=1;}
    return NBA97_BODY_OK;
}
static int post_frame(Run* r,uint32_t* ended){uint32_t v,v2,index,input;
    TRY(pump_2dd84(r));TRY(rd(r,0x80068fd0,0x800fdb7c,2,&v));if(!v)TRY(call(r,0x80068fe0,0x80076b3c,0,0,0,0));
    TRY(rd(r,0x80068fec,0x800fa038,2,&v));if(v)TRY(call(r,0x80068ffc,0x800786c0,0,0,0,0));
    TRY(rd(r,0x80069004,0x8001edec,2,&v));
    if(!v){TRY(rd(r,0x80069014,0x8001ee36,2,&v));TRY(rd(r,0x80069018,0x8001eefa,2,&v2));if(((v+v2)&0xffffu)!=0)goto after_input;}
    for(index=0;index<8;++index){
        TRY(call(r,0x8006902c,0x8008f224,index,0,1,&input));
        if((input&0xffffu)!=0){TRY(rd(r,0x80069044,0x8001edec,2,&v));if(v){TRY(wr(r,0x80069058,0x800fdb78,1,1,NBA97_FRAME_WRITE));TRY(wr(r,0x80069064,0x8001edec,2,99,NBA97_FRAME_WRITE));continue;}}
        if(input&0x100u)TRY(wr(r,0x80069078,0x800fdb9c,2,index,NBA97_FRAME_WRITE));
        else if(input&0x80u)TRY(wr(r,0x80069090,0x800fdb9c,2,index+128u,NBA97_FRAME_WRITE));
    }
after_input:
    TRY(rd(r,0x800690a8,0x800fdb9c,2,&v));if(s32(sx16(v))>=0){TRY(call(r,0x800690b8,0x8006720c,0,0,0,0));TRY(wr(r,0x800690c8,0x800fdb9c,2,0xffff,NBA97_FRAME_WRITE));TRY(call(r,0x800690cc,0x800a584c,0,0,0,0));}
    TRY(rd(r,0x800690d8,0x800fdb90,2,&v));if(v==128)TRY(call(r,0x800690e8,0x800664a0,0,0,0,0));
    TRY(rd(r,0x800690f4,0x800fdb78,1,&v));*ended=v!=0;return NBA97_BODY_OK;
}
static int run_match(Run* r){uint32_t ended=0,exit_flag=0,s6=r->in->incoming_s6.word&0xffffu,s6_known=r->in->incoming_s6.known,fp=0,v,v2,saved;
    if(s6_known>1||(!s6_known&&r->in->incoming_s6.word))return NBA97_BODY_ARGUMENT;
restart:
    exit_flag=0;++r->out->outer_restarts;TRY(call(r,0x80068c24,0x80066f88,0,0,0,0));TRY(call(r,0x80068c2c,0x80079664,0,0,1,0));
period:
    ended=0;TRY(call(r,0x80068c4c,0x80067468,0,0,0,0));TRY(rd(r,0x80068c58,0x8001edec,2,&v));fp=0;
    if(v==99){TRY(wr(r,0x80068c80,0x800fdb78,1,1,NBA97_FRAME_WRITE));ended=1;exit_flag=1;}
    while(!ended){TRY(simulation(r,&ended,&s6,&s6_known,&fp));if(ended)break;
        TRY(post_frame(r,&ended));if(ended)exit_flag=1;}
    TRY(rd(r,0x80069120,0x800fdb78,1,&v));if(!v)TRY(call(r,0x80069130,0x80067814,0,0,0,0));
    TRY(rd(r,0x8006913c,0x800fdb68,2,&v));if(v==5)exit_flag=1;
    if(!exit_flag)goto period;
    TRY(rd(r,0x80069168,0x8001edec,2,&v));if(v==98){TRY(rd(r,0x80069178,0x800fdb78,1,&v2));if(!v2)TRY(call(r,0x80069188,0x80083100,0,0,0,0));
        TRY(rd(r,0x80069194,0x800fe91c,4,&saved));TRY(call(r,0x8006919c,0x800a3a74,0x800fdb4c,0xe7c,2,0));TRY(wr(r,0x800691a8,0x800fe91c,4,saved,NBA97_FRAME_WRITE_POINTER));
        TRY(call(r,0x800691ac,0x800659f0,0,0,0,0));TRY(wr(r,0x800691b8,0x8001edec,2,0,NBA97_FRAME_WRITE));goto restart;}
    TRY(call(r,0x800691bc,0x80067930,0,0,0,0));return NBA97_BODY_OK;
}
int nba97_game_match_tick(Nba97MatchTickContext* c,Nba97MatchTickProgress* p){Run r;int status;
    if(!p)return NBA97_BODY_ARGUMENT;
    memset(p,0,sizeof *p);if(!c||!c->access)return NBA97_BODY_ARGUMENT;
    r.in=c;r.out=p;status=run_match(&r);if(status==NBA97_BODY_OK){p->completed=1;p->stopped_pc=p->stopped_address=p->stopped_entry=0;}return status;
}
