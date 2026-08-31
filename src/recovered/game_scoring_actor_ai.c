#include "game_scoring_actor_ai.h"
#include <string.h>

typedef struct Run {Nba97ScoringActorAiContext* in;Nba97ScoringActorAiProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_BODY_OK)return status_;}while(0)

static int32_t s32(uint32_t v){return v<0x80000000u?(int32_t)v:-1-(int32_t)~v;}
static uint32_t sx16(uint32_t v){return v&0x8000u?(v&0xffffu)|0xffff0000u:v&0xffffu;}
static uint32_t mask(unsigned n){return n==4?UINT32_MAX:(1u<<(8u*n))-1u;}
static unsigned knowledge(unsigned n){return (1u<<n)-1u;}

static int reserve(Run* r,uint32_t pc,uint32_t address,uint32_t entry){
    r->out->stopped_pc=pc;r->out->stopped_address=address;r->out->stopped_entry=entry;
    if(r->out->operations>=r->in->operation_budget)return NBA97_BODY_JOURNAL_LIMIT;
    ++r->out->operations;return NBA97_BODY_OK;
}
static int access(Run* r,uint32_t pc,uint32_t address,unsigned width,unsigned kind,
                  Nba97PlayerFrameValue* value){
    unsigned i;int status;TRY(reserve(r,pc,address,0));
    if((width==4&&(address&3u))||(width==2&&(address&1u)))return NBA97_BODY_ALIGNMENT_TRAP;
    status=r->in->access(r->in->user,pc,address,width,kind,value);
    if(status!=NBA97_BODY_OK)return status;
    if(value->is_reference>1||value->reference.known>1||
       (!value->reference.known&&(value->reference.allocation||value->reference.offset)))
        return NBA97_BODY_ARGUMENT;
    if(!value->is_reference&&value->reference.known)return NBA97_BODY_ARGUMENT;
    if(value->is_reference&&(width!=4||(!value->reference.known&&
       (value->known_mask||value->word))))return NBA97_BODY_ARGUMENT;
    if(value->known_mask&~knowledge(width)||(value->word&~mask(width)))
        return NBA97_BODY_ARGUMENT;
    for(i=0;i<width;++i)if(!(value->known_mask&(1u<<i))&&
       (value->word&(255u<<(8u*i))))return NBA97_BODY_ARGUMENT;
    if(kind==NBA97_FRAME_READ)++r->out->reads;else ++r->out->stores;
    return NBA97_BODY_OK;
}
static int rd(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t* word){
    Nba97PlayerFrameValue value={0};TRY(access(r,pc,address,width,NBA97_FRAME_READ,&value));
    if(value.is_reference||value.known_mask!=knowledge(width))return NBA97_BODY_UNKNOWN;
    *word=value.word;return NBA97_BODY_OK;
}
static int wr(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t word){
    Nba97PlayerFrameValue value={0};value.word=word&mask(width);
    value.known_mask=(uint8_t)knowledge(width);
    return access(r,pc,address,width,NBA97_FRAME_WRITE,&value);
}
static int call(Run* r,uint32_t pc,uint32_t entry,unsigned count,
                uint32_t a0,uint32_t a1,uint32_t a2){
    Nba97ScoringActorAiCall query;int status;TRY(reserve(r,pc,0,entry));
    if(!r->in->service)return NBA97_SCORING_ACTOR_AI_SERVICE_REQUIRED;
    query.pc=pc;query.entry=entry;query.argument[0]=a0;query.argument[1]=a1;
    query.argument[2]=a2;query.argument_count=count;
    status=r->in->service(r->in->user,&query);if(status!=NBA97_BODY_OK)return status;
    ++r->out->services;return NBA97_BODY_OK;
}

/* Call-free GAME6E734. The source deliberately updates the primary actor's
 * +1A before scanning teammates, so a later refusal keeps that prefix. */
static int scorer_totals(Run* r,uint32_t actor,uint32_t identity,uint32_t points){
    uint32_t i,p,v;
    ++r->out->cpu_leaves;
    if(actor){TRY(rd(r,0x8006e73c,actor+0x1au,2,&v));
        TRY(wr(r,0x8006e748,actor+0x1au,2,v+points));}
    for(i=0;i<8;++i){
        TRY(rd(r,0x8006e754,0x800fdc50u+i*4u,4,&p));++r->out->players_visited;
        TRY(rd(r,0x8006e75c,p+0x26u,2,&v));if(s32(sx16(v))<0)continue;
        if(p==actor)continue;
        TRY(rd(r,0x8006e774,p+0x24u,2,&v));if((v&0xffffu)==(identity&0xffffu))continue;
        TRY(rd(r,0x8006e784,p+0x1cu,2,&v));
        TRY(wr(r,0x8006e790,p+0x1cu,2,v+points));
    }
    return NBA97_BODY_OK;
}

/* Call-free GAME58AA8. The <999 tests intentionally guard a neighboring
 * counter while the following halfword is incremented; this original quirk
 * is not corrected. The associated actor increments are intentionally not
 * saturated. */
static int initial_score_stats(Run* r,uint32_t actor){
    uint32_t team,mode,stats,v,index,target;
    ++r->out->cpu_leaves;
    TRY(wr(r,0x80058ab8,0x800fdbea,2,1));
    TRY(rd(r,0x80058abc,actor,4,&team));
    TRY(rd(r,0x80058ac4,0x800fdbd8,2,&mode));
    TRY(rd(r,0x80058ad0,0x800fdc70u+team*4u,4,&stats));
    if((mode&0xffffu)==1){
        TRY(rd(r,0x80058ae0,stats+8u,2,&v));
        if((v&0xffffu)<999u)TRY(wr(r,0x80058af8,stats+8u,2,v+1u));
    }else{
        TRY(rd(r,0x80058afc,stats,2,&v));
        if((v&0xffffu)<999u)TRY(wr(r,0x80058b10,stats,2,v+1u));
        TRY(rd(r,0x80058b18,0x800fdbd8,2,&mode));
        if((mode&0xffffu)!=2){TRY(rd(r,0x80058b28,stats+4u,2,&v));
            if((v&0xffffu)<999u)TRY(wr(r,0x80058b3c,stats+4u,2,v+1u));}
    }
    TRY(rd(r,0x80058b40,actor+4u,2,&index));
    if(s32(sx16(index))<0)return NBA97_BODY_OK;
    TRY(rd(r,0x80058b54,0x800fdbd8,2,&mode));
    TRY(rd(r,0x80058b60,0x800fdc50u+(index&0xffffu)*4u,4,&target));
    if((mode&0xffffu)==1){TRY(rd(r,0x80058b70,target+8u,2,&v));
        return wr(r,0x80058b80,target+8u,2,v+1u);}
    TRY(rd(r,0x80058b84,target,2,&v));TRY(wr(r,0x80058b90,target,2,v+1u));
    TRY(rd(r,0x80058b98,0x800fdbd8,2,&mode));
    if((mode&0xffffu)!=2){TRY(rd(r,0x80058ba8,target+4u,2,&v));
        TRY(wr(r,0x80058bb4,target+4u,2,v+1u));}
    return NBA97_BODY_OK;
}

static int run_owner(Run* r,uint32_t* selected){
    uint32_t v,v2,ball,team,player,actor,index,mode,stats,assist=0,target=0;
    uint32_t points,current,other,difference;
    TRY(rd(r,0x8006e7b0,0x800fe8cc,4,&v));
    TRY(wr(r,0x8006e7dc,0x800fdbd6,2,0));
    TRY(wr(r,0x8006e7e4,0x800fdbd4,2,0));
    if(v==10){
        TRY(rd(r,0x8006e7f4,0x800fe8e4,2,&v2));
        if(s32(sx16(v2))<0){TRY(rd(r,0x8006e808,0x800fe8ca,2,&index));
            TRY(rd(r,0x8006e81c,0x80020becu+((index&0xffffu)<<2),4,&actor));
            TRY(call(r,0x8006e820,0x80056ffc,2,actor,1,0));
            TRY(wr(r,0x8006e82c,0x800fe8cc,2,0));}
    }
    TRY(rd(r,0x8006e834,0x800fdc48,4,&ball));
    TRY(wr(r,0x8006e840,0x800fdbbe,2,1));
    TRY(wr(r,0x8006e84c,0x800fdba4,4,0x5a0));
    TRY(wr(r,0x8006e854,ball+0xb4u,2,30));
    TRY(rd(r,0x8006e85c,0x800fdc48,4,&ball));
    TRY(rd(r,0x8006e868,0x8001ee04,4,&v));
    TRY(rd(r,0x8006e86c,ball+8u,4,&v2));
    team=s32(v^v2)<0?0x8001eeb8u:0x8001edf4u;
    TRY(rd(r,0x8006e884,team+0x14u,2,&v));
    TRY(wr(r,0x8006e88c,0x800fa040,4,v));
    TRY(rd(r,0x8006e890,team+4u,4,&player));
    TRY(rd(r,0x8006e898,player+0x44u,2,&v));
    TRY(rd(r,0x8006e89c,player+0x46u,2,&v2));v&=0x7fffu;
    if((v2&0xffffu)<v)TRY(wr(r,0x8006e8b0,player+0x46u,2,v));
    TRY(rd(r,0x8006e8b4,team+4u,4,&player));
    TRY(wr(r,0x8006e8bc,player+0x44u,2,0));
    TRY(rd(r,0x8006e8c0,team+4u,4,&player));
    TRY(rd(r,0x8006e8c8,player+0x14u,2,&index));
    TRY(rd(r,0x8006e8d8,0x80020becu+((index&0xffffu)<<2),4,&actor));
    for(v=0;v<5;++v){TRY(wr(r,0x8006e8e0u+v*12u,actor+v*0xf4u+0xdfu,1,0));
        TRY(wr(r,0x8006e8e4u+v*12u,actor+v*0xf4u+0xdeu,1,0));}
    TRY(rd(r,0x8006e918,team+0x52u,2,&index));
    TRY(wr(r,0x8006e924,0x800fe8c6,2,index));
    TRY(rd(r,0x8006e928,team+0x48u,4,&v));
    TRY(rd(r,0x8006e930,0x800fdb58,4,&v2));
    v=(v&0x7fffffffu)-v2;
    TRY(rd(r,0x8006e934,team+0x4cu,4,&points));
    if(s32(points)<s32(v))TRY(wr(r,0x8006e950,team+0x4cu,4,v));
    TRY(rd(r,0x8006e958,0x800fdb58,4,&v));
    TRY(wr(r,0x8006e960,team+0x48u,4,v));
    TRY(rd(r,0x8006e968,0x800fdb94,2,&v));
    if(v)TRY(rd(r,0x8006e97c,0x8001eec4u+((index&0xffffu)<<1),2,&v2));
    else TRY(rd(r,0x8006e98c,0x8001ee0au+((index&0xffffu)<<1),2,&v2));
    if(v)v2+=12u;
    TRY(wr(r,0x8006e994,0x800fe8f4,2,v2));
    TRY(rd(r,0x8006e9a0,0x800fdbea,2,&v));
    TRY(rd(r,0x8006e9b0,0x80020becu+((index&0xffffu)<<2),4,&actor));
    if(!v)TRY(initial_score_stats(r,actor));
    TRY(rd(r,0x8006e9c8,0x800fdbd8,2,&mode));
    if((mode&0xffffu)!=1){TRY(rd(r,0x8006e9d8,actor+0xdeu,1,&v));
        TRY(wr(r,0x8006e9e4,actor+0xdeu,1,v+1u));}
    TRY(rd(r,0x8006e9ec,0x800fdbda,2,&v));
    TRY(rd(r,0x8006e9f4,0x800fdc70u+((index&0xffffu)<<2),4,&stats));
    if(!v){TRY(rd(r,0x8006ea04,0x800fdbd8,2,&v2));
        if((v2&0xffffu)!=1)TRY(call(r,0x8006ea14,0x8007f074,3,team,index,v2));}
    TRY(rd(r,0x8006ea1c,team+0x54u,2,&assist));
    if(s32(sx16(assist))>=0)TRY(rd(r,0x8006ea34,0x800fdc50u+((assist&0xffffu)<<2),4,&target));
    TRY(rd(r,0x8006ea3c,0x800fdbd8,2,&mode));
    if((mode&0xffffu)==1){
        TRY(rd(r,0x8006ea4c,stats+8u,2,&v));
        if((v&0xffffu)<999u){TRY(rd(r,0x8006ea60,stats+0xau,2,&v));
            TRY(wr(r,0x8006ea6c,stats+0xau,2,v+1u));}
        TRY(rd(r,0x8006ea70,team+0x54u,2,&v));
        if(s32(sx16(v))>=0){TRY(rd(r,0x8006ea80,target+0xau,2,&v));
            TRY(rd(r,0x8006ea84,target+0x24u,2,&v2));
            TRY(wr(r,0x8006ea94,target+0xau,2,v+1u));
            TRY(scorer_totals(r,target,sx16(v2),1));}
        else{TRY(rd(r,0x8006eaa0,team+0x14u,2,&v));TRY(scorer_totals(r,0,v,1));}
        goto finish;
    }
    TRY(rd(r,0x8006eab8,stats,2,&v));
    if((v&0xffffu)<999u){TRY(rd(r,0x8006eacc,stats+2u,2,&v));
        TRY(wr(r,0x8006ead8,stats+2u,2,v+1u));}
    TRY(rd(r,0x8006eadc,team+0x54u,2,&v));
    if(s32(sx16(v))>=0){TRY(rd(r,0x8006eaec,target+2u,2,&v));
        TRY(rd(r,0x8006eaf0,target+0x24u,2,&v2));
        TRY(wr(r,0x8006eb00,target+2u,2,v+1u));
        TRY(scorer_totals(r,target,sx16(v2),2));}
    else{TRY(rd(r,0x8006eb04,team+0x14u,2,&v));TRY(scorer_totals(r,0,v,2));}
    if((mode&0xffffu)==3){
        TRY(rd(r,0x8006eb24,stats,2,&v));
        if((v&0xffffu)<999u){TRY(rd(r,0x8006eb38,stats+6u,2,&v));
            TRY(wr(r,0x8006eb44,stats+6u,2,v+1u));}
        TRY(rd(r,0x8006eb48,team+0x54u,2,&v));
        if(s32(sx16(v))>=0){TRY(rd(r,0x8006eb58,target+6u,2,&v));
            TRY(rd(r,0x8006eb5c,target+0x24u,2,&v2));
            TRY(wr(r,0x8006eb6c,target+6u,2,v+1u));
            TRY(scorer_totals(r,target,sx16(v2),1));}
        else{TRY(rd(r,0x8006eb70,team+0x14u,2,&v));TRY(scorer_totals(r,0,v,1));}
    }
    TRY(rd(r,0x8006eb8c,0x800fdbda,2,&v));if(v)goto finish;
    TRY(rd(r,0x8006eb9c,team+0x56u,2,&index));if(s32(sx16(index))<0)goto finish;
    TRY(rd(r,0x8006ebac,team+0x52u,2,&v));if((index&0xffffu)==(v&0xffffu))goto finish;
    TRY(rd(r,0x8006ebc0,0x800fdc70u+((index&0xffffu)<<2),4,&stats));
    TRY(rd(r,0x8006ebc8,stats+0x10u,2,&v));
    if((v&0xffffu)<999u)TRY(wr(r,0x8006ebdc,stats+0x10u,2,v+1u));
    TRY(rd(r,0x8006ebe0,team+0x58u,2,&index));
    if(s32(sx16(index))>=0){TRY(rd(r,0x8006ebf4,0x800fdc50u+((index&0xffffu)<<2),4,&target));
        TRY(rd(r,0x8006ebfc,target+0x10u,2,&v));TRY(wr(r,0x8006ec08,target+0x10u,2,v+1u));}
    TRY(rd(r,0x8006ec0c,team+0x14u,2,&v));
    if(!v){TRY(rd(r,0x8006ec1c,team+0x56u,2,&v));
        TRY(call(r,0x8006ec20,0x8007f20c,2,0,sx16(v),0));}
finish:
    TRY(rd(r,0x8006ec28,team+0x2eu,2,&v));
    TRY(rd(r,0x8006ec2c,team+0x44u,2,&v2));
    TRY(rd(r,0x8006ec30,team+0x46u,2,&points));
    v+=mode;v2+=mode;TRY(wr(r,0x8006ec3c,team+0x44u,2,v2));
    TRY(wr(r,0x8006ec4c,team+0x2eu,2,v));
    v2&=0x7fffu;if((points&0xffffu)<v2)TRY(wr(r,0x8006ec50,team+0x46u,2,v2));
    TRY(rd(r,0x8006ec58,0x800fdc40,4,&current));
    TRY(rd(r,0x8006ec60,current+4u,4,&other));
    TRY(rd(r,0x8006ec64,current+0x2eu,2,&v));
    TRY(rd(r,0x8006ec68,other+0x2eu,2,&v2));difference=v-v2;
    TRY(wr(r,0x8006ec74,current+0xa4u,2,difference));
    TRY(rd(r,0x8006ec7c,0x800fdc40,4,&current));
    TRY(rd(r,0x8006ec84,current+0xa4u,2,&difference));
    TRY(rd(r,0x8006ec88,current+4u,4,&other));
    TRY(wr(r,0x8006ec98,other+0xa4u,2,0u-difference));
    TRY(rd(r,0x8006ec9c,0x800fe8bc,2,&v));
    if((v&0xffffu)==10)TRY(wr(r,0x8006ecac,0x800fe8bc,2,0));
    *selected=team;return NBA97_BODY_OK;
}

int nba97_game_scoring_actor_ai(Nba97ScoringActorAiContext* context,
                                uint32_t* selected,
                                Nba97ScoringActorAiProgress* progress){
    Run run;int status;if(!progress)return NBA97_BODY_ARGUMENT;
    memset(progress,0,sizeof *progress);if(selected)*selected=0;
    if(!context||!context->access||!selected)return NBA97_BODY_ARGUMENT;
    run.in=context;run.out=progress;status=run_owner(&run,selected);
    if(status==NBA97_BODY_OK){progress->completed=1;progress->stopped_pc=0;
        progress->stopped_address=0;progress->stopped_entry=0;}
    return status;
}
