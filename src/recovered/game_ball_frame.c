#include "game_ball_frame.h"
#include "game_court_packets.h"
#include <string.h>
typedef struct Run {Nba97PlayerFrameContext* in;Nba97PlayerFrameProgress* out;int link_status;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_BODY_OK)return status_;}while(0)
static int32_t s32(uint32_t v){return v<0x80000000u?(int32_t)v:-1-(int32_t)~v;}
static uint32_t sx16(uint32_t v){return v&0x8000u?(v&65535u)|0xffff0000u:v&65535u;}
static uint32_t shr(uint32_t v,unsigned n){return v&0x80000000u?(v>>n)|(~0u<<(32-n)):v>>n;}
static uint32_t bits(unsigned n){return n==4?UINT32_MAX:(1u<<(8*n))-1;}
static unsigned knowledge(unsigned n){return (1u<<n)-1;}
static int reserve(Run* r,uint32_t pc,uint32_t address){
    r->out->stopped_pc=pc;r->out->stopped_address=address;
    if(r->out->operations>=r->in->operation_budget)return NBA97_BODY_JOURNAL_LIMIT;
    ++r->out->operations;return NBA97_BODY_OK;
}
static int access(Run* r,uint32_t pc,uint32_t address,unsigned width,unsigned kind,Nba97PlayerFrameValue* v){
    int status;unsigned i;
    TRY(reserve(r,pc,address));
    if(width==3&&(address&3))return NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT;
    if((width==4&&(address&3))||(width==2&&(address&1)))return NBA97_BODY_ALIGNMENT_TRAP;
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
static int rd(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t* word){
    Nba97PlayerFrameValue v={0};TRY(access(r,pc,address,width,NBA97_FRAME_READ,&v));
    if(v.known_mask!=knowledge(width))return NBA97_BODY_UNKNOWN;
    *word=v.word;return NBA97_BODY_OK;
}
static int low16(Run* r,uint32_t pc,uint32_t address,uint32_t* word){
    Nba97PlayerFrameValue v={0};TRY(access(r,pc,address,4,NBA97_FRAME_READ,&v));
    if((v.known_mask&3)!=3)return NBA97_BODY_UNKNOWN;
    *word=v.word&65535u;return NBA97_BODY_OK;
}
static int wr(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t word){
    Nba97PlayerFrameValue v={0};v.word=word&bits(width);v.known_mask=(uint8_t)knowledge(width);
    return access(r,pc,address,width,NBA97_FRAME_WRITE,&v);
}
static int math(Run* r,uint32_t pc,unsigned kind,unsigned index,uint32_t word,uint32_t* out){
    Nba97PlayerMathRequest q;Nba97GamePeriodValue v={0,0};int status;
    TRY(reserve(r,pc,0));if(!r->in->math)return NBA97_FRAME_MATH_REQUIRED;
    q.pc=pc;q.kind=kind;q.index=index;q.word=word;
    status=r->in->math(r->in->user,&q,&v);if(status!=NBA97_BODY_OK)return status;
    ++r->out->math_calls;if(out){if(v.known>1||(!v.known&&v.word))return NBA97_BODY_ARGUMENT;
        if(!v.known)return NBA97_BODY_UNKNOWN;
        *out=v.word;}
    return NBA97_BODY_OK;
}
/* Reuse the recovered56914 owner; translate only its memory/status boundary. */
static int link_access(void* user,uint32_t pc,uint32_t address,unsigned width,int write,Nba97CourtValue* value){
    Run* r=(Run*)user;uint32_t word=value->word;int status;
    status=write?wr(r,pc,address,width,word):rd(r,pc,address,width,&word);
    if(status!=NBA97_BODY_OK){r->link_status=status;return NBA97_COURT_RESOURCE;}
    value->word=word;value->known=1;return NBA97_COURT_COMPLETE;
}
static int link(Run* r,uint32_t table,uint32_t packet){
    Nba97CourtContext c;Nba97CourtProgress p;int status;memset(&c,0,sizeof c);
    c.access=link_access;c.user=r;c.operation_budget=3;r->link_status=NBA97_BODY_OK;
    status=nba97_game_court_link(&c,table,packet,&p);
    if(status!=NBA97_COURT_COMPLETE){
        if(r->link_status!=NBA97_BODY_OK)return r->link_status;
        r->out->stopped_pc=p.stopped_pc;r->out->stopped_address=p.stopped_address;
        /*56914's LWL/SWL support other alignments in hardware. The recovered
         * shared owner deliberately bounds its native tag domain to aligned
         * words; do not mislabel that restriction as an original LW trap. */
        if(status==NBA97_COURT_ALIGNMENT)return NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT;
        if(status==NBA97_COURT_LIMIT)return NBA97_BODY_JOURNAL_LIMIT;
        return NBA97_BODY_ARGUMENT;
    }
    ++r->out->links;return NBA97_BODY_OK;
}
static int load_camera(Run* r){
    uint32_t words[5],v;unsigned i;
    for(i=0;i<5;++i){if(i==4)TRY(low16(r,0x80055f18+i*4,0x800f9fd8+i*4,&words[i]));else TRY(rd(r,0x80055f18+i*4,0x800f9fd8+i*4,4,&words[i]));}
    for(i=0;i<5;++i)TRY(math(r,i==4?0x80055f40:0x80055f2c+i*4,NBA97_PROJECTION_ROTATION,i,words[i],0));
    for(i=0;i<3;++i){TRY(rd(r,0x80055f44+i*4,0x800f9fec+i*4,4,&v));words[i]=v;}
    for(i=0;i<3;++i)TRY(math(r,i==2?0x80055f5c:0x80055f50+i*4,NBA97_PROJECTION_TRANSLATION,i,words[i],0));
    return NBA97_BODY_OK;
}
static int quad(Run* r,uint32_t vertices,uint32_t packet){
    uint32_t v;unsigned i;
    for(i=0;i<6;++i){if(i&1)TRY(low16(r,0x80055fe4+i*4,vertices+i*4,&v));else TRY(rd(r,0x80055fe4+i*4,vertices+i*4,4,&v));
        TRY(math(r,0x80055fe4+i*4,NBA97_PROJECTION_VERTEX,i,v,0));}
    TRY(math(r,0x80056000,NBA97_PROJECTION_THREE,0,0,0));
    for(i=0;i<3;++i){TRY(math(r,0x80056010+i*4,NBA97_PROJECTION_SCREEN,i,0,&v));TRY(wr(r,0x80056010+i*4,packet+8+i*8,4,v));}
    /* FLAG/IR0/SZ3 reads and private caller-stack results are dead in49E9C.
     * They do not change retained geometry; no fabricated stack destination. */
    for(i=0;i<2;++i){if(i)TRY(low16(r,0x80056024,vertices+28,&v));else TRY(rd(r,0x80056020,vertices+24,4,&v));
        TRY(math(r,0x80056020+i*4,NBA97_PROJECTION_VERTEX,i,v,0));}
    TRY(math(r,0x8005602c,NBA97_FRAME_PROJECT_ONE,0,0,0));
    TRY(math(r,0x8005603c,NBA97_PROJECTION_SCREEN,2,0,&v));TRY(wr(r,0x8005603c,packet+32,4,v));
    return NBA97_BODY_OK;
}
static int indirect(Run* r,uint32_t pc,uint32_t slot,uint32_t next,unsigned width,uint32_t* value){
    uint32_t pointer;TRY(rd(r,pc,slot,4,&pointer));return rd(r,next,pointer,width,value);
}
static int animation(Run* r){
    uint32_t flag,a,b,p,x,y,z,old;int32_t ax,ay,az;
    TRY(rd(r,0x80049304,0x80103ed4,2,&flag));
    if(!flag){TRY(indirect(r,0x80049320,0x800fc660,0x80049328,2,&flag));if(flag)return NBA97_BODY_OK;}
    TRY(indirect(r,0x8004933c,0x800fc644,0x80049344,4,&a));
    if(a){
        TRY(indirect(r,0x80049358,0x800fc640,0x80049360,2,&b));
        if(s32(sx16(b))<0){
            TRY(indirect(r,0x80049374,0x800fc64c,0x8004937c,4,&b));
            if(a!=b){
                TRY(rd(r,0x80049390,0x800fc654,4,&p));TRY(rd(r,0x80049398,p+0x99c,2,&x));TRY(rd(r,0x8004939c,p+0x99e,2,&y));
                TRY(rd(r,0x800493bc,0x800fc654,4,&p));TRY(rd(r,0x800493c4,p+0x9a0,2,&z));
                ax=s32(sx16(x));ay=s32(sx16(y));az=s32(sx16(z));
                TRY(rd(r,az<0?0x800493e8:0x800493d8,0x800dc7fc,4,&old));
                x=(uint32_t)(ax<0?-ax:ax);y=(uint32_t)(ay<0?-ay:ay);z=(uint32_t)(az<0?-az:az);
                TRY(wr(r,0x800493f8,0x800dc7fc,4,old-x-y-z));
            }
        }
    }
    TRY(rd(r,0x80049400,0x800dc7fc,4,&old));
    if(s32(old)<0){
        TRY(rd(r,0x80049414,0x80103f9c,4,&a));++a;
        TRY(wr(r,0x8004942c,0x800dc7fc,4,500));
        /* Wrapped increment, signed remainder: negative frame values are not
         * repaired to the ordinary0..14 animation range. */
        TRY(wr(r,0x80049454,0x80103f9c,4,(uint32_t)(s32(a)%15)));
    }
    return wr(r,0x8004945c,0x80103ed4,2,0);
}
static int uv(Run* r){
    static const uint32_t frame_pc[]={0x80049464,0x800494c0,0x80049508,0x80049560,0x800495a8,0x80049600,0x80049648,0x800496a0,0x800496e8,0x80049740,0x80049788,0x800497e0,0x80049828,0x80049880,0x800498c8,0x80049920};
    static const uint32_t bank_pc[]={0x80049478,0x800494d0,0x80049518,0x80049570,0x800495b8,0x80049610,0x80049658,0x800496b0,0x800496f8,0x80049750,0x80049798,0x800497f0,0x80049838,0x80049890,0x800498d8,0x80049934};
    static const uint32_t store_pc[]={0x800494b8,0x80049500,0x80049558,0x800495a0,0x800495f8,0x80049640,0x80049698,0x800496e0,0x80049738,0x80049780,0x800497d8,0x80049820,0x80049878,0x800498c0,0x80049918,0x80049964};
    static const uint32_t address[]={0x80103ef0,0x80103ef1,0x80103ef8,0x80103ef9,0x80103f00,0x80103f01,0x80103f08,0x80103f09,0x8010b1fc,0x8010b1fd,0x8010b204,0x8010b205,0x8010b20c,0x8010b20d,0x8010b214,0x8010b215};
    static const uint32_t bias[]={64,96,95,96,64,128,95,128,64,127,95,127,64,96,95,96};
    unsigned i;uint32_t frame,bank,value;
    /* Each UV byte reloads BOTH frame and bank. Earlier packet aliases can
     * change subsequent reads; do not snapshot the whole sprite update. */
    for(i=0;i<16;++i){
        TRY(rd(r,frame_pc[i],0x80103f9c,4,&frame));TRY(rd(r,bank_pc[i],0x8001ede8,4,&bank));
        value=(uint32_t)((i&1)?s32(frame)/6:s32(frame)%6)*32u+bias[i];
        TRY(wr(r,store_pc[i],address[i]+bank*40u,1,value));
    }
    return NBA97_BODY_OK;
}
static int project(Run* r,uint32_t packet_xy,uint32_t* depth){
    uint32_t value;
    TRY(rd(r,0x80056624,0x800fea64,4,&value));TRY(math(r,0x80056624,NBA97_PROJECTION_VERTEX,0,value,0));
    TRY(low16(r,0x80056628,0x800fea68,&value));TRY(math(r,0x80056628,NBA97_PROJECTION_VERTEX,1,value,0));
    TRY(math(r,0x80056630,NBA97_FRAME_PROJECT_ONE,0,0,0));
    TRY(math(r,0x80056634,NBA97_PROJECTION_SCREEN,2,0,&value));TRY(wr(r,0x80056634,packet_xy,4,value));
    /* IR0/FLAG only go to dead, private caller stack slots. SZ3 is live. */
    TRY(math(r,0x80056640,NBA97_FRAME_DEPTH,3,0,&value));*depth=shr(value,2);return NBA97_BODY_OK;
}
static int centers(Run* r,uint32_t* ball_depth,uint32_t* reflection_depth){
    uint32_t slot,p,x,y,z,bank,color;
    TRY(load_camera(r));TRY(rd(r,0x8004997c,0x800fc658,4,&slot));
    TRY(rd(r,0x80049984,slot,4,&p));TRY(rd(r,0x8004998c,p+8,4,&x));TRY(wr(r,0x800499a0,0x800fea64,2,shr(x,8)<<3));
    TRY(rd(r,0x800499a4,slot,4,&p));TRY(rd(r,0x800499ac,p+16,4,&y));TRY(wr(r,0x800499c0,0x800fea66,2,shr(y,8)<<3));
    TRY(rd(r,0x800499c4,slot,4,&p));TRY(rd(r,0x800499cc,0x8001ede8,4,&bank));TRY(rd(r,0x800499d8,p+12,4,&z));
    TRY(wr(r,0x800499fc,0x800fea68,2,shr(z,8)<<3));TRY(project(r,0x80103eec+bank*40u,ball_depth));
    TRY(rd(r,0x80049a0c,0x800fea66,2,&y));color=128u-shr(y<<16,20);
    if(s32(color)<1)color=1;else if(s32(color)>128)color=128;
    TRY(rd(r,0x80049a44,0x8001ede8,4,&bank));TRY(wr(r,0x80049a60,0x8010b1f4+bank*40u,1,color));
    TRY(rd(r,0x80049a68,0x8001ede8,4,&bank));TRY(wr(r,0x80049a8c,0x8010b1f5+bank*40u,1,color));
    TRY(rd(r,0x80049a94,0x8001ede8,4,&bank));TRY(wr(r,0x80049ab0,0x8010b1f6+bank*40u,1,color));
    TRY(rd(r,0x80049ab8,0x800fc658,4,&slot));TRY(rd(r,0x80049ac4,0x8001ede8,4,&bank));
    TRY(rd(r,0x80049acc,slot,4,&p));TRY(rd(r,0x80049ae0,p+16,4,&y));
    TRY(wr(r,0x80049b00,0x800fea66,2,(0u-shr(y,8))<<3));
    return project(r,0x8010b1f8+bank*40u,reflection_depth);
}
static int divide(Run* r,uint32_t pc,uint32_t numerator,uint32_t denominator,uint32_t* value){
    if(!denominator||(numerator==0x80000000u&&denominator==UINT32_MAX)){
        r->out->stopped_pc=pc+(!denominator?12u:36u);r->out->stopped_address=0;return NBA97_FRAME_ARITHMETIC_TRAP;}
    *value=(uint32_t)(s32(numerator)/s32(denominator));return NBA97_BODY_OK;
}
static int sprite(Run* r,uint32_t depth,uint32_t reflected_depth){
    uint32_t h,numerator,size,reflected_size,bank,table,ball,mirror,dx,dy,rx,ry,x,y,left,right,top,bottom,bucket;
    if(!depth)depth=1;
    if(!reflected_depth)reflected_depth=1;
    TRY(rd(r,0x80049b20,0x800b729c,4,&h));
    /* The source shifts/subtracts before signed division. Keep low32 wrap;
     * computing an unbounded H*245760 first changes large input behavior. */
    numerator=(uint32_t)(s32(((h<<12)-(h<<8))<<6)/1000);
    TRY(divide(r,0x80049b50,numerator,depth,&size));TRY(divide(r,0x80049b84,numerator,reflected_depth,&reflected_size));
    if(s32(size)>=1000)return NBA97_BODY_OK;
    if(s32(size)<100)size=100;
    TRY(rd(r,0x80049bcc,0x8001ede8,4,&bank));bucket=(depth&4095u)*4u;
    ball=0x80103ee4+bank*40u;mirror=0x8010b1f0+bank*40u;
    dy=shr(size<<5,10);ry=shr(reflected_size<<5,10);
    dx=shr((uint32_t)(s32(size<<9)/10),10);rx=shr((uint32_t)(s32(reflected_size<<9)/10),10);
    TRY(rd(r,0x80049c0c,0x80102924,4,&table));TRY(rd(r,0x80049c18,ball+10,2,&y));TRY(rd(r,0x80049c24,ball+8,2,&x));
    x=sx16(x);y=sx16(y);top=y-dy;bottom=y+dy;left=x-dx;right=x+dx;
    TRY(wr(r,0x80049c3c,ball+26,2,bottom));TRY(wr(r,0x80049c40,ball+34,2,bottom));
    TRY(wr(r,0x80049c48,ball+10,2,top));TRY(wr(r,0x80049c4c,ball+18,2,top));
    TRY(wr(r,0x80049c6c,ball+8,2,left));TRY(wr(r,0x80049c70,ball+16,2,right));
    TRY(wr(r,0x80049c74,ball+24,2,left));TRY(wr(r,0x80049c78,ball+32,2,right));
    TRY(rd(r,0x80049c84,mirror+8,2,&x));TRY(rd(r,0x80049c90,mirror+10,2,&y));
    x=sx16(x);y=sx16(y);top=y-ry;bottom=y+ry;left=x-rx;right=x+rx;
    TRY(wr(r,0x80049cb8,mirror+10,2,top));TRY(wr(r,0x80049cbc,mirror+18,2,top));
    TRY(wr(r,0x80049cc4,mirror+8,2,left));TRY(wr(r,0x80049cc8,mirror+16,2,right));
    TRY(wr(r,0x80049ccc,mirror+24,2,left));TRY(wr(r,0x80049cd0,mirror+26,2,bottom));
    TRY(wr(r,0x80049cd4,mirror+32,2,right));TRY(wr(r,0x80049cdc,mirror+34,2,bottom));
    TRY(link(r,table+bucket,ball));TRY(rd(r,0x80049ce4,0x800dcf10,4,&h));
    /* Reflected size has no100 clamp, and reflected linking uses BALL depth.
     * The final table and bank are reloaded after the first packet splice. */
    if(!h){TRY(rd(r,0x80049cf8,0x80102924,4,&table));TRY(rd(r,0x80049d00,0x8001ede8,4,&bank));TRY(link(r,table+bucket,0x8010b1f0+bank*40u));}
    return NBA97_BODY_OK;
}
static int ball(Run* r){uint32_t depth,reflection;TRY(animation(r));TRY(uv(r));TRY(centers(r,&depth,&reflection));return sprite(r,depth,reflection);}
static int shadow(Run* r){
    uint32_t p,x,z,bank,table;unsigned i;static const uint32_t xp[]={0x80049dac,0x80049db8,0x80049dc4,0x80049dd0};
    static const uint32_t zp[]={0x80049db4,0x80049dc0,0x80049dcc,0x80049dd8};
    TRY(indirect(r,0x80049d38,0x800fc658,0x80049d4c,4,&p));TRY(rd(r,0x80049d54,p+8,4,&x));TRY(rd(r,0x80049d58,p+12,4,&z));
    for(i=0;i<4;++i)TRY(wr(r,0x80049d70+i*8,0x800d8ef6+i*8,2,0));
    x=shr(x,8)<<3;z=shr(z,8)<<3;
    for(i=0;i<4;++i){TRY(wr(r,xp[i],0x800d8ef4+i*8,2,x+((i&1)?32u:0u-32u)));TRY(wr(r,zp[i],0x800d8ef8+i*8,2,z+(i<2?32u:0u-32u)));}
    TRY(load_camera(r));TRY(rd(r,0x80049df0,0x8001ede8,4,&bank));TRY(quad(r,0x800d8ef4,0x800d9234+bank*40u));
    TRY(rd(r,0x80049e5c,0x80102924,4,&table));TRY(rd(r,0x80049e64,0x8001ede8,4,&bank));TRY(link(r,table+0x3ff8,0x800d9234+bank*40u));
    ++r->out->shadows;return NBA97_BODY_OK;
}
static int run(Nba97PlayerFrameContext* c,Nba97PlayerFrameProgress* p,int ground){
    Run r;int status;if(!c||!p||!c->access)return NBA97_BODY_ARGUMENT;
    memset(p,0,sizeof *p);r.in=c;r.out=p;r.link_status=NBA97_BODY_OK;status=ground?shadow(&r):ball(&r);
    if(status==NBA97_BODY_OK){p->completed=1;p->stopped_pc=0;p->stopped_address=0;}return status;
}
int nba97_game_ball_frame(Nba97PlayerFrameContext* c,Nba97PlayerFrameProgress* p){return run(c,p,0);}
int nba97_game_ball_shadow(Nba97PlayerFrameContext* c,Nba97PlayerFrameProgress* p){return run(c,p,1);}
