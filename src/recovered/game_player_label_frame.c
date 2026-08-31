#include "game_player_label_frame.h"
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
static int wr(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t word){
    Nba97PlayerFrameValue v={0};v.word=word&bits(width);v.known_mask=(uint8_t)knowledge(width);
    return access(r,pc,address,width,NBA97_FRAME_WRITE,&v);
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
static int release_packets(Run* r,uint32_t packet,uint32_t count){
    uint32_t style,base,map,position,left,v;
    TRY(rd(r,0x8002f110,0x800b2048,4,&style));TRY(rd(r,0x8002f118,style+0x18,4,&base));
    left=shr(sx16(count)-1u,1);
    /* Original inverse/shift sequence is modulo32, not a host pointer
     * subtraction/division. The bitmap describes pairs of80-byte glyphs. */
    v=(packet-base)*3u;v+=v<<4;v+=v<<8;
    TRY(rd(r,0x8002f148,style+0x1c,4,&map));v+=v<<16;position=map+shr(0u-v,5);
    if(!left)return wr(r,0x8002f16c,position,1,0);
    if(s32(left)<0)return NBA97_BODY_OK;
    for(;;){
        TRY(wr(r,0x8002f178,position,1,0));++position;--left;
        if(left&0x8000u)break;
    }
    return NBA97_BODY_OK;
}
static int labels(Run* r){
    uint32_t saved_style,style,pool,heads,index,object,v,a,b,c,d,actor=0;
    uint32_t dx,dz,packets,other,count,current,depth,table;
    TRY(rd(r,0x80035bf8,0x800b2048,4,&saved_style));
    do {
        TRY(rd(r,0x80035c40,0x800b2048,4,&style));TRY(rd(r,0x80035c48,style+0x14,4,&heads));
        TRY(rd(r,0x80035c54,heads+0x1ec+actor*2,2,&index));index=sx16(index);
        while(s32(index)>=0){
            TRY(rd(r,0x80035c7c,0x800b2048,4,&style));TRY(rd(r,0x80035c84,style+0x10,4,&pool));object=pool+(index<<6);
            TRY(rd(r,0x80035c90,object+0x12,2,&v));++r->out->indicators;
            if(s32(sx16(v))<=0)goto retire;
            TRY(rd(r,0x80035ca8,object+0xe,2,&a));TRY(rd(r,0x80035cac,object+0x1e,2,&b));
            TRY(rd(r,0x80035cb0,object+0x10,2,&c));TRY(rd(r,0x80035cb4,object+0x20,2,&d));
            TRY(rd(r,0x80035cc0,0x80109afc,4,&v));dx=0xffffffecu-sx16(a)-sx16(b);dz=0xffffffecu-sx16(c)-sx16(d);
            if(v)goto translate;
            TRY(rd(r,0x80055f0c,0x1f80000c,4,&v));if(v&(1u<<actor))goto translate;
            TRY(rd(r,0x80035cf8,0x80021d84,1,&v));if(v==1)goto show;
            TRY(rd(r,0x80035d0c,0x800fdbcc,2,&a));if(actor==sx16(a))goto show;
            TRY(rd(r,0x80035d24,0x80020bec+actor*4,4,&a));TRY(rd(r,0x80035d2c,a+4,2,&a));if(!(a&0x8000))goto show;
            if(v==2){TRY(rd(r,actor<5?0x80035d54:0x80035d64,actor<5?0x8001ee36:0x8001eefa,2,&a));if(a)goto show;}
            TRY(rd(r,0x80035d78,0x800f9ffe,2,&a));if(!a)goto translate;
            TRY(rd(r,0x80035d90,0x80020bec+actor*4,4,&a));TRY(rd(r,0x80035d98,a,4,&a));TRY(rd(r,0x80035da0,0x800fa03c,4,&b));if(a!=b)goto translate;
show:
            TRY(rd(r,0x80035db0,object+0xe,2,&a));TRY(rd(r,0x80035db8,object+0x1e,2,&b));
            TRY(rd(r,0x80035dbc,object+0x10,2,&c));TRY(rd(r,0x80035dc0,object+0x20,2,&d));
            TRY(rd(r,0x80035dc4,0x800fea94+actor*4,2,&v));dx=sx16(v)-sx16(a)-sx16(b);
            TRY(rd(r,0x80035dcc,0x800fea96+actor*4,2,&v));dz=sx16(v)-sx16(c)-sx16(d)+3;
translate:
            TRY(rd(r,0x80035de0,object+0x1e,2,&a));TRY(rd(r,0x80035de4,object+0x20,2,&b));TRY(rd(r,0x80035dec,0x800b2048,4,&style));
            TRY(wr(r,0x80035df8,object+0x1e,2,a+dx));TRY(wr(r,0x80035dfc,object+0x20,2,b+dz));
            TRY(rd(r,0x80035e00,style+0x53,1,&v));TRY(rd(r,0x80035e04,object+0xc,2,&count));count=sx16(count);
            TRY(rd(r,0x80035e08,object+8,4,&packets));other=packets+(v^1u)*40u;packets+=v*40u;
            while(count){
                TRY(rd(r,0x80035e4c,other+8,2,&v));v+=dx;TRY(wr(r,0x80035e58,packets+24,2,v));TRY(wr(r,0x80035e5c,packets+8,2,v));
                TRY(rd(r,0x80035e60,other+16,2,&v));v+=dx;TRY(wr(r,0x80035e6c,packets+32,2,v));TRY(wr(r,0x80035e70,packets+16,2,v));
                TRY(rd(r,0x80035e74,other+10,2,&v));v+=dz;TRY(wr(r,0x80035e80,packets+18,2,v));TRY(wr(r,0x80035e84,packets+10,2,v));
                TRY(rd(r,0x80035e88,other+26,2,&v));v+=dz;current=packets;packets+=80;
                TRY(wr(r,0x80035e98,current+34,2,v));TRY(wr(r,0x80035e9c,current+26,2,v));
                TRY(rd(r,0x80035ea0,0x80106038+actor*4,4,&depth));TRY(rd(r,0x80035ea8,0x80102924,4,&table));
                --count;other+=80;TRY(link(r,table+((depth&4095u)<<2),current));
            }
            goto next_object;
retire:
            TRY(rd(r,0x80035ee0,object+8,4,&packets));TRY(rd(r,0x80035ee4,object+0xc,2,&count));
            /* The lifetime store is the source call delay slot and survives
             * packet-release refusal. No list repair runs on failure. */
            TRY(wr(r,0x80035ef0,object+0x12,2,65535));TRY(release_packets(r,packets,sx16(count)));++r->out->child_calls;
            TRY(rd(r,0x80035ef4,saved_style+0x3c,2,&v));
            if(index==sx16(v)){TRY(rd(r,0x80035f04,object+0x1c,2,&v));TRY(wr(r,0x80035f0c,saved_style+0x3c,2,v));}
            else {
                TRY(rd(r,0x80035f10,object+0x1a,2,&v));TRY(rd(r,0x80035f14,saved_style+0x10,4,&pool));TRY(rd(r,0x80035f18,object+0x1c,2,&a));
                TRY(wr(r,0x80035f24,pool+(sx16(v)<<6)+0x1c,2,a));
            }
            TRY(rd(r,0x80035f28,saved_style+0x3e,2,&v));
            if(index==sx16(v)){TRY(rd(r,0x80035f38,object+0x1a,2,&v));TRY(wr(r,0x80035f40,saved_style+0x3e,2,v));}
            else {
                TRY(rd(r,0x80035f44,object+0x1c,2,&v));TRY(rd(r,0x80035f48,saved_style+0x10,4,&pool));TRY(rd(r,0x80035f4c,object+0x1a,2,&a));
                TRY(wr(r,0x80035f58,pool+(sx16(v)<<6)+0x1a,2,a));
            }
            TRY(rd(r,0x80035f5c,saved_style+0x14,4,&heads));heads+=0x1ec+actor*2;TRY(rd(r,0x80035f68,heads,2,&v));
            if(index==sx16(v)){TRY(rd(r,0x80035f78,object+0x18,2,&v));TRY(wr(r,0x80035f80,heads,2,v));}
            else {
                TRY(rd(r,0x80035f84,object+0x16,2,&v));TRY(rd(r,0x80035f88,saved_style+0x10,4,&pool));TRY(rd(r,0x80035f8c,object+0x18,2,&a));
                TRY(wr(r,0x80035f98,pool+(sx16(v)<<6)+0x18,2,a));
            }
            TRY(rd(r,0x80035f9c,object+0x18,2,&v));
            if(!(v&0x8000)){TRY(rd(r,0x80035fac,saved_style+0x10,4,&pool));TRY(rd(r,0x80035fb0,object+0x16,2,&a));TRY(wr(r,0x80035fb8,pool+(v<<6)+0x16,2,a));}
next_object:
            TRY(rd(r,0x80035fbc,object+0x18,2,&index));index=sx16(index);
        }
        ++actor;++r->out->actors;
    }while(actor<10);
    return NBA97_BODY_OK;
}
static int begin(Run* r,Nba97PlayerFrameContext* c,Nba97PlayerFrameProgress* p){
    if(!c||!c->access||!p)return NBA97_BODY_ARGUMENT;
    memset(p,0,sizeof *p);r->in=c;r->out=p;r->link_status=NBA97_BODY_OK;return NBA97_BODY_OK;
}
static int finish(Run* r,int status){if(status==NBA97_BODY_OK){r->out->completed=1;r->out->stopped_pc=0;r->out->stopped_address=0;}return status;}
int nba97_game_player_label_frame(Nba97PlayerFrameContext* c,Nba97PlayerFrameProgress* p){Run r;int status;TRY(begin(&r,c,p));status=labels(&r);return finish(&r,status);}
int nba97_game_player_label_frame_release_packets(Nba97PlayerFrameContext* c,uint32_t packet,uint32_t count,Nba97PlayerFrameProgress* p){
    Run r;int status;TRY(begin(&r,c,p));status=release_packets(&r,packet,count);return finish(&r,status);
}
