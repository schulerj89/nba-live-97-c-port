#include "game_ball_attachment.h"
#include <string.h>
typedef struct Run {Nba97PlayerFrameContext* in;Nba97PlayerFrameProgress* out;} Run;
typedef struct Endpoint {uint32_t x,z,height;} Endpoint;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_BODY_OK)return status_;}while(0)
static uint32_t sx16(uint32_t v){return v&0x8000u?(v&65535u)|0xffff0000u:v&65535u;}
static int32_t s32(uint32_t v){return v<0x80000000u?(int32_t)v:-1-(int32_t)~v;}
static uint32_t bits(unsigned n){return n==4?UINT32_MAX:(1u<<(8*n))-1;}
static unsigned knowledge(unsigned n){return (1u<<n)-1;}
static int reserve(Run* r,uint32_t pc,uint32_t address){
    r->out->stopped_pc=pc;r->out->stopped_address=address;
    if(r->out->operations>=r->in->operation_budget)return NBA97_BODY_JOURNAL_LIMIT;
    ++r->out->operations;return NBA97_BODY_OK;
}
static int access(Run* r,uint32_t pc,uint32_t address,unsigned width,unsigned kind,Nba97PlayerFrameValue* v){
    unsigned i;int status;TRY(reserve(r,pc,address));
    if(address&(width-1))return NBA97_BODY_ALIGNMENT_TRAP;
    status=r->in->access(r->in->user,pc,address,width,kind,v);if(status!=NBA97_BODY_OK)return status;
    if(v->is_reference>1||v->reference.known>1||(!v->reference.known&&(v->reference.allocation||v->reference.offset)))return NBA97_BODY_ARGUMENT;
    if(!v->is_reference&&v->reference.known)return NBA97_BODY_ARGUMENT;
    if(v->is_reference&&(width!=4||(!v->reference.known&&(v->known_mask||v->word))))return NBA97_BODY_ARGUMENT;
    if(v->known_mask&~knowledge(width))return NBA97_BODY_ARGUMENT;
    if(v->word&~bits(width))return NBA97_BODY_ARGUMENT;
    for(i=0;i<width;++i)if(!(v->known_mask&(1u<<i))&&(v->word&(255u<<(8*i))))return NBA97_BODY_ARGUMENT;
    if(kind==NBA97_FRAME_READ)++r->out->reads;else ++r->out->stores;
    return NBA97_BODY_OK;
}
static int value(Run* r,uint32_t pc,uint32_t address,unsigned width,Nba97PlayerFrameValue* v){
    memset(v,0,sizeof *v);return access(r,pc,address,width,NBA97_FRAME_READ,v);
}
static int require(Run* r,uint32_t pc,uint32_t address,Nba97PlayerFrameValue v,unsigned width,uint32_t* word){
    r->out->stopped_pc=pc;r->out->stopped_address=address;
    if(v.known_mask!=knowledge(width))return NBA97_BODY_UNKNOWN;
    *word=v.word;return NBA97_BODY_OK;
}
static int rd(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t* word){
    Nba97PlayerFrameValue v;TRY(value(r,pc,address,width,&v));return require(r,pc,address,v,width,word);
}
static int wr(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t word){
    Nba97PlayerFrameValue v={0};v.word=word&bits(width);v.known_mask=(uint8_t)knowledge(width);
    return access(r,pc,address,width,NBA97_FRAME_WRITE,&v);
}
static int endpoint(Run* r,uint32_t actor,uint32_t hand,const uint32_t* destinations,Endpoint* out){
    Nba97PlayerFrameValue flags;uint32_t table,id,v,height;
    TRY(value(r,0x8002d37c,actor+0x9a,2,&flags));
    /* The full LHU span is validated, but ANDI1 discards its high byte. */
    if(!(flags.known_mask&1))return NBA97_BODY_UNKNOWN;
    if(hand==(flags.word&1)){table=0x800fed20;TRY(rd(r,0x8002d3a4,actor,4,&id));}
    else {table=0x800faa04;TRY(rd(r,0x8002d390,actor,4,&id));}
    table+=id<<3;
    TRY(rd(r,0x8002d3b8,table,2,&v));out->x=sx16(v)<<5;
    if(destinations)TRY(wr(r,0x8002d3c4,destinations[0],4,out->x));
    TRY(rd(r,0x8002d3c8,table+4,2,&v));out->z=sx16(v)<<5;
    if(destinations)TRY(wr(r,0x8002d3d4,destinations[1],4,out->z));
    TRY(rd(r,0x8002d3d8,table+2,2,&v));TRY(rd(r,0x8002d3dc,actor+0x10,4,&height));
    out->height=(sx16(v)<<5)+height;
    if(destinations)TRY(wr(r,0x8002d3ec,destinations[2],4,out->height));
    return NBA97_BODY_OK;
}
static int call_endpoint(Run* r,uint32_t pc,uint32_t actor,uint32_t hand,Endpoint* out){
    TRY(reserve(r,pc,0));TRY(endpoint(r,actor,hand,0,out));++r->out->child_calls;return NBA97_BODY_OK;
}
static int divide(Run* r,uint32_t numerator,uint32_t denominator,uint32_t zero_pc,uint32_t overflow_pc,uint32_t* out){
    if(!denominator||(numerator==0x80000000u&&denominator==UINT32_MAX)){
        r->out->stopped_pc=denominator?overflow_pc:zero_pc;r->out->stopped_address=0;
        return NBA97_FRAME_ARITHMETIC_TRAP;
    }
    *out=(uint32_t)(s32(numerator)/s32(denominator));return NBA97_BODY_OK;
}
static int attach(Run* r,uint32_t entry,uint32_t* result){
    uint32_t index,actor,ball,state,v,animation,factor,denominator,qx,qz;
    uint32_t offset=entry-NBA97_BALL_ATTACH_PRIMARY;
    Nba97PlayerFrameValue actor_value,ball_value;Endpoint first,second;
    int blend=entry==NBA97_BALL_ATTACH_BLEND;
    uint32_t owner_pc=blend?0x80057f60:0x80058124+offset;
    TRY(rd(r,owner_pc,0x800fdbcc,2,&index));
    if(index&0x8000){*result=blend?32:sx16(index);return NBA97_BODY_OK;}
    TRY(value(r,blend?0x80057f90:0x80058148+offset,0x80020bec+(index<<2),4,&actor_value));
    TRY(value(r,blend?0x80057f98:0x80058154+offset,0x800fdc48,4,&ball_value));
    /* Both source pointer loads precede the first dereference. An unavailable
     * ball pointer must not suppress the reached hand-array reads. */
    TRY(require(r,0x8002d37c,0x80020bec+(index<<2),actor_value,4,&actor));
    TRY(call_endpoint(r,blend?0x80057fac:0x80058168+offset,actor,entry==NBA97_BALL_ATTACH_SECONDARY,&first));
    if(blend){
        TRY(rd(r,0x80057fb4,actor+0x46,2,&state));
        if(state-21u<2){
            TRY(call_endpoint(r,0x80057fdc,actor,1,&second));
            TRY(rd(r,0x80057fe4,actor+0xb8,2,&v));
            if(v==1){TRY(rd(r,0x80057ff4,actor+0x50,2,&factor));factor-=16;}
            else {
                TRY(rd(r,0x80058004,0x8001ecec,4,&animation));
                TRY(rd(r,0x80058008,actor+0x50,2,&v));
                TRY(rd(r,0x8005800c,animation+7,1,&factor));factor-=v+1;
            }
            TRY(rd(r,0x80058034,0x8001ecec,4,&animation));
            TRY(rd(r,0x8005803c,animation+7,1,&denominator));denominator-=16;
            /* MULT/MFLO wraps before signed DIV. No endpoint/range clamp. */
            TRY(divide(r,(second.x-first.x)*factor,denominator,0x80058058,0x80058070,&qx));
            TRY(divide(r,(second.z-first.z)*factor,denominator,0x800580a4,0x800580bc,&qz));
            first.x+=qx;first.z+=qz;
        }
    }
    TRY(rd(r,blend?0x800580d4:0x80058170+offset,actor+8,4,&v));
    TRY(require(r,blend?0x800580e4:0x80058180+offset,0x800fdc48,ball_value,4,&ball));
    TRY(wr(r,blend?0x800580e4:0x80058180+offset,ball+8,4,v+first.x));
    /* Read live actor Z after the X store; ball/actor may actually alias. */
    TRY(rd(r,blend?0x800580e8:0x80058184+offset,actor+0xc,4,&v));
    TRY(wr(r,blend?0x800580f8:0x80058194+offset,ball+0xc,4,v+first.z));
    if(!blend)TRY(wr(r,0x8005819c+offset,ball+0x10,4,first.height));
    TRY(wr(r,blend?0x80058104:0x800581a4+offset,0x800fdc32,2,blend?1:entry==NBA97_BALL_ATTACH_PRIMARY?2:3));
    *result=blend?first.height:entry==NBA97_BALL_ATTACH_PRIMARY?2:3;
    return NBA97_BODY_OK;
}
static int begin(Run* r,Nba97PlayerFrameContext* c,Nba97GamePeriodValue* v,Nba97PlayerFrameProgress* p){
    if(!c||!c->access||!v||!p)return NBA97_BODY_ARGUMENT;
    memset(p,0,sizeof *p);memset(v,0,sizeof *v);r->in=c;r->out=p;return NBA97_BODY_OK;
}
static int finish(Run* r,int status,uint32_t word,Nba97GamePeriodValue* v){
    if(status==NBA97_BODY_OK){v->word=word;v->known=1;r->out->completed=1;r->out->stopped_pc=0;r->out->stopped_address=0;}
    return status;
}
int nba97_game_ball_attachment(Nba97PlayerFrameContext* c,uint32_t entry,Nba97GamePeriodValue* v,Nba97PlayerFrameProgress* p){
    Run r;uint32_t word=0;int status;TRY(begin(&r,c,v,p));
    if(entry!=NBA97_BALL_ATTACH_BLEND&&entry!=NBA97_BALL_ATTACH_PRIMARY&&entry!=NBA97_BALL_ATTACH_SECONDARY)return NBA97_BODY_ARGUMENT;
    status=attach(&r,entry,&word);return finish(&r,status,word,v);
}
int nba97_game_hand_endpoint(Nba97PlayerFrameContext* c,uint32_t actor,uint32_t hand,
    uint32_t x,uint32_t z,uint32_t height,Nba97GamePeriodValue* v,Nba97PlayerFrameProgress* p){
    Run r;Endpoint result={0,0,0};uint32_t destinations[3];int status;TRY(begin(&r,c,v,p));
    destinations[0]=x;destinations[1]=z;destinations[2]=height;
    status=endpoint(&r,actor,hand,destinations,&result);return finish(&r,status,result.height,v);
}
