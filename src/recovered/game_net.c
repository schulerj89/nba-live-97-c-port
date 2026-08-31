#include "game_net.h"
#include "game_court_packets.h"
#include <string.h>
typedef struct Run {Nba97PlayerFrameContext* in;Nba97GameNetProgress* out;int link_status;} Run;
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
static int wp(Run* r,uint32_t pc,uint32_t address,uint32_t word){
    Nba97PlayerFrameValue v={0};v.word=word;v.known_mask=15;return access(r,pc,address,4,NBA97_FRAME_WRITE_POINTER,&v);
}
static int byte_copy(Run* r,uint32_t readpc,uint32_t writepc,uint32_t source,uint32_t destination){
    Nba97PlayerFrameValue v={0};TRY(access(r,readpc,source,1,NBA97_FRAME_READ,&v));
    return access(r,writepc,destination,1,NBA97_FRAME_WRITE,&v);
}
static int declared(Run* r,uint32_t source,const uint32_t pcs[3],Nba97GamePeriodValue* out){
    unsigned i;uint32_t word=0;int known=1;
    for(i=0;i<3;++i){Nba97PlayerFrameValue v={0};TRY(access(r,pcs[i],source+i,1,NBA97_FRAME_READ,&v));word=(word<<8)|v.word;known=known&&v.known_mask==1;}
    out->known=(uint8_t)known;out->word=known?word:0;return NBA97_BODY_OK;
}
static int decode(Run* r,uint32_t source,uint32_t destination,Nba97GamePeriodValue* length){
    static const uint32_t headerpc[3]={0x800a9fe4,0x800a9ff4,0x800aa01c};
    static const uint32_t sizepc[3]={0x800aa194,0x800aa19c,0x800aa1a4};
    uint32_t v,tag,command,b1,b2,b3,back,count,i,src=source;Nba97GamePeriodValue ignored;
    TRY(declared(r,source+2,headerpc,&ignored));length->word=0;length->known=1;
    TRY(rd(r,0x800a4678,source+1,1,&tag));if(tag!=0xfb&&tag!=0x32)return NBA97_BODY_OK;
    TRY(rd(r,0x800a4698,source,1,&v));v=(v&0xfe)-0x10;
    if(v>=59)return NBA97_BODY_OK;
    TRY(rd(r,0x800a46bc,0x800288b4+v*4,4,&v));
    if(v==0x800a4720)return NBA97_BODY_OK;
    if(v!=0x800a46cc){TRY(reserve(r,v==0x800a46e4?0x800a46ec:v==0x800a46fc?0x800a4700:v==0x800a4710?0x800a4714:0x800a46c4,v));return NBA97_NET_CODEC_REQUIRED;}
    /* AA168 tests the optional compressed-size flag but does not check the
     * returned output length against its writes. Preserve command termination. */
    TRY(rd(r,0x800aa178,src,1,&v));src+=2;if(v&1)src+=3;
    TRY(declared(r,src,sizepc,length));src+=3;
    for(;;){
        TRY(rd(r,0x800aa1c0,src,1,&command));++src;
        if(command<0x80){
            TRY(rd(r,0x800aa1d4,src++,1,&b1));count=command&3;
            for(i=0;i<count;++i){TRY(byte_copy(r,0x800aa1e4,0x800aa1f0,src++,destination));++destination;}
            back=destination-1-((command&0x60)<<3)-b1;
            TRY(byte_copy(r,0x800aa210,0x800aa218,back++,destination));++destination;
            TRY(byte_copy(r,0x800aa224,0x800aa234,back++,destination));++destination;
            count=((command&0x1c)>>2)+1;
            for(i=0;i<count;++i){TRY(byte_copy(r,0x800aa23c,0x800aa248,back++,destination));++destination;}
        }else if(command<0xc0){
            TRY(rd(r,0x800aa268,src++,1,&b1));TRY(rd(r,0x800aa270,src++,1,&b2));count=b1>>6;
            for(i=0;i<count;++i){TRY(byte_copy(r,0x800aa280,0x800aa28c,src++,destination));++destination;}
            back=destination-1-((b1&63)<<8)-b2;
            TRY(byte_copy(r,0x800aa2ac,0x800aa2b4,back++,destination));++destination;
            TRY(byte_copy(r,0x800aa2b8,0x800aa2c4,back++,destination));++destination;
            TRY(byte_copy(r,0x800aa2cc,0x800aa2dc,back++,destination));++destination;
            count=(command&63)+1;
            for(i=0;i<count;++i){TRY(byte_copy(r,0x800aa2e4,0x800aa2f0,back++,destination));++destination;}
        }else if(command<0xe0){
            TRY(rd(r,0x800aa310,src++,1,&b1));TRY(rd(r,0x800aa318,src++,1,&b2));TRY(rd(r,0x800aa320,src++,1,&b3));count=command&3;
            for(i=0;i<count;++i){TRY(byte_copy(r,0x800aa32c,0x800aa338,src++,destination));++destination;}
            back=destination-1-((command&0x10)<<12)-(b1<<8)-b2;
            TRY(byte_copy(r,0x800aa364,0x800aa36c,back++,destination));++destination;
            TRY(byte_copy(r,0x800aa370,0x800aa37c,back++,destination));++destination;
            TRY(byte_copy(r,0x800aa380,0x800aa38c,back++,destination));++destination;
            TRY(byte_copy(r,0x800aa39c,0x800aa3ac,back++,destination));++destination;
            count=((command&12)<<6)+b3+1;
            for(i=0;i<count;++i){TRY(byte_copy(r,0x800aa3b4,0x800aa3c0,back++,destination));++destination;}
        }else if(command<0xfc){
            static const uint32_t rp[4]={0x800aa3e4,0x800aa3f0,0x800aa400,0x800aa410};
            static const uint32_t wp_[4]={0x800aa3ec,0x800aa3fc,0x800aa40c,0x800aa420};
            count=((command&31)+1)*4;
            for(i=0;i<count;++i){TRY(byte_copy(r,rp[i&3],wp_[i&3],src++,destination));++destination;}
        }else{
            count=command&3;for(i=0;i<count;++i){TRY(byte_copy(r,0x800aa440,0x800aa44c,src++,destination));++destination;}
            ++r->out->decodes;return NBA97_BODY_OK;
        }
    }
}
static int decompress(Run* r,uint32_t source,uint32_t destination){Nba97GamePeriodValue unused;return decode(r,source,destination,&unused);}
static int packet_tag(Run* r,uint32_t packet,int quad){
    TRY(wr(r,quad?0x8009c318:0x8009c3f4,packet+3,1,quad?5:3));
    return wr(r,quad?0x8009c324:0x8009c400,packet+7,1,quad?0x28:0x40);
}
static int opaque(Run* r,uint32_t packet){uint32_t v;TRY(rd(r,0x8009c288,packet+7,1,&v));return wr(r,0x8009c298,packet+7,1,v&0xfd);}
static int initialize(Run* r){
    uint32_t resource,v,p;unsigned i,side,bank;
    TRY(rd(r,0x8004b6c8,0x80103f44,4,&resource));
    /*4B714 relocates the live source table IN PLACE. Calling initialization
     * again adds the base again; there is no source already-relocated guard. */
    for(i=0;i<30;++i){TRY(rd(r,0x8004b700,resource+i*4,4,&v));TRY(wp(r,0x8004b714,resource+i*4,v+resource));}
    for(i=0;i<30;++i){TRY(rd(r,0x8004b734,resource+i*4,4,&v));TRY(wp(r,0x8004b740,0x80102938+i*4,v));}
    for(side=0;side<2;++side)for(bank=0;bank<2;++bank){
        for(i=0;i<15;++i){
            p=0x801064a4+side*0x1324+bank*0x2d0+i*48;
            TRY(packet_tag(r,p+32,0));TRY(wr(r,0x8004b7b8,p+36,1,214));TRY(wr(r,0x8004b7c0,p+37,1,115));TRY(wr(r,0x8004b7cc,p+38,1,82));TRY(opaque(r,p+32));
            TRY(packet_tag(r,p,0));TRY(wr(r,0x8004b7e8,p+5,1,99));TRY(wr(r,0x8004b7f0,p+4,1,181));TRY(wr(r,0x8004b7f8,p+6,1,66));TRY(opaque(r,p));
            TRY(packet_tag(r,p+16,0));TRY(wr(r,0x8004b814,p+20,1,140));TRY(wr(r,0x8004b81c,p+21,1,74));TRY(wr(r,0x8004b828,p+22,1,49));TRY(opaque(r,p+16));
        }
        for(i=0;i<2;++i){p=0x80106444+side*0x1324+bank*48+i*24;TRY(packet_tag(r,p,1));
            TRY(wr(r,0x8004b864,p+5,1,99));TRY(wr(r,0x8004b86c,p+4,1,181));TRY(wr(r,0x8004b874,p+6,1,66));TRY(opaque(r,p));}
        for(i=0;i<100;++i){p=0x80106a44+side*0x1324+bank*1600+i*16;TRY(packet_tag(r,p,0));
            TRY(wr(r,0x8004b8b8,p+4,1,181));TRY(wr(r,0x8004b8bc,p+5,1,181));TRY(wr(r,0x8004b8c4,p+6,1,181));TRY(opaque(r,p));}
    }
    for(i=0;i<15;++i){
        TRY(rd(r,0x8004b950,0x800b731c+i*8,2,&v));TRY(wr(r,0x8004b968,0x801063cc+i*8,2,0xac0-v));
        TRY(rd(r,0x8004b96c,0x800b731e + i*8,2,&v));TRY(wr(r,0x8004b980,0x801063ce + i*8,2,v+0x280));
        TRY(rd(r,0x8004b984,0x800b7320+i*8,2,&v));TRY(wr(r,0x8004b990,0x801063d0+i*8,2,v));
        TRY(rd(r,0x8004b994,0x800b731c+i*8,2,&v));TRY(wr(r,0x8004b9a8,0x801076f0+i*8,2,v-0xac0));
        TRY(rd(r,0x8004b9ac,0x800b731e + i*8,2,&v));TRY(wr(r,0x8004b9c0,0x801076f2+i*8,2,v+0x280));
        TRY(rd(r,0x8004b9c4,0x800b7320+i*8,2,&v));TRY(wr(r,0x8004b9dc,0x801076f4+i*8,2,v));
    }
    TRY(rd(r,0x8004b9f0,0x800b7a00,4,&v));TRY(rd(r,0x8004ba04,0x80102938+(v<<2),4,&v));TRY(decompress(r,v,0x800fb998));
    TRY(rd(r,0x8004ba1c,0x800b7a04,4,&v));TRY(rd(r,0x8004ba30,0x80102938+(v<<2),4,&v));TRY(decompress(r,v,0x800fbfd8));
    TRY(wp(r,0x8004ba44,0x801076ec,0x800fb998));TRY(wp(r,0x8004ba4c,0x80108a10,0x800fbfd8));
    ++r->out->initializations;return NBA97_BODY_OK;
}
static int paused(Run* r,uint32_t pc,uint32_t* value){uint32_t p;TRY(rd(r,pc,0x800fc660,4,&p));return rd(r,pc+8,p,2,value);}
static int advance(Run* r){
    uint32_t v,p,frame,x,y;unsigned side,i;
    for(side=0;side<2;++side){
        uint32_t d=side*252,slot=0x800b7a00+side*4,destination=0x800fb998+side*1600;
        TRY(rd(r,0x8004b1e8+d,slot,4,&frame));
        if(frame){
            TRY(paused(r,0x8004b1fc+d,&p));
            if(!p){
                /* Signed remainder, with ADDIU wrap BEFORE division. Negative
                 * malformed timers are not normalized into the30-frame table. */
                frame=(uint32_t)(s32(frame+1)%30);TRY(wr(r,0x8004b244+d,slot,4,frame));
            }
            TRY(rd(r,0x8004b24c+d,slot,4,&frame));TRY(rd(r,0x8004b260+d,0x80102938+(frame<<2),4,&v));TRY(decompress(r,v,destination));
            TRY(rd(r,0x8004b274+d,slot,4,&v));if(!v)TRY(wr(r,side?0x8004b384:0x8004b288,side?0x80108a0a:0x801076e6,2,side?0:0x800));
        }else{
            TRY(paused(r,0x8004b298+d,&p));if(p){TRY(rd(r,0x8004b2b4+d,slot,4,&frame));TRY(rd(r,0x8004b2c8+d,0x80102938+(frame<<2),4,&v));TRY(decompress(r,v,destination));}
        }
    }
    for(side=0;side<2;++side){
        uint32_t d=side*364,slot=0x800b7a08+side*4,frameslot=0x800b7a00+side*4;
        uint32_t destination=0x800fb998+side*1600,rim=0x801063ce + side*0x1324;
        TRY(rd(r,0x8004b3dc+d,slot,4,&v));
        if(v){
            TRY(rd(r,0x8004b3f0+d,frameslot,4,&frame));if(!frame){TRY(rd(r,0x8004b408+d,0x80102938+(frame<<2),4,&v));TRY(decompress(r,v,destination));}
            for(i=0;i<200;++i){TRY(rd(r,0x8004b434+d,destination+i*8,2,&x));TRY(rd(r,0x8004b438+d,destination+i*8+2,2,&y));
                TRY(wr(r,0x8004b448+d,destination+i*8+2,2,y-shr(sx16(x)+80,2)));}
            TRY(rd(r,0x8004b464+d,rim+80,2,&v));
            if(sx16(v)==0x280){for(i=0;i<15;++i){TRY(rd(r,0x8004b488+d,0x800b731c+i*8,2,&x));TRY(rd(r,0x8004b48c+d,rim+i*8,2,&y));TRY(wr(r,0x8004b49c+d,rim+i*8,2,y-shr(x<<16,18)));}}
            TRY(paused(r,0x8004b4b4+d,&p));if(!p){TRY(rd(r,0x8004b4d4+d,slot,4,&v));TRY(wr(r,0x8004b4e4+d,slot,4,v-1));}
        }else{
            TRY(rd(r,0x8004b4ec+d,rim+80,2,&v));if(sx16(v)!=0x280){
                for(i=0;i<15;++i)TRY(wr(r,0x8004b508+d,rim+112-i*8,2,0x280));
                TRY(rd(r,0x8004b51c+d,frameslot,4,&v));TRY(rd(r,0x8004b530+d,0x80102938+(v<<2),4,&v));TRY(decompress(r,v,destination));
            }
        }
    }
    return NBA97_BODY_OK;
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
static int low(Run* r,uint32_t pc,uint32_t address,uint32_t* value){
    Nba97PlayerFrameValue v={0};TRY(access(r,pc,address,4,NBA97_FRAME_READ,&v));
    if((v.known_mask&3)!=3)return NBA97_BODY_UNKNOWN;
    *value=v.word&65535;return NBA97_BODY_OK;
}
static int camera(Run* r){
    uint32_t words[5],v;unsigned i;
    for(i=0;i<5;++i){if(i==4)TRY(low(r,0x80055f28,0x800f9fe8,&words[i]));else TRY(rd(r,0x80055f18+i*4,0x800f9fd8+i*4,4,&words[i]));}
    for(i=0;i<5;++i)TRY(math(r,i==4?0x80055f40:0x80055f2c+i*4,NBA97_PROJECTION_ROTATION,i,words[i],0));
    for(i=0;i<3;++i)TRY(rd(r,0x80055f44+i*4,0x800f9fec+i*4,4,&words[i]));
    for(i=0;i<3;++i){v=words[i];TRY(math(r,i==2?0x80055f5c:0x80055f50+i*4,NBA97_PROJECTION_TRANSLATION,i,v,0));}
    return NBA97_BODY_OK;
}
static int project4(Run* r,const uint32_t vertex[4],const uint32_t output[4],uint32_t* depth){
    uint32_t v;unsigned i;
    for(i=0;i<6;++i){if(i&1)TRY(low(r,0x80055f60+i*4,vertex[i/2]+4,&v));else TRY(rd(r,0x80055f60+i*4,vertex[i/2],4,&v));
        TRY(math(r,0x80055f60+i*4,NBA97_PROJECTION_VERTEX,i,v,0));}
    TRY(math(r,0x80055f7c,NBA97_PROJECTION_THREE,0,0,0));
    for(i=0;i<3;++i){TRY(math(r,0x80055f8c+i*4,NBA97_PROJECTION_SCREEN,i,0,&v));TRY(wr(r,0x80055f8c+i*4,output[i],4,v));}
    TRY(rd(r,0x80055f9c,vertex[3],4,&v));TRY(math(r,0x80055f9c,NBA97_PROJECTION_VERTEX,0,v,0));
    TRY(low(r,0x80055fa0,vertex[3]+4,&v));TRY(math(r,0x80055fa0,NBA97_PROJECTION_VERTEX,1,v,0));
    TRY(math(r,0x80055fa8,NBA97_FRAME_PROJECT_ONE,0,0,0));
    TRY(math(r,0x80055fb8,NBA97_PROJECTION_SCREEN,2,0,&v));TRY(wr(r,0x80055fb8,output[3],4,v));
    /* FLAG/IR0 output addresses are private caller-stack words, not render
     * culling. Their dead reads/stores do not change retained geometry. */
    TRY(math(r,0x80055fd0,NBA97_NET_AVERAGE_FOUR,0,0,0));
    return math(r,0x80055fd8,NBA97_PROJECTION_DEPTH,0,0,depth);
}
typedef struct Matrix {uint32_t word[5],address;} Matrix;
static int mr(Run* r,const Matrix* m,uint32_t pc,unsigned off,unsigned width,unsigned mask,uint32_t* out){
    Nba97PlayerFrameValue v={0};
    if(!m->address){*out=(m->word[off/4]>>((off&3)*8))&bits(width);return NBA97_BODY_OK;}
    TRY(access(r,pc,m->address+off,width,NBA97_FRAME_READ,&v));if((v.known_mask&mask)!=mask)return NBA97_BODY_UNKNOWN;*out=v.word;return NBA97_BODY_OK;
}
static int mw(Run* r,Matrix* m,uint32_t pc,unsigned off,unsigned width,uint32_t value){
    if(m->address)return wr(r,pc,m->address+off,width,value);
    m->word[off/4]=(m->word[off/4]&~(bits(width)<<((off&3)*8)))|((value&bits(width))<<((off&3)*8));return NBA97_BODY_OK;
}
static uint32_t mul12(uint32_t a,uint32_t b){return shr(a*b,12);}
static int trig(Run* r,uint32_t angle,unsigned axis,uint32_t* sine,uint32_t* cosine){
    static const uint32_t pos[3]={0x800560d0,0x80056134,0x800561cc},neg[3]={0x800560a8,0x8005610c,0x80056198};
    uint32_t a=sx16(angle),word,index=(s32(a)<0?0-a:a)&4095;
    TRY(rd(r,s32(a)<0?neg[axis]:pos[axis],0x800b3254+index*4,4,&word));
    *sine=sx16(word);if(s32(a)<0)*sine=0-*sine;*cosine=sx16(word>>16);return NBA97_BODY_OK;
}
static int euler(Run* r,uint32_t angles,Matrix* dst){
    uint32_t a,sx,cx,sy,cy,sz,cz,t;
    TRY(rd(r,0x80056080,angles,2,&a));TRY(trig(r,a,0,&sx,&cx));
    TRY(rd(r,0x800560e4,angles+2,2,&a));TRY(trig(r,a,1,&sy,&cy));
    TRY(rd(r,0x80056150,angles+4,2,&a));
    TRY(mw(r,dst,0x80056154,4,2,sy));TRY(mw(r,dst,0x80056168,10,2,shr(0-cy*sx,12)));
    TRY(mw(r,dst,s32(sx16(a))<0?0x8005617c:0x800561bc,16,2,mul12(cy,cx)));TRY(trig(r,a,2,&sz,&cz));
    TRY(mw(r,dst,0x800561ec,0,2,mul12(cz,cy)));TRY(mw(r,dst,0x80056204,2,2,shr(0-sz*cy,12)));
    t=mul12(cz,0-sy);TRY(mw(r,dst,0x8005623c,6,2,mul12(sz,cx)-mul12(t,sx)));TRY(mw(r,dst,0x80056264,12,2,mul12(sz,sx)+mul12(t,cx)));
    t=mul12(sz,0-sy);TRY(mw(r,dst,0x8005629c,8,2,mul12(cz,cx)+mul12(t,sx)));return mw(r,dst,0x800562c0,14,2,mul12(cz,sx)-mul12(t,cx));
}
static int load_matrix(Run* r,const Matrix* m,uint32_t readpc,uint32_t writepc){
    uint32_t v[5];unsigned i;
    for(i=0;i<5;++i)TRY(mr(r,m,readpc+i*4,i*4,4,i==4?3:15,&v[i]));
    for(i=0;i<5;++i)TRY(math(r,writepc+i*4,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_ROTATION,i,v[i],0));
    return NBA97_BODY_OK;
}
static int product(Run* r,const Matrix* a,const Matrix* b,Matrix* dst){
    uint32_t x,y,z,first[3],second[3],v;unsigned i;
    TRY(load_matrix(r,a,0x800562cc,0x800562e0));
    TRY(mr(r,b,0x800562f4,0,2,3,&x));TRY(mr(r,b,0x800562f8,4,4,12,&y));TRY(mr(r,b,0x800562fc,12,4,3,&z));
    TRY(math(r,0x8005630c,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_VERTEX,0,x|(y&0xffff0000),0));TRY(math(r,0x80056310,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_VERTEX,1,z&65535,0));TRY(math(r,0x80056318,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_ROTATE,0,0,0));
    TRY(mr(r,b,0x8005631c,2,2,3,&x));TRY(mr(r,b,0x80056320,8,4,3,&y));TRY(mr(r,b,0x80056324,14,2,3,&z));
    for(i=0;i<3;++i)TRY(math(r,0x80056330+i*4,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_IR,i,0,&first[i]));
    TRY(math(r,0x8005633c,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_VERTEX,0,x|(y<<16),0));TRY(math(r,0x80056340,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_VERTEX,1,z,0));TRY(math(r,0x80056348,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_ROTATE,0,0,0));
    TRY(mr(r,b,0x8005634c,4,2,3,&x));TRY(mr(r,b,0x80056350,8,4,12,&y));TRY(mr(r,b,0x80056354,16,4,3,&z));
    for(i=0;i<3;++i)TRY(math(r,0x80056364+i*4,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_IR,i,0,&second[i]));
    TRY(math(r,0x80056370,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_VERTEX,0,x|(y&0xffff0000),0));TRY(math(r,0x80056374,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_VERTEX,1,z&65535,0));TRY(math(r,0x8005637c,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_ROTATE,0,0,0));
    TRY(mw(r,dst,0x8005638c,0,4,(first[0]&65535)|(second[0]<<16)));TRY(mw(r,dst,0x8005639c,12,4,(first[2]&65535)|(second[2]<<16)));
    TRY(math(r,0x800563a0,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_IR,0,0,&x));TRY(math(r,0x800563a4,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_IR,1,0,&y));
    TRY(mw(r,dst,0x800563b4,4,4,(x&65535)|(first[1]<<16)));TRY(mw(r,dst,0x800563c4,8,4,(second[1]&65535)|(y<<16)));
    TRY(math(r,0x800563c8,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_IR,2,0,&v));return mw(r,dst,0x800563c8,16,4,v);
}
static int rim_matrix(Run* r,uint32_t side){
    Matrix rotation={{0},0},identity={{4096,0,4096,0,4096},0},scaled={{0},0},net={{0},0},combined={{0},0};
    uint32_t angles[3],v,x,y,z,translation[3];unsigned i;
    TRY(rd(r,0x8004c160,0x800fa638,2,&angles[0]));TRY(rd(r,0x8004c168,0x800fa63a,2,&angles[1]));TRY(rd(r,0x8004c170,0x800fa63c,2,&angles[2]));
    for(i=0;i<3;++i)TRY(wr(r,0x8004c1ac+i*8,0x800fa638+i*2,2,angles[i]&4095));
    TRY(euler(r,0x800fa638,&rotation));TRY(product(r,&rotation,&identity,&scaled));
    /*4C1DC..4C258 scales only the FIRST ROW by16/10, with signed quotient
     * truncation. This is not a uniform matrix scale or repaired camera. */
    for(i=0;i<3;++i){v=(scaled.word[i/2]>>((i&1)*16))&65535;v=(uint32_t)(s32(sx16(v)<<4)/10);TRY(mw(r,&scaled,0, i*2,2,v));}
    for(i=0;i<5;++i)TRY(math(r,i==4?0x80055f40:0x80055f2c+i*4,NBA97_PROJECTION_ROTATION,i,scaled.word[i],0));
    for(i=0;i<3;++i)TRY(math(r,i==2?0x80055f5c:0x80055f50+i*4,NBA97_PROJECTION_TRANSLATION,i,0,0));
    TRY(rd(r,0x8004c268,0x800fab98,2,&x));x=sx16(x)+(side?0u-0xac0:0xac0)+(side?80:0u-80);
    TRY(rd(r,0x8004c2a0,0x800fab9a,2,&y));TRY(rd(r,0x8004c2a8,0x800fab9c,2,&z));y+=0x280;
    TRY(math(r,0x80056650,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_VERTEX,0,(x&65535)|(y<<16),0));TRY(math(r,0x80056654,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_VERTEX,1,z,0));
    TRY(math(r,0x8005665c,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_TRANSFORM,0,0,0));
    for(i=0;i<3;++i)TRY(math(r,0x80056660+i*4,NBA97_NET_VECTOR_BASE+NBA97_PLAYER_MAC,i,0,&translation[i]));
    net.address=0x801076c4+side*0x1324;TRY(euler(r,net.address+32,&net));TRY(product(r,&scaled,&net,&combined));
    TRY(rd(r,0x8004c314,0x800fa630,2,&v));translation[0]+=sx16(v);TRY(rd(r,0x8004c320,0x800fa632,2,&v));translation[1]+=sx16(v);TRY(rd(r,0x8004c32c,0x800fa634,2,&v));translation[2]+=sx16(v);
    for(i=0;i<5;++i)TRY(math(r,i==4?0x80055f40:0x80055f2c+i*4,NBA97_PROJECTION_ROTATION,i,combined.word[i],0));
    for(i=0;i<3;++i)TRY(math(r,i==2?0x80055f5c:0x80055f50+i*4,NBA97_PROJECTION_TRANSLATION,i,translation[i],0));
    return NBA97_BODY_OK;
}
static int table_link(Run* r,uint32_t pc,uint32_t depth,uint32_t packet){uint32_t table;TRY(rd(r,pc,0x80102924,4,&table));return link(r,table+((depth&4095)<<2),packet);}
static int visible(Run* r,uint32_t pc,uint32_t side,int* yes){
    uint32_t count,suppression,mask;TRY(rd(r,pc,0x8010b60c,4,&count));TRY(rd(r,pc+12,0x800dcf10,4,&suppression));TRY(rd(r,pc+20,0x800fcc54,4,&mask));
    *yes=(mask&(1u<<((count-1+side-suppression)&31)))!=0;return NBA97_BODY_OK;
}
static int line_values(Run* r,uint32_t pc,uint32_t p,uint32_t v[6]){
    static const unsigned off[6]={10,10,8,14,14,12};unsigned i;for(i=0;i<6;++i)TRY(rd(r,pc+i*4,p+off[i],2,&v[i]));return NBA97_BODY_OK;
}
static int draw(Run* r){
    uint32_t side,bank,vertices,p,q,depth,v[4],out[4],a[6],b[6],count,suppression,mask,index;unsigned i;int yes;
    for(side=0;side<2;++side){
        TRY(camera(r));vertices=0x801063cc+side*0x1324;
        TRY(rd(r,0x8004bb10,0x8001ede8,4,&bank));p=0x80106444+side*0x1324+bank*48;
        v[0]=vertices+24;v[1]=vertices+16;v[2]=vertices;v[3]=vertices+8;
        for(i=0;i<4;++i)out[i]=p+8+i*4;
        TRY(project4(r,v,out,&depth));TRY(visible(r,0x8004bb78,side,&yes));if(yes)TRY(table_link(r,0x8004bbb4,depth,p));
        v[0]=vertices;v[1]=vertices+8;v[2]=vertices+32;v[3]=vertices+40;
        for(i=0;i<4;++i)out[i]=p+32+i*4;
        TRY(project4(r,v,out,&depth));TRY(visible(r,0x8004bc10,side,&yes));if(yes)TRY(table_link(r,0x8004bc4c,depth,p+24));
        for(index=0;index<7;++index){
            TRY(rd(r,0x8004bc84,0x8001ede8,4,&bank));p=0x801064a4+side*0x1324+bank*720+index*96;q=p+48;
            v[0]=vertices+index*16;v[1]=v[0]+8;v[2]=v[1];v[3]=v[0]+16;out[0]=p+8;out[1]=p+12;out[2]=q+8;out[3]=q+12;
            TRY(project4(r,v,out,&depth));TRY(line_values(r,0x8004bcec,p,a));TRY(rd(r,0x8004bd0c,0x8010b60c,4,&count));
            TRY(wr(r,0x8004bd20,p+26,2,a[0]+1));TRY(wr(r,0x8004bd24,p+42,2,a[1]-1));TRY(wr(r,0x8004bd28,p+40,2,a[2]));TRY(wr(r,0x8004bd2c,p+24,2,a[2]));
            TRY(wr(r,0x8004bd30,p+30,2,a[3]+1));TRY(wr(r,0x8004bd34,p+46,2,a[4]-1));TRY(wr(r,0x8004bd38,p+44,2,a[5]));TRY(wr(r,0x8004bd3c,p+28,2,a[5]));
            TRY(line_values(r,0x8004bd40,q,b));TRY(wr(r,0x8004bd64,q+42,2,b[1]-1));TRY(rd(r,0x8004bd6c,0x800dcf10,4,&suppression));
            TRY(wr(r,0x8004bd7c,q+26,2,b[0]+1));TRY(rd(r,0x8004bd84,0x800fcc54,4,&mask));
            TRY(wr(r,0x8004bd94,q+40,2,b[2]));TRY(wr(r,0x8004bd98,q+24,2,b[2]));TRY(wr(r,0x8004bd9c,q+30,2,b[3]+1));TRY(wr(r,0x8004bda0,q+46,2,b[4]-1));TRY(wr(r,0x8004bda4,q+44,2,b[5]));TRY(wr(r,0x8004bdb8,q+28,2,b[5]));
            if(mask&(1u<<((count-1+side-suppression)&31))){
                static const uint32_t pc[6]={0x8004bdc0,0x8004bddc,0x8004bdf0,0x8004be04,0x8004be18,0x8004be2c};
                for(i=0;i<6;++i)TRY(table_link(r,pc[i],depth,i<3?p+i*16:q+(i-3)*16));
            }
        }
        TRY(rd(r,0x8004be60,0x8001ede8,4,&bank));p=0x801064a4+side*0x1324+bank*720+0x2a0;
        v[0]=vertices+112;v[1]=vertices+16;v[2]=v[0];v[3]=v[1];out[0]=p+8;out[1]=p+12;out[2]=out[0];out[3]=out[1];
        TRY(project4(r,v,out,&depth));TRY(line_values(r,0x8004bebc,p,a));TRY(rd(r,0x8004bed8,0x8010b60c,4,&count));TRY(rd(r,0x8004beec,0x800fcc54,4,&mask));
        TRY(wr(r,0x8004bf00,p+26,2,a[0]+1));TRY(rd(r,0x8004bf08,0x800dcf10,4,&suppression));
        TRY(wr(r,0x8004bf18,p+42,2,a[1]-1));TRY(wr(r,0x8004bf1c,p+40,2,a[2]));TRY(wr(r,0x8004bf20,p+24,2,a[2]));TRY(wr(r,0x8004bf24,p+30,2,a[3]+1));TRY(wr(r,0x8004bf28,p+46,2,a[4]-1));TRY(wr(r,0x8004bf2c,p+44,2,a[5]));TRY(wr(r,0x8004bf40,p+28,2,a[5]));
        if(mask&(1u<<((count-1+side-suppression)&31))){TRY(table_link(r,0x8004bf48,depth,p));TRY(table_link(r,0x8004bf64,depth,p+16));TRY(table_link(r,0x8004bf78,depth,p+32));}
    }
    TRY(rd(r,0x8004bfc4,0x800dcf10,4,&suppression));if(suppression)return NBA97_BODY_OK;
    for(side=0;side<2;++side){
        TRY(rd(r,0x8004bfe4,0x8010b60c,4,&count));TRY(rd(r,0x8004bfec,0x800fcc54,4,&mask));
        if(!(mask&(1u<<((count-1+side)&31))))continue;
        TRY(rim_matrix(r,side));TRY(rd(r,0x8004c018,0x80103fa8,4,&count));TRY(rd(r,0x8004c024,0x801076ec+side*0x1324,4,&vertices));
        if(s32(shr(count,1)-2)<=0)continue;
        index=0;
        do{
            v[0]=vertices;v[1]=vertices+8;v[2]=vertices+16;v[3]=vertices+24;
            TRY(rd(r,0x8004c04c,0x8001ede8,4,&bank));p=0x80106a44+side*0x1324+bank*1600+index*32;
            out[0]=p+8;out[1]=p+12;out[2]=p+24;out[3]=p+28;TRY(project4(r,v,out,&depth));
            TRY(table_link(r,0x8004c0b0,depth,p));TRY(table_link(r,0x8004c0cc,depth,p+16));TRY(rd(r,0x8004c0e0,0x80103fa8,4,&count));
            ++index;vertices+=32;
        }while(s32(index)<s32(shr(count,1)-2));
    }
    return NBA97_BODY_OK;
}
static int frame(Run* r){
    uint32_t first;TRY(rd(r,0x8004b1a8,0x800b72dc,4,&first));
    if(first){TRY(initialize(r));TRY(wr(r,0x8004b1cc,0x80103fa8,4,100));TRY(wr(r,0x8004b1d4,0x800b72dc,4,0));}
    /* Source4B1D8 renders BEFORE animation advance/deformation. */
    TRY(draw(r));return advance(r);
}
static int begin(Nba97PlayerFrameContext* in,Nba97GameNetProgress* out,Run* r){
    if(!out)return NBA97_BODY_ARGUMENT;
    memset(out,0,sizeof *out);if(!in||!in->access)return NBA97_BODY_ARGUMENT;
    r->in=in;r->out=out;r->link_status=NBA97_BODY_OK;return NBA97_BODY_OK;
}
static int finish(Run* r,int status){if(status==NBA97_BODY_OK){r->out->completed=1;r->out->stopped_pc=0;r->out->stopped_address=0;}return status;}
int nba97_game_net_frame(Nba97PlayerFrameContext* in,Nba97GameNetProgress* out){Run r;TRY(begin(in,out,&r));return finish(&r,frame(&r));}
int nba97_game_net_initialize(Nba97PlayerFrameContext* in,Nba97GameNetProgress* out){Run r;TRY(begin(in,out,&r));return finish(&r,initialize(&r));}
int nba97_game_net_draw(Nba97PlayerFrameContext* in,Nba97GameNetProgress* out){Run r;TRY(begin(in,out,&r));return finish(&r,draw(&r));}
int nba97_game_net_decode(Nba97PlayerFrameContext* in,uint32_t source,uint32_t destination,Nba97GamePeriodValue* length,Nba97GameNetProgress* out){
    Run r;TRY(begin(in,out,&r));if(!length)return NBA97_BODY_ARGUMENT;return finish(&r,decode(&r,source,destination,length));
}
