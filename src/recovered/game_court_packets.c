#include "game_court_packets.h"
#include <string.h>

#define TRY(x) do { int status_=(x); if(status_!=NBA97_COURT_COMPLETE)return status_; } while(0)
typedef struct Run { Nba97CourtContext* context; Nba97CourtProgress* out; } Run;
static int32_t s32(uint32_t x){return x<0x80000000u?(int32_t)x:-1-(int32_t)~x;}
static uint32_t mask(unsigned width){return width==4?UINT32_MAX:((1u<<(width*8))-1u);}
static int reserve(Run* r,uint32_t pc,uint32_t address){
    r->out->stopped_pc=pc;r->out->stopped_address=address;
    if(r->out->operations==r->context->operation_budget)return NBA97_COURT_LIMIT;
    ++r->out->operations;return NBA97_COURT_COMPLETE;
}
static int access(Run* r,uint32_t pc,uint32_t address,unsigned width,int write,uint32_t* word){
    Nba97CourtValue value;int status;
    TRY(reserve(r,pc,address));
    if((width==4||width==3)?(address&3u):(width==2&&(address&1u)))return NBA97_COURT_ALIGNMENT;
    value.word=write?(*word&mask(width)):0;value.known=(uint8_t)write;
    status=r->context->access(r->context->user,pc,address,width,write,&value);
    if(status!=NBA97_COURT_COMPLETE)return status;
    if(value.known>1 || (!value.known&&value.word) || (value.word&~mask(width)))return NBA97_COURT_ARGUMENT;
    if(!value.known)return NBA97_COURT_UNKNOWN;
    if(write)++r->out->stores;else{*word=value.word;++r->out->reads;}
    return NBA97_COURT_COMPLETE;
}
static int read(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t* word){return access(r,pc,address,width,0,word);}
static int write(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t word){return access(r,pc,address,width,1,&word);}
static int math(Run* r,uint32_t pc,enum Nba97CourtMathKind kind,const uint32_t* input,unsigned index,uint32_t* word){
    Nba97CourtMathRequest request;Nba97CourtValue value={0,0};int status;
    TRY(reserve(r,pc,0));if(!r->context->math)return NBA97_COURT_MATH_REQUIRED;
    memset(&request,0,sizeof request);request.kind=kind;request.pc=pc;request.index=index;
    if(input)request.word=*input;
    status=r->context->math(r->context->user,&request,&value);
    if(status!=NBA97_COURT_COMPLETE)return status;
    ++r->out->math_operations;
    if(word){if(value.known>1||(!value.known&&value.word))return NBA97_COURT_ARGUMENT;
        if(!value.known)return NBA97_COURT_UNKNOWN;*word=value.word;}
    return NBA97_COURT_COMPLETE;
}
static int screen_store(Run* r,uint32_t pc,uint32_t packet,unsigned offset,unsigned fifo){
    uint32_t value;TRY(math(r,pc,NBA97_COURT_SCREEN,0,fifo,&value));return write(r,pc,packet+offset,4,value);
}
static int begin(Nba97CourtContext* c,Nba97CourtProgress* out,Run* r){
    if(!c||!c->access||!out)return NBA97_COURT_ARGUMENT;
    memset(out,0,sizeof *out);r->context=c;r->out=out;return NBA97_COURT_COMPLETE;
}
static int complete(Run* r){r->out->completed=1;r->out->stopped_pc=0;r->out->stopped_address=0;return NBA97_COURT_COMPLETE;}
static int link(Run* r,uint32_t table,uint32_t packet){
    uint32_t old;TRY(read(r,0x80056914,table,3,&old));
    TRY(write(r,0x8005691c,packet,3,old));TRY(write(r,0x80056924,table,3,packet));
    ++r->out->linked;return NBA97_COURT_COMPLETE;
}
int nba97_game_court_link(Nba97CourtContext* c,uint32_t table,uint32_t packet,Nba97CourtProgress* out){
    Run r;TRY(begin(c,out,&r));TRY(link(&r,table,packet));return complete(&r);
}
static int fixed_textured(Run* r,uint32_t vertices,uint32_t packet,uint32_t table,uint32_t count){
    uint32_t next[6],last[2],clip,leading,old;unsigned i;
    count-=1;
    for(i=0;i<6;++i)TRY(read(r,0x80054d7cu+i*4u,vertices+i*4u,4,&next[i]));
    do {
        for(i=0;i<6;++i)TRY(math(r,0x80054d94u+i*4u,NBA97_COURT_LOAD_VERTEX_WORD,&next[i],i,0));
        TRY(read(r,0x80054dac,vertices+24u,4,&last[0]));
        TRY(math(r,0x80054db0,NBA97_COURT_PROJECT_THREE,0,0,0));
        TRY(read(r,0x80054db4,vertices+28u,4,&last[1]));vertices+=32;
        /* Original software pipeline intentionally prefetches beyond the final
         * quad. Require actual enclosing ownership; do not append zero padding. */
        for(i=0;i<4;++i)TRY(read(r,0x80054dbcu+i*4u,vertices+i*4u,4,&next[i]));
        TRY(math(r,0x80054dcc,NBA97_COURT_NORMAL_CLIP,0,0,&clip));
        TRY(read(r,0x80054dd0,vertices+16u,4,&next[4]));
        TRY(read(r,0x80054dd4,vertices+20u,4,&next[5]));
        TRY(math(r,0x80054dd8,NBA97_COURT_LOAD_VERTEX_WORD,&last[0],0,0));
        TRY(math(r,0x80054ddc,NBA97_COURT_LOAD_VERTEX_WORD,&last[1],1,0));
        /* This really is MFC2 data31, not CFC2 FLAG. Preserve that distinction
         * even if a replacement "clip flags" check might look more sensible. */
        TRY(math(r,0x80054de4,NBA97_COURT_LEADING_BITS,0,0,&leading));count-=1;
        if(s32(clip)>0&&s32(leading)>=0){
            TRY(screen_store(r,0x80054df8,packet,8,0));
            TRY(screen_store(r,0x80054dfc,packet,16,1));
            TRY(screen_store(r,0x80054e00,packet,32,2));
            TRY(math(r,0x80054e08,NBA97_COURT_PROJECT_ONE,0,0,0));
            TRY(read(r,0x80054e0c,table,3,&old));
            /* 54D4C reverses56914's stores: table FIRST, then packet. */
            TRY(write(r,0x80054e14,table,3,packet));
            TRY(write(r,0x80054e18,packet,3,old));
            TRY(screen_store(r,0x80054e1c,packet,24,2));++r->out->linked;
        }
        packet+=40;++r->out->quads;
    }while(s32(count)>=0);
    return NBA97_COURT_COMPLETE;
}
static int depth_textured(Run* r,uint32_t vertices,uint32_t packet,uint32_t table,uint32_t count,uint32_t depth_mask){
    uint32_t points[6],last[2],clip,leading,depth,old,slot;unsigned i;count-=1;
    do {
        for(i=0;i<6;++i){TRY(read(r,0x80054ee8u+i*4u,vertices+i*4u,4,&points[i]));
            TRY(math(r,0x80054ee8u+i*4u,NBA97_COURT_LOAD_VERTEX_WORD,&points[i],i,0));}
        TRY(math(r,0x80054f04,NBA97_COURT_PROJECT_THREE,0,0,0));
        TRY(math(r,0x80054f0c,NBA97_COURT_NORMAL_CLIP,0,0,&clip));
        TRY(math(r,0x80054f18,NBA97_COURT_LEADING_BITS,0,0,&leading));
        if(s32(clip)>0&&s32(leading)>=0){
            TRY(screen_store(r,0x80054f2c,packet,8,0));
            TRY(screen_store(r,0x80054f30,packet,16,1));
            TRY(screen_store(r,0x80054f34,packet,32,2));
            TRY(read(r,0x80054f38,vertices+24u,4,&last[0]));
            TRY(math(r,0x80054f38,NBA97_COURT_LOAD_VERTEX_WORD,&last[0],0,0));
            TRY(read(r,0x80054f3c,vertices+28u,4,&last[1]));
            TRY(math(r,0x80054f3c,NBA97_COURT_LOAD_VERTEX_WORD,&last[1],1,0));
            TRY(math(r,0x80054f44,NBA97_COURT_PROJECT_ONE,0,0,0));
            TRY(screen_store(r,0x80054f4c,packet,24,2));
            TRY(math(r,0x80054f50,NBA97_COURT_AVERAGE_FOUR,0,0,0));
            TRY(math(r,0x80054f54,NBA97_COURT_ORDER_DEPTH,0,0,&depth));
            slot=table+((depth&depth_mask)<<2);
            TRY(read(r,0x80054f68,slot,4,&old));
            TRY(write(r,0x80054f78,packet,4,0x09000000u|(old&0xffffffu)));
            TRY(write(r,0x80054f80,slot,4,packet&0xffffffu));++r->out->linked;
        }
        vertices+=32;packet+=40;count-=1;++r->out->quads;
    }while(s32(count)>=0);
    return NBA97_COURT_COMPLETE;
}
static int fixed_flat(Run* r,uint32_t vertices,uint32_t packet,uint32_t table,uint32_t count){
    uint32_t points[6],last[2],leading,old;unsigned i;count-=1;
    do {
        for(i=0;i<6;++i){TRY(read(r,0x80054e5cu+i*4u,vertices+i*4u,4,&points[i]));
            TRY(math(r,0x80054e5cu+i*4u,NBA97_COURT_LOAD_VERTEX_WORD,&points[i],i,0));}
        TRY(math(r,0x80054e78,NBA97_COURT_PROJECT_THREE,0,0,0));
        TRY(read(r,0x80054e7c,vertices+24u,4,&last[0]));
        TRY(math(r,0x80054e7c,NBA97_COURT_LOAD_VERTEX_WORD,&last[0],0,0));
        TRY(read(r,0x80054e80,vertices+28u,4,&last[1]));
        TRY(math(r,0x80054e80,NBA97_COURT_LOAD_VERTEX_WORD,&last[1],1,0));
        TRY(screen_store(r,0x80054e84,packet,20,2));
        TRY(screen_store(r,0x80054e88,packet,12,1));
        TRY(screen_store(r,0x80054e8c,packet,8,0));
        TRY(math(r,0x80054e90,NBA97_COURT_LEADING_BITS,0,0,&leading));
        TRY(read(r,0x80054e94,table,4,&old));
        if(s32(leading)>=0){
            TRY(math(r,0x80054ea0,NBA97_COURT_PROJECT_ONE,0,0,0));
            TRY(write(r,0x80054eac,packet,4,0x05000000u|(old&0xffffffu)));
            TRY(write(r,0x80054eb4,table,4,packet&0xffffffu));
            TRY(screen_store(r,0x80054eb8,packet,16,2));++r->out->linked;
        }
        vertices+=32;packet+=24;count-=1;++r->out->quads;
    }while(s32(count)>=0);
    return NBA97_COURT_COMPLETE;
}
int nba97_game_court_packets(Nba97CourtContext* c,enum Nba97CourtPacketPass pass,
    uint32_t vertices,uint32_t packet,uint32_t table,uint32_t count,uint32_t depth_mask,Nba97CourtProgress* out){
    Run r;if(pass<NBA97_COURT_FIXED_TEXTURED_54D4C||pass>NBA97_COURT_FIXED_FLAT_54E50)return NBA97_COURT_ARGUMENT;
    TRY(begin(c,out,&r));
    if(pass==NBA97_COURT_FIXED_TEXTURED_54D4C)TRY(fixed_textured(&r,vertices,packet,table,count));
    else if(pass==NBA97_COURT_DEPTH_TEXTURED_54ED8)TRY(depth_textured(&r,vertices,packet,table,count,depth_mask));
    else TRY(fixed_flat(&r,vertices,packet,table,count));
    return complete(&r);
}

static int camera_load(Run* r){
    uint32_t word[5];unsigned i;
    for(i=0;i<5;++i)TRY(read(r,0x80055f18u+i*4u,0x800f9fd8u+i*4u,4,&word[i]));
    for(i=0;i<5;++i)TRY(math(r,i==4?0x80055f40u:0x80055f2cu+i*4u,NBA97_COURT_LOAD_ROTATION_WORD,&word[i],i,0));
    for(i=0;i<3;++i)TRY(read(r,0x80055f44u+i*4u,0x800f9fecu+i*4u,4,&word[i]));
    for(i=0;i<3;++i)TRY(math(r,i==2?0x80055f5cu:0x80055f50u+i*4u,NBA97_COURT_LOAD_TRANSLATION_WORD,&word[i],i,0));
    return NBA97_COURT_COMPLETE;
}
static int visibility(Run* r,uint32_t group,unsigned phase,int* draw){
    static const uint32_t pc[3][6]={
        {0x8004ad0c,0x8004ad24,0x8004ad38,0x8004ad54,0x8004ad68,0x8004ad7c},
        {0x8004ae30,0x8004ae44,0x8004ae58,0x8004ae74,0x8004ae88,0x8004ae9c},
        {0x8004af60,0x8004af78,0x8004af8c,0x8004afa8,0x8004afbc,0x8004afd0}};
    uint32_t special,selected=0,bits,bank;
    *draw=0;TRY(read(r,pc[phase][0],0x800febec,4,&special));
    if(special)TRY(read(r,pc[phase][1],0x800fe9cc,4,&selected));
    if(!special||selected!=group){
        TRY(read(r,pc[phase][2],0x800fcc54,4,&bits));
        if(bits&(1u<<(group&31))){*draw=1;return NBA97_COURT_COMPLETE;}
    }
    /* The source rereads these globals even if the previous values are known.
     * Keep that ordering when packet/resource storage aliases live state. */
    TRY(read(r,pc[phase][3],0x800febec,4,&special));if(!special)return NBA97_COURT_COMPLETE;
    TRY(read(r,pc[phase][4],0x800fe9cc,4,&selected));if(selected!=group)return NBA97_COURT_COMPLETE;
    TRY(read(r,pc[phase][5],0x8001ede8,4,&bank));*draw=bank!=0;
    return NBA97_COURT_COMPLETE;
}
int nba97_game_court_frame(Nba97CourtContext* c,Nba97CourtProgress* out){
    Run run;Run* r=&run;uint32_t root,extra,vertices,tex,flat,count,index,group=0;
    uint32_t record,bucket,table,bank,packet,n,limit_,line_table,line,line_end,xy,source;
    int draw;
    TRY(begin(c,out,r));TRY(camera_load(r));
    TRY(read(r,0x8004acb0,0x800febe4,4,&root));
    TRY(read(r,0x8004acb8,0x800dcf10,4,&extra));
    TRY(read(r,0x8004acbc,root+8,4,&vertices));
    TRY(read(r,0x8004acc0,root+12,4,&tex));
    TRY(read(r,0x8004acc4,root+16,4,&flat));
    TRY(read(r,0x8004acc8,root,4,&count));limit_=count-(extra+10u);
    TRY(write(r,0x8004acdc,0x80102c84,4,tex));TRY(write(r,0x8004ace4,0x800fc964,4,flat));
    index=0;
    if(s32(limit_)>0)do{
        TRY(read(r,0x8004acf4,0x80102c84,4,&record));TRY(read(r,0x8004acfc,record,4,&bucket));
        TRY(read(r,0x8004ad04,0x801046d8,4,&table));table+=124u-(bucket<<2);
        TRY(visibility(r,group,0,&draw));
        if(draw){
            TRY(read(r,0x8004ad90,0x8001ede8,4,&bank));TRY(read(r,0x8004ad98,0x80102c84,4,&record));
            TRY(read(r,0x8004ada4,record+(bank<<2)+8,4,&packet));TRY(read(r,0x8004ada8,record+4,4,&n));
            TRY(fixed_textured(r,vertices,packet,table,n));
        }
        TRY(read(r,0x8004adbc,0x80102c84,4,&record));TRY(read(r,0x8004adc4,0x800febe4,4,&root));
        TRY(read(r,0x8004adcc,0x800dcf10,4,&extra));TRY(read(r,0x8004add0,record+4,4,&n));
        ++index;++group;TRY(write(r,0x8004ade4,0x80102c84,4,record+16));
        TRY(read(r,0x8004ade8,root,4,&count));limit_=count-(extra+10u);vertices+=n<<5;
    }while(s32(index)<s32(limit_));
    TRY(read(r,0x8004ae08,0x800febe4,4,&root));TRY(read(r,0x8004ae10,0x800dcf10,4,&extra));
    TRY(read(r,0x8004ae14,root,4,&count));index=count-(extra+10u);
    if(s32(index)<s32(count))do{
        TRY(visibility(r,group,1,&draw));
        if(draw){
            TRY(read(r,0x8004aeb0,0x8001ede8,4,&bank));TRY(read(r,0x8004aeb8,0x80102c84,4,&record));
            TRY(read(r,0x8004aec0,0x80102924,4,&table));TRY(read(r,0x8004aecc,record+(bank<<2)+8,4,&packet));
            TRY(read(r,0x8004aed8,record+4,4,&n));TRY(depth_textured(r,vertices,packet,table,n,0xfff));
        }
        TRY(read(r,0x8004aee8,0x80102c84,4,&record));TRY(read(r,0x8004aef0,0x800febe4,4,&root));
        TRY(read(r,0x8004aef4,record+4,4,&n));TRY(read(r,0x8004aef8,root,4,&count));
        ++index;++group;TRY(write(r,0x8004af0c,0x80102c84,4,record+16));vertices+=n<<5;
    }while(s32(index)<s32(count));
    TRY(read(r,0x8004af24,0x800febe4,4,&root));TRY(read(r,0x8004af2c,root+4,4,&count));index=0;
    if(s32(count)>0)do{
        TRY(read(r,0x8004af48,0x800fc964,4,&record));TRY(read(r,0x8004af50,record,4,&bucket));
        TRY(read(r,0x8004af58,0x801046d8,4,&table));table+=124u-(bucket<<2);
        TRY(visibility(r,group,2,&draw));
        if(draw){
            TRY(read(r,0x8004afe4,0x8001ede8,4,&bank));TRY(read(r,0x8004afec,0x800fc964,4,&record));
            TRY(read(r,0x8004aff8,0x800feda0+(bank<<2),4,&line_table));
            TRY(read(r,0x8004b004,record+(bank<<2)+8,4,&packet));
            /* The fifth argument is unused by54E50, but the caller still
             * consumes its pointer. Do not omit this observable source read. */
            TRY(read(r,0x8004b00c,line_table+(index<<2),4,&line));TRY(read(r,0x8004b018,record+4,4,&n));
            TRY(fixed_flat(r,vertices,packet,table,n));
            TRY(read(r,0x8004b02c,0x8001ede8,4,&bank));TRY(read(r,0x8004b034,0x800fc964,4,&record));
            TRY(read(r,0x8004b040,0x800feda0+(bank<<2),4,&line_table));
            TRY(read(r,0x8004b048,record+4,4,&n));TRY(read(r,0x8004b04c,record+(bank<<2)+8,4,&packet));
            TRY(read(r,0x8004b054,line_table+(index<<2),4,&line));
            line_end=line+14;source=packet+22;limit_=0;
            if(s32(n)>0)do{
                TRY(read(r,0x8004b068,source-14,2,&xy));TRY(write(r,0x8004b070,line_end-6,2,xy));
                TRY(read(r,0x8004b074,source-12,2,&xy));TRY(write(r,0x8004b07c,line_end-4,2,xy));
                TRY(read(r,0x8004b080,source-2,2,&xy));TRY(write(r,0x8004b088,line_end-2,2,xy));
                TRY(read(r,0x8004b08c,source,2,&xy));TRY(write(r,0x8004b09c,line_end,2,xy));
                TRY(link(r,table,line));TRY(read(r,0x8004b0a4,0x800fc964,4,&record));
                ++limit_;TRY(read(r,0x8004b0ac,record+4,4,&n));line+=16;source+=24;line_end+=16;
            }while(s32(limit_)<s32(n));
        }
        TRY(read(r,0x8004b0c8,0x800fc964,4,&record));TRY(read(r,0x8004b0d0,0x800febe4,4,&root));
        TRY(read(r,0x8004b0d4,record+4,4,&n));TRY(read(r,0x8004b0d8,root+4,4,&count));
        ++index;++group;TRY(write(r,0x8004b0ec,0x800fc964,4,record+16));vertices+=n<<5;
    }while(s32(index)<s32(count));
    TRY(read(r,0x8004b104,0x800dcf10,4,&extra));
    if(extra){
        TRY(read(r,0x8004b118,0x801046d8,4,&table));TRY(read(r,0x8004b120,0x8001ede8,4,&bank));
        TRY(link(r,table+124,0x801041a4+bank*72u));
        TRY(read(r,0x8004b148,0x801046d8,4,&table));TRY(read(r,0x8004b150,0x8001ede8,4,&bank));
        TRY(link(r,table+124,0x801041c8+bank*72u));
    }
    return complete(r);
}
