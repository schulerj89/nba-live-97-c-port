#include "game_player_marker_resources.h"
#include <string.h>
typedef struct Run {Nba97PlayerMarkerContext* in;Nba97PlayerMarkerProgress* out;} Run;
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
static int call(Run* r,uint32_t pc,uint32_t entry,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t* result){
    Nba97PlayerMarkerCall q;Nba97GamePeriodValue v={0,0};int status;
    TRY(reserve(r,pc,0));if(!r->in->io)return NBA97_MARKER_IO_REQUIRED;
    q.pc=pc;q.entry=entry;q.args[0]=a;q.args[1]=b;q.args[2]=c;q.args[3]=d;q.args[4]=e;
    status=r->in->io(r->in->user,&q,&v);if(status!=NBA97_BODY_OK)return status;
    ++r->out->calls;if(result){if(v.known>1||(!v.known&&v.word))return NBA97_BODY_ARGUMENT;
        if(!v.known)return NBA97_BODY_UNKNOWN;
        *result=v.word;}
    return NBA97_BODY_OK;
}
static int upload_sync(Run* r,uint32_t image,uint32_t x,uint32_t y,uint32_t cy){
    TRY(call(r,0x80050e4c,0x800946b8,image,x,y,512,cy,0));
    return call(r,0x80050e54,0x800994f4,0,0,0,0,0,0);
}
static int entry(Run* r,uint32_t resource,uint32_t index,uint32_t* image){
    uint32_t n,v;TRY(rd(r,0x800a3fec,resource+8,4,&n));*image=0;
    if(index<n){TRY(rd(r,0x800a4000,resource+20+index*8,4,&v));*image=resource+v;}
    return NBA97_BODY_OK;
}
static int name(Run* r,uint32_t resource,uint32_t query,uint32_t* image){
    uint32_t n,key,v,p=resource+16;TRY(rd(r,0x800a5478,resource+8,4,&n));
    TRY(rd(r,0x800a547c,query,4,&key));*image=0;if(!n)return NBA97_BODY_OK;
    for(;;){
        /* A548C prefetches a name beyond the final directory entry on a miss.
         * Keep that reached read even though A5494 then rejects the result. */
        TRY(rd(r,0x800a548c,p,4,&v));if(!n)return NBA97_BODY_OK;--n;p+=8;
        if(v==key){TRY(rd(r,0x800a54a4,p-4,4,&v));*image=resource+v;return NBA97_BODY_OK;}
    }
}
static int page(Run* r,uint32_t mode,uint32_t abr,uint32_t x,uint32_t y,uint32_t* v){
    uint32_t graphics;int alternate;
    /*9BFBC/9BFD0 sample LIVE993DC twice unless the first result is1. */
    TRY(rd(r,0x800993e0,0x800c55c0,1,&graphics));alternate=graphics==1;
    if(!alternate){TRY(rd(r,0x800993e0,0x800c55c0,1,&graphics));alternate=graphics==2;}
    *v=alternate?((mode&3)<<9)|((abr&3)<<7)|((y&0x300)>>3)|((x&0x3ff)>>6)
                :((mode&3)<<7)|((abr&3)<<5)|((y&0x100)>>4)|((x&0x3ff)>>6)|((y&0x200)<<2);
    return NBA97_BODY_OK;
}
static uint32_t clut(uint32_t x,uint32_t y){return ((y<<6)|((shr(x,4))&63))&65535;}
static int palette(Run* r,uint32_t image,uint32_t* value){
    uint32_t v,x,y;*value=0;
    while(image){
        TRY(rd(r,0x800a9c58,image,1,&v));
        if((v&0xf7)==0x23){TRY(rd(r,0x800a9c6c,image+12,2,&x));TRY(rd(r,0x800a9c70,image+14,2,&y));
            *value=sx16(clut(sx16(x),sx16(y)));return NBA97_BODY_OK;}
        TRY(rd(r,0x800a9c88,image,4,&v));image=(v&0xffffff00)?image+shr(v,8):0;
    }
    return NBA97_BODY_OK;
}
static int packet_tag(Run* r,uint32_t packet){
    TRY(wr(r,0x8009c32c,packet+3,1,9));return wr(r,0x8009c338,packet+7,1,0x2c);
}
static int semi(Run* r,uint32_t packet,int enabled){
    uint32_t v;TRY(rd(r,enabled?0x8009c27c:0x8009c288,packet+7,1,&v));
    return wr(r,0x8009c298,packet+7,1,enabled?v|2:v&0xfd);
}
static int packet(Run* r,uint32_t p,uint32_t image,int reflected){
    uint32_t x,y,w,h,mode,v,a,b;TRY(packet_tag(r,p));
    if(!reflected){
        TRY(rd(r,0x80050e90,image+12,1,&x));TRY(rd(r,0x80050e94,image+14,1,&y));x=(x&63)*2;
        TRY(wr(r,0x80050ea4,p+12,1,x));TRY(wr(r,0x80050ea8,p+13,1,y));
        TRY(rd(r,0x80050eac,image+4,1,&w));TRY(wr(r,0x80050eb0,p+21,1,y));TRY(wr(r,0x80050eb4,p+28,1,x));
        TRY(wr(r,0x80050ec0,p+20,1,x+w-1));TRY(rd(r,0x80050ec4,image+6,1,&h));TRY(wr(r,0x80050ed4,p+29,1,y+h-1));
        TRY(rd(r,0x80050ed8,image+4,1,&w));TRY(wr(r,0x80050ee8,p+36,1,x+w-1));
        TRY(rd(r,0x80050eec,image+6,1,&h));TRY(wr(r,0x80050efc,p+37,1,y+h-1));
        TRY(rd(r,0x80050f00,image,1,&mode));TRY(rd(r,0x80050f04,image+12,2,&a));TRY(rd(r,0x80050f08,image+14,2,&b));
        TRY(page(r,mode,0,sx16(a),sx16(b),&v));TRY(wr(r,0x80050f20,p+22,2,v));
        TRY(wr(r,0x80050f28,p+4,1,128));TRY(wr(r,0x80050f2c,p+5,1,128));TRY(wr(r,0x80050f34,p+6,1,128));TRY(semi(r,p,0));
        TRY(rd(r,0x80050f38,image,4,&v));image=(v&0xffffff00)?image+shr(v,8):0;
        /*50F54's zero link is NOT a default palette: it leads to LH at0xC. */
        TRY(rd(r,0x80050f5c,image+12,2,&a));TRY(rd(r,0x80050f60,image+14,2,&b));TRY(wr(r,0x80050f6c,p+14,2,clut(sx16(a),sx16(b))));
    }else{
        TRY(rd(r,0x80050fa4,image+12,1,&x));TRY(rd(r,0x80050fa8,image+14,1,&y));x=(x&63)*2;
        TRY(wr(r,0x80050fb8,p+12,1,x));TRY(rd(r,0x80050fbc,image+6,1,&h));TRY(wr(r,0x80050fcc,p+13,1,y+h-1));
        TRY(rd(r,0x80050fd0,image+4,1,&w));TRY(wr(r,0x80050fe0,p+20,1,x+w-1));TRY(rd(r,0x80050fe4,image+6,1,&h));
        TRY(wr(r,0x80050fe8,p+28,1,x));TRY(wr(r,0x80050fec,p+29,1,y));TRY(wr(r,0x80050ff8,p+21,1,y+h-1));
        TRY(rd(r,0x80050ffc,image+4,1,&w));TRY(wr(r,0x80051000,p+37,1,y));TRY(wr(r,0x8005100c,p+36,1,x+w-1));
        TRY(rd(r,0x80051010,image,1,&mode));TRY(rd(r,0x80051014,image+12,2,&a));TRY(rd(r,0x80051018,image+14,2,&b));
        TRY(page(r,mode,3,sx16(a),sx16(b),&v));TRY(wr(r,0x80051030,p+22,2,v));
        TRY(wr(r,0x80051038,p+4,1,128));TRY(wr(r,0x8005103c,p+5,1,128));TRY(wr(r,0x80051044,p+6,1,128));TRY(semi(r,p,1));
        TRY(rd(r,0x80051048,image,4,&v));image=(v&0xffffff00)?image+shr(v,8):0;
        TRY(rd(r,0x8005106c,image+12,2,&a));TRY(rd(r,0x80051070,image+14,2,&b));TRY(wr(r,0x8005107c,p+14,2,clut(sx16(a),sx16(b))));
    }
    ++r->out->packets;return NBA97_BODY_OK;
}
static int copy(Run* r,uint32_t source,uint32_t destination,unsigned bytes){
    Nba97PlayerFrameValue v[8];unsigned n,i,half;int backward=0;
    if(bytes!=16&&bytes!=32&&bytes!=528)return NBA97_BODY_ARGUMENT;
    if(s32(source)<s32(destination)){
        TRY(reserve(r,0x800aa65c,source));if(s32(source)>INT32_MAX-(int32_t)bytes)return NBA97_FRAME_ARITHMETIC_TRAP;
        backward=s32(destination)<s32(source+bytes);
    }
    if(backward){TRY(reserve(r,0x800aa670,destination));
        if(s32(destination)>INT32_MAX-(int32_t)bytes)return NBA97_FRAME_ARITHMETIC_TRAP;
        source+=bytes;destination+=bytes;}
    if((source|destination)&3){TRY(reserve(r,backward?0x800aa67c:0x800aa478,source));return NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT;}
    /* AA468 has eight-load/eight-store groups, not a whole-copy snapshot.
     * Preserve overlaps and unknown byte/reference identity through each group. */
    while(!backward&&bytes>=64){
        for(half=0;half<2;++half){
            for(i=0;i<8;++i){memset(&v[i],0,sizeof v[i]);TRY(access(r,0x800aa48c+half*64+i*4,source+half*32+i*4,4,NBA97_FRAME_READ,&v[i]));}
            for(i=0;i<8;++i)TRY(access(r,0x800aa4ac+half*64+i*4,destination+half*32+i*4,4,NBA97_FRAME_WRITE,&v[i]));
        }
        source+=64;destination+=64;bytes-=64;
    }
    n=bytes/16;
    while(n--){
        if(backward){source-=16;destination-=16;}
        for(i=0;i<4;++i){memset(&v[i],0,sizeof v[i]);TRY(access(r,(backward?0x800aa690:0x800aa528)+i*4,source+i*4,4,NBA97_FRAME_READ,&v[i]));}
        for(i=0;i<4;++i)TRY(access(r,(backward?0x800aa6a0:0x800aa538)+i*4,destination+i*4,4,NBA97_FRAME_WRITE,&v[i]));
        if(!backward){source+=16;destination+=16;}
    }
    ++r->out->copies;return NBA97_BODY_OK;
}
static int arrows(Run* r){
    static const uint16_t xs[8]={0x2a0,0x2a6,0x2ac,0x2b2,0x2b8,0x2c0,0x2c6,0x2cc};
    unsigned i;uint32_t resource,image,p,pc,v,x,y,w,h,mode;
    for(i=0;i<8;++i){
        pc=0x8004cb00+i*256;p=0x800faa54+i*40;
        TRY(rd(r,i?pc+24:0x8004caf8,0x800dce04,4,&resource));
        TRY(name(r,resource,0x80026134+i*8,&image));
        TRY(call(r,pc+(i?64:56),0x800946b8,image,xs[i],0x50,512,236,0));
        TRY(packet_tag(r,p));TRY(palette(r,image,&v));TRY(wr(r,pc+92,p+14,2,v));
        TRY(rd(r,pc+96,image+12,2,&x));TRY(rd(r,pc+100,image,1,&mode));TRY(rd(r,pc+104,image+14,2,&y));
        TRY(page(r,mode,0,sx16(x),sx16(y),&v));TRY(wr(r,pc+124,p+22,2,v));
        TRY(rd(r,pc+128,image+12,2,&x));TRY(rd(r,pc+132,image+14,1,&y));x=(x&63)*4;
        TRY(wr(r,pc+148,p+12,1,x));TRY(wr(r,pc+156,p+13,1,y));
        TRY(rd(r,pc+160,image+4,1,&w));TRY(wr(r,pc+168,p+21,1,y));TRY(wr(r,pc+176,p+28,1,x));
        TRY(wr(r,pc+192,p+20,1,x+w-1));TRY(rd(r,pc+196,image+6,1,&h));TRY(wr(r,pc+216,p+29,1,y+h-1));
        TRY(rd(r,pc+220,image+4,1,&w));TRY(wr(r,pc+240,p+36,1,x+w-1));
        TRY(rd(r,pc+244,image+6,1,&h));TRY(wr(r,pc+264,p+37,1,y+h-1));TRY(semi(r,p,0));
        ++r->out->arrows;
    }
    return NBA97_BODY_OK;
}
static int initialize(Run* r){
    uint32_t asdw,ball,image,v,a,b,blob,circ,p,mode;unsigned i,j;
    TRY(call(r,0x8004d4c0,0x80029bfc,0x80026174,0,0,0,0,&asdw));
    TRY(wp(r,0x8004d4d4,0x800dce04,asdw));
    TRY(call(r,0x8004d4d8,0x80029bfc,0x80026184,0,0,0,0,&ball));
    for(i=0;i<15;++i){
        TRY(entry(r,ball,i*2,&image));
        /*4D4FC's signed reciprocal division has exactly this result for the
         * source loop's0..14. All15 even-index ball images are uploaded. */
        TRY(upload_sync(r,image,0x2a0+(i%6)*16,0x60+(i/6)*32,0xe0));
    }
    TRY(entry(r,ball,0,&image));TRY(wp(r,0x8004d56c,0x800fdb48,image));
    TRY(packet(r,0x80103ee4,image,0));TRY(packet(r,0x80103f0c,image,0));
    TRY(packet(r,0x8010b1f0,image,1));TRY(packet(r,0x8010b218,image,1));
    /*4D5A4..4D64C performs duplicate page stores, with fresh image/mode reads
     * between them. Do not consolidate them or copy a previously built tag. */
    for(i=0;i<4;++i){
        static const uint32_t readpc[4]={0x8004d5a4,0x8004d5cc,0x8004d5f4,0x8004d61c};
        static const uint32_t storepc[4]={0x8004d5c0,0x8004d5e8,0x8004d610,0x8004d644};
        TRY(rd(r,readpc[i],image,1,&mode));TRY(page(r,mode,i<2?0:3,0x280,0,&v));
        TRY(wr(r,storepc[i],i<2?0x80103f22:0x8010b22e,2,v));
        TRY(wr(r,storepc[i]+8,i<2?0x80103efa:0x8010b206,2,v));
    }
    for(i=0;i<4;++i)TRY(packet(r,0x800f9c58+i*40,image,0));
    /* Source releases BALL while retaining FDB48's numeric image address. No
     * native lifetime extension or later dereference is implied by that SW. */
    TRY(call(r,0x8004d67c,0x80090698,ball,0,0,0,0,0));
    TRY(rd(r,0x8004d688,0x800dce04,4,&asdw));TRY(name(r,asdw,0x80026194,&blob));
    TRY(rd(r,0x8004d6a0,0x800dce04,4,&asdw));TRY(wp(r,0x8004d6b0,0x8010b1ec,blob));
    TRY(name(r,asdw,0x8002619c,&circ));TRY(wp(r,0x8004d6d8,0x80109b7c,circ));
    TRY(upload_sync(r,circ,0x2e0,0xa0,0xe2));TRY(rd(r,0x8004d6e4,circ,4,&v));circ+=shr(v,8);
    for(i=0;i<10;++i){TRY(copy(r,circ,0x800eba50+i*48,16));TRY(copy(r,circ+16,0x800eba60+i*48,32));}
    TRY(rd(r,0x8004d738,0x8010b1ec,4,&blob));TRY(rd(r,0x8004d744,blob,4,&v));
    TRY(copy(r,blob+shr(v,8),0x80109b90,528));TRY(upload_sync(r,blob,0x2d0,0xa0,0xe3));
    for(i=0;i<10;++i)for(j=0;j<2;++j){
        p=0x800d8f14+i*80+j*40;TRY(packet(r,p,blob,0));
        TRY(rd(r,0x8004d7ac,blob+12,2,&a));TRY(rd(r,0x8004d7b0,blob,1,&mode));TRY(rd(r,0x8004d7b4,blob+14,2,&b));
        TRY(page(r,mode,2,sx16(a),sx16(b),&v));TRY(wr(r,0x8004d7d4,p+22,2,v));TRY(semi(r,p,1));
    }
    /*4D810 uploads the SAME mutable blob again. Player packets retain their
     * earlier UVs while the two ball-shadow packets use the new coordinates. */
    TRY(upload_sync(r,blob,0x200,0x100,0xe1));
    for(j=0;j<2;++j){
        p=0x800d9234+j*40;TRY(packet(r,p,blob,0));
        TRY(rd(r,0x8004d838,blob+12,2,&a));TRY(rd(r,0x8004d83c,blob,1,&mode));TRY(rd(r,0x8004d840,blob+14,2,&b));
        TRY(page(r,mode,2,sx16(a),sx16(b),&v));TRY(wr(r,0x8004d860,p+22,2,v));TRY(semi(r,p,1));
    }
    TRY(arrows(r));TRY(rd(r,0x8004d884,0x800dce04,4,&asdw));
    return call(r,0x8004d888,0x80090698,asdw,0,0,0,0,0);
}
static int begin(Nba97PlayerMarkerContext* c,Nba97PlayerMarkerProgress* p,Run* r){
    if(!p)return NBA97_BODY_ARGUMENT;
    memset(p,0,sizeof *p);if(!c||!c->access)return NBA97_BODY_ARGUMENT;
    r->in=c;r->out=p;return NBA97_BODY_OK;
}
static int finish(Run* r,int status){if(status==NBA97_BODY_OK){r->out->completed=1;r->out->stopped_pc=0;r->out->stopped_address=0;}return status;}
int nba97_game_player_marker_resources(Nba97PlayerMarkerContext* c,Nba97PlayerMarkerProgress* p){Run r;TRY(begin(c,p,&r));return finish(&r,initialize(&r));}
int nba97_game_player_marker_arrows(Nba97PlayerMarkerContext* c,Nba97PlayerMarkerProgress* p){Run r;TRY(begin(c,p,&r));return finish(&r,arrows(&r));}
int nba97_game_player_marker_packet(Nba97PlayerMarkerContext* c,uint32_t packet_address,uint32_t image,unsigned reflected,Nba97PlayerMarkerProgress* p){
    Run r;TRY(begin(c,p,&r));if(reflected>1)return NBA97_BODY_ARGUMENT;return finish(&r,packet(&r,packet_address,image,(int)reflected));
}
int nba97_game_player_marker_copy(Nba97PlayerMarkerContext* c,uint32_t source,uint32_t destination,unsigned bytes,Nba97PlayerMarkerProgress* p){
    Run r;TRY(begin(c,p,&r));return finish(&r,copy(&r,source,destination,bytes));
}
