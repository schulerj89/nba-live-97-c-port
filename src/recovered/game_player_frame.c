#include "game_player_frame.h"
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
static int wp(Run* r,uint32_t pc,uint32_t address,uint32_t word){
    Nba97PlayerFrameValue v={0};v.word=word;v.known_mask=15;return access(r,pc,address,4,NBA97_FRAME_WRITE_POINTER,&v);
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
static int child(Run* r,uint32_t pc,uint32_t entry){
    int status;TRY(reserve(r,pc,0));if(!r->in->child)return NBA97_FRAME_CHILD_REQUIRED;
    status=r->in->child(r->in->user,pc,entry);if(status!=NBA97_BODY_OK)return status;
    ++r->out->child_calls;return NBA97_BODY_OK;
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
static int copy40(Run* r,uint32_t source,uint32_t destination){
    Nba97PlayerFrameValue v[4],extra;unsigned i,j;int backward=0;
    /* AA65C uses signed ADD (not ADDU). Preserve its overflow trap. */
    if(s32(source)<s32(destination)){
        TRY(reserve(r,0x800aa65c,source));
        if(s32(source)>INT32_MAX-40)return NBA97_FRAME_ARITHMETIC_TRAP;
        backward=s32(destination)<s32(source+40);
    }
    if((source|destination)&3){TRY(reserve(r,backward?0x800aa67c:0x800aa478,source));return NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT;}
    if(backward){
        TRY(reserve(r,0x800aa670,destination));
        if(s32(destination)>INT32_MAX-40)return NBA97_FRAME_ARITHMETIC_TRAP;
        source+=40;destination+=40;
    }
    for(j=0;j<2;++j){
        if(backward){source-=16;destination-=16;}
        for(i=0;i<4;++i){memset(&v[i],0,sizeof v[i]);TRY(access(r,(backward?0x800aa690:0x800aa528)+i*4,source+i*4,4,NBA97_FRAME_READ,&v[i]));}
        for(i=0;i<4;++i)TRY(access(r,(backward?0x800aa6a0:0x800aa538)+i*4,destination+i*4,4,NBA97_FRAME_WRITE,&v[i]));
        if(!backward){source+=16;destination+=16;}
    }
    for(i=0;i<2;++i){
        memset(&v[0],0,sizeof v[0]);
        if(backward){
            source-=4;destination-=4;
            TRY(access(r,0x800aa73c,source,4,NBA97_FRAME_READ,&v[0]));
            /* Aligned LWR overwrites all32bits loaded by LWL. Both source
             * accesses and both fullword SWL/SWR stores still occur. */
            memset(&extra,0,sizeof extra);TRY(access(r,0x800aa740,source,4,NBA97_FRAME_READ,&extra));v[0]=extra;
            TRY(access(r,0x800aa748,destination,4,NBA97_FRAME_WRITE,&v[0]));
            TRY(access(r,0x800aa74c,destination,4,NBA97_FRAME_WRITE,&v[0]));
        }else{
            TRY(access(r,0x800aa564,source,4,NBA97_FRAME_READ,&v[0]));
            TRY(access(r,0x800aa56c,destination,4,NBA97_FRAME_WRITE,&v[0]));source+=4;destination+=4;
        }
    }
    return NBA97_BODY_OK;
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
static int shadow(Run* r){
    uint32_t ctx,x,z,index,bank,table;unsigned i;
    static const uint32_t yp[4]={0x80049ec4,0x80049eec,0x80049f14,0x80049f3c};
    static const uint32_t xp[4]={0x80049eb4,0x80049ee4,0x80049f0c,0x80049f34};
    static const uint32_t zp[4]={0x80049ed0,0x80049ef8,0x80049f20,0x80049f48};
    static const uint32_t xs[4]={0x80049ecc,0x80049ef4,0x80049f1c,0x80049f44};
    static const uint32_t zs[4]={0x80049ee0,0x80049f08,0x80049f30,0x80049f5c};
    TRY(rd(r,0x80049ea0,0x800f0ed4,4,&ctx));
    for(i=0;i<4;++i){TRY(rd(r,xp[i],ctx+4,4,&x));TRY(wr(r,yp[i],0x800d8ef6+i*8,2,0));
        TRY(wr(r,xs[i],0x800d8ef4+i*8,2,x+((i&1)?128u:0xffffff80u)));
        TRY(rd(r,zp[i],ctx+8,4,&z));TRY(wr(r,zs[i],0x800d8ef8+i*8,2,z+(i<2?128u:0xffffff80u)));}
    TRY(load_camera(r));TRY(rd(r,0x80049f74,0x801029b0,4,&index));TRY(rd(r,0x80049f7c,0x8001ede8,4,&bank));
    TRY(quad(r,0x800d8ef4,0x800d8f14+index*80+bank*40));
    TRY(rd(r,0x80049ff0,0x80102924,4,&table));TRY(rd(r,0x80049ff8,0x801029b0,4,&index));TRY(rd(r,0x8004a000,0x8001ede8,4,&bank));
    TRY(link(r,table+0x3ff8,0x800d8f14+index*80+bank*40));++r->out->shadows;return NBA97_BODY_OK;
}
static int indirect(Run* r,uint32_t load_pc,uint32_t slot,uint32_t deref_pc,unsigned width,uint32_t* value){
    uint32_t p;TRY(rd(r,load_pc,slot,4,&p));return rd(r,deref_pc,p,width,value);
}
static int indicator(Run* r){
    uint32_t a,b,index,physical,bank,xy,packet,u0,u3,v0,v3,w,h,table;int32_t x,y,ax,ay;unsigned direction,i;int use_screen=0;
    TRY(indirect(r,0x8004c5ec,0x800fc644,0x8004c60c,4,&a));
    if(a){TRY(indirect(r,0x8004c620,0x800fc640,0x8004c628,2,&b));
        if(s32(sx16(b))<0){TRY(indirect(r,0x8004c63c,0x800fc64c,0x8004c644,4,&b));
            if(a!=b){TRY(indirect(r,0x8004c658,0x800fc660,0x8004c660,2,&b));use_screen=b==0;}}}
    if(use_screen){TRY(rd(r,0x8004c68c,0x801029b0,4,&index));TRY(rd(r,0x8004c6a0,0x800fea94+index*4,4,&xy));x=s32(sx16(xy));y=s32(sx16(xy>>16));}
    else{TRY(rd(r,0x8004c674,0x8001ede8,4,&bank));x=s32(1000+bank*4);y=1000;}
    TRY(rd(r,0x8004c6c4,0x801029b0,4,&index));TRY(rd(r,0x8004c6cc,0x800fc654,4,&physical));TRY(rd(r,0x8004c6e8,physical+index*244+4,2,&a));
    if(s32(sx16(a))<0)return NBA97_BODY_OK;
    ax=x<30?30:(x>=461?460:x);ay=y<30?30:(y>=201?200:y);
    if(x<30)direction=y<30?7:(y>=201?5:6);
    else if(x>=461)direction=y<30?1:(y>=201?3:2);
    else if(y<30)direction=0;else if(y>=201)direction=4;else return NBA97_BODY_OK;
    TRY(indirect(r,0x8004c7b0,0x800fc638,0x8004c7b8,2,&a));if(a)return NBA97_BODY_OK;
    TRY(rd(r,0x8004c7cc,0x800fdbcc,2,&a));TRY(rd(r,0x8004c7d4,0x801029b0,4,&index));if(sx16(a)==index)return NBA97_BODY_OK;
    TRY(rd(r,0x8004c7fc,0x8001ede8,4,&bank));TRY(copy40(r,0x800faa54+direction*40,0x80106090+index*80+bank*40));
    for(i=0;i<3;++i){uint32_t pc=0x8004c834+i*104;
        TRY(rd(r,pc,0x8001ede8,4,&bank));TRY(rd(r,pc+8,0x801029b0,4,&index));TRY(rd(r,pc+16,0x800fc654,4,&physical));
        TRY(rd(r,pc+48,physical+index*244+4,2,&a));TRY(rd(r,pc+72,0x800b72a0+i*8+sx16(a),1,&b));
        TRY(wr(r,pc+96,0x80106094+index*80+bank*40+i,1,b));}
    TRY(rd(r,0x8004c96c,0x8001ede8,4,&bank));TRY(rd(r,0x8004c974,0x801029b0,4,&index));packet=0x80106090+bank*40+index*80;
    TRY(rd(r,0x8004c99c,packet+36,1,&u3));TRY(rd(r,0x8004c9a8,packet+12,1,&u0));
    TRY(rd(r,0x8004c9b4,packet+37,1,&v3));TRY(rd(r,0x8004c9c0,packet+13,1,&v0));w=u3>u0?u3-u0:u0-u3;h=v3>v0?v3-v0:v0-v3;
    TRY(wr(r,0x8004c9f4,packet+8,2,(uint32_t)ax));TRY(wr(r,0x8004c9f8,packet+10,2,(uint32_t)ay));
    TRY(wr(r,0x8004c9fc,packet+16,2,(uint32_t)ax+w));TRY(wr(r,0x8004ca00,packet+18,2,(uint32_t)ay));
    TRY(wr(r,0x8004ca04,packet+24,2,(uint32_t)ax));TRY(wr(r,0x8004ca08,packet+26,2,(uint32_t)ay+h));
    TRY(wr(r,0x8004ca0c,packet+32,2,(uint32_t)ax+w));TRY(wr(r,0x8004ca10,packet+34,2,(uint32_t)ay+h));++r->out->indicators;
    TRY(indirect(r,0x8004ca18,0x800fc644,0x8004ca20,4,&a));if(!a)return NBA97_BODY_OK;
    /* Original early test is any negative; late test is EXACTLY -1. Keep the
     * already-written packet when the late condition prevents linking it. */
    TRY(indirect(r,0x8004ca34,0x800fc640,0x8004ca3c,2,&b));if(b!=65535)return NBA97_BODY_OK;
    TRY(indirect(r,0x8004ca50,0x800fc64c,0x8004ca58,4,&b));if(a==b)return NBA97_BODY_OK;
    TRY(indirect(r,0x8004ca6c,0x800fc660,0x8004ca74,2,&b));if(b)return NBA97_BODY_OK;
    TRY(indirect(r,0x8004ca88,0x800fc638,0x8004ca90,2,&b));if(s32(sx16(b))>0)return NBA97_BODY_OK;
    if((uint32_t)y-20<201&&(uint32_t)x-20<473)return NBA97_BODY_OK;
    TRY(rd(r,0x8004cabc,0x80102924,4,&table));return link(r,table+40,packet);
}
static int frame(Run* r){
    uint32_t a,b,index,physical,entities,entity,ctx,angle,value,i32,i8,xy,mask;
    TRY(rd(r,0x80052918,0x800b72d4,4,&a));TRY(rd(r,0x80052920,0x800b72d8,4,&b));a+=b;
    TRY(wr(r,0x8005293c,0x800b72d4,4,a));
    if(s32(a)>=16){TRY(wr(r,0x8005294c,0x800b72d4,4,15));TRY(wr(r,0x80052970,0x800b72d8,4,0xfffffffeu));}
    else if(s32(a)<2){TRY(wr(r,0x80052968,0x800b72d4,4,2));TRY(wr(r,0x80052970,0x800b72d8,4,2));}
    TRY(rd(r,0x80052978,0x800f0ed8,4,&ctx));TRY(wr(r,0x80052984,0x801029b0,4,0));TRY(wp(r,0x8005298c,0x800f0ed4,ctx));
    do{
        TRY(rd(r,0x80052994,0x801029b0,4,&index));TRY(rd(r,0x8005299c,0x800fc654,4,&physical));TRY(rd(r,0x800529a4,0x800fc650,4,&entities));
        TRY(rd(r,0x800529c0,entities+index*4,4,&entity));TRY(rd(r,0x800529cc,physical+index*244+0x98,2,&angle));
        TRY(rd(r,0x800529d0,entity+8,4,&value));TRY(rd(r,0x800529d8,0x800f0ed4,4,&ctx));TRY(wr(r,0x800529e0,ctx+4,4,shr(value,5)));
        TRY(rd(r,0x800529e8,0x800fc650,4,&entities));TRY(rd(r,0x800529f4,entities+index*4,4,&entity));TRY(rd(r,0x800529fc,entity+12,4,&value));TRY(wr(r,0x80052a08,ctx+8,4,shr(value,5)));
        TRY(rd(r,0x80052a10,0x800fc650,4,&entities));TRY(rd(r,0x80052a1c,entities+index*4,4,&entity));TRY(rd(r,0x80052a24,entity+16,4,&value));TRY(wr(r,0x80052a30,ctx+12,4,shr(value,5)));
        TRY(rd(r,0x80052a38,0x800fc650,4,&entities));TRY(rd(r,0x80052a44,entities+index*4,4,&entity));i32=index*32;i8=index*8;
        TRY(wr(r,0x80052a58,0x80103edc,4,sx16(angle)<<2));TRY(wp(r,0x80052a6c,0x8010292c,0x80103fd8+i32));
        TRY(wp(r,0x80052a80,0x800f9d04,0x800fb430+i8));TRY(wp(r,0x80052a94,0x800fea38,0x801028c8+i8));TRY(wp(r,0x80052aa8,0x800f0fb4,0x800faa04+i8));
        TRY(rd(r,0x80052ab4,entity+0xa8,2,&value));TRY(wp(r,0x80052acc,0x800f1c4c,0x800fb010+i32));TRY(wp(r,0x80052ae0,0x800f9cf8,0x800fb2e4+i32));
        TRY(wp(r,0x80052af4,0x800f9c54,0x800fb17c+i32));TRY(wp(r,0x80052b08,0x800fc62c,0x800fed20+i8));TRY(wp(r,0x80052b10,0x800f9d00,0x800fb480+i32));
        TRY(wr(r,0x80052b18,ctx+22,2,value));TRY(child(r,0x80052b1c,0x8005200c));TRY(child(r,0x80052b24,0x80055368));
        TRY(rd(r,0x80052b38,0x801029b0,4,&index));TRY(rd(r,0x80052b4c,0x800fea94+index*4,4,&xy));
        if(sx16(xy)+40<593&&sx16(xy>>16)+40<537){
            TRY(child(r,0x80052b90,0x800525ac));TRY(rd(r,0x80055f0c,0x1f80000c,4,&mask));TRY(rd(r,0x80052bb0,0x801029b0,4,&index));
            if(!(mask&(1u<<(index&31))))TRY(shadow(r));
        }
        TRY(indicator(r));TRY(rd(r,0x80052bdc,0x801029b0,4,&index));TRY(rd(r,0x80052be4,0x800f0ed4,4,&ctx));++index;ctx+=0xbcc;
        TRY(wr(r,0x80052bf4,0x801029b0,4,index));TRY(wp(r,0x80052c00,0x800f0ed4,ctx));++r->out->actors;
        /* Original loop reloads mutable index; do not replace it with a fixed
         * ten-element loop or hide wrapped/aliased input behind a clamp. */
    }while(s32(index)<10);
    return NBA97_BODY_OK;
}
static int begin(Nba97PlayerFrameContext* c,Nba97PlayerFrameProgress* p,Run* r){
    if(!c||!p||!c->access)return NBA97_BODY_ARGUMENT;
    memset(p,0,sizeof *p);r->in=c;r->out=p;r->link_status=NBA97_BODY_OK;return NBA97_BODY_OK;
}
static int finish(Run* r,int status){if(status==NBA97_BODY_OK){r->out->completed=1;r->out->stopped_pc=0;r->out->stopped_address=0;}return status;}
int nba97_game_player_frame(Nba97PlayerFrameContext* c,Nba97PlayerFrameProgress* p){Run r;TRY(begin(c,p,&r));return finish(&r,frame(&r));}
int nba97_game_player_shadow(Nba97PlayerFrameContext* c,Nba97PlayerFrameProgress* p){Run r;TRY(begin(c,p,&r));return finish(&r,shadow(&r));}
int nba97_game_player_indicator(Nba97PlayerFrameContext* c,Nba97PlayerFrameProgress* p){Run r;TRY(begin(c,p,&r));return finish(&r,indicator(&r));}
int nba97_game_player_copy40(Nba97PlayerFrameContext* c,uint32_t source,uint32_t dest,Nba97PlayerFrameProgress* p){Run r;TRY(begin(c,p,&r));return finish(&r,copy40(&r,source,dest));}
