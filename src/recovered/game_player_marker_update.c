#include "game_player_marker_update.h"
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
static uint32_t packet(uint32_t index,uint32_t bank){return 0x800d8f14u+index*80u+bank*40u;}
static int actor(Run* r,uint32_t ip,uint32_t tp,uint32_t ap,uint32_t* index,uint32_t* address){
    uint32_t table;TRY(rd(r,ip,0x801029b0,4,index));TRY(rd(r,tp,0x800fc650,4,&table));
    return rd(r,ap,table+(*index<<2),4,address);
}
static int blink(Run* r,uint32_t delta,int* hidden){
    uint32_t v;*hidden=0;TRY(rd(r,0x8004a0d4+delta,0x800eb678,4,&v));if(v)return NBA97_BODY_OK;
    TRY(rd(r,0x8004a0e8+delta,0x800fdb90,2,&v));if(v!=0x82)return NBA97_BODY_OK;
    TRY(rd(r,0x8004a0fc+delta,0x800fe88e,2,&v));if(!v)return NBA97_BODY_OK;
    TRY(rd(r,0x800a5814,0x800d7a70,4,&v));if(!(v&32))return NBA97_BODY_OK;
    TRY(rd(r,0x8004a124+delta,0x800f9ffe,2,&v));*hidden=!v;return NBA97_BODY_OK;
}
static int strength(Run* r,uint32_t de_pc,uint32_t df_pc,uint32_t multiplier_pc,uint32_t a,uint32_t* v){
    uint32_t flag,m;TRY(rd(r,de_pc,a+0xde,1,&flag));
    if(flag<3){TRY(rd(r,df_pc,a+0xdf,1,&flag));if(flag<3)goto clamp;}
    TRY(rd(r,multiplier_pc,0x800b72d4,4,&m));*v=(((*v&255)*m)>>4);
clamp:
    if(!(*v&255))*v=1;
    return NBA97_BODY_OK;
}
/* Controller colors deliberately reload actor/index/table/bank independently
 * for each component. The selected and unselected source blocks differ in PC
 * only; the final blue store is shared at4A9A0. */
static int controller_colors(Run* r,uint32_t delta,uint32_t* blue,uint32_t* final_packet){
    uint32_t i,a,b,c;unsigned component;
    for(component=0;component<2;++component){uint32_t d=delta+component*0x58;
        TRY(actor(r,0x8004a248+d,0x8004a250+d,0x8004a25c+d,&i,&a));
        TRY(rd(r,0x8004a264+d,0x8001ede8,4,&b));TRY(rd(r,0x8004a268+d,a+4,2,&c));
        TRY(rd(r,0x8004a280+d,0x800b72a0+component*8+sx16(c),1,&c));
        TRY(wr(r,0x8004a298+d,packet(i,b)+4+component,1,c));
    }
    TRY(actor(r,0x8004a2f8+delta,0x8004a300+delta,0x8004a314+delta,&i,&a));
    TRY(rd(r,0x8004a324+delta,0x8001ede8,4,&b));TRY(rd(r,0x8004a328+delta,a+4,2,&c));
    TRY(rd(r,0x8004a33c+delta,0x800b72b0+sx16(c),1,blue));*final_packet=packet(i,b);
    return NBA97_BODY_OK;
}
static int live_packet(Run* r,uint32_t index_pc,uint32_t bank_pc,uint32_t* p){
    uint32_t i,b;TRY(rd(r,index_pc,0x801029b0,4,&i));TRY(rd(r,bank_pc,0x8001ede8,4,&b));*p=packet(i,b);return NBA97_BODY_OK;
}
static int update(Run* r){
    uint32_t v,i,a,b,c,p,palette,stats,id,strength_value,controller,physical,slot;
    uint32_t mode=0,abr=0,x=0x2e0,blue=128,u;int hidden;
    TRY(rd(r,0x8004a048,0x800fac20,4,&v));++v;TRY(wr(r,0x8004a060,0x800fac20,4,v));
    /* Original wrapped FAC20 gate. A normal nonzero increment does no work;
     * only wrapped zero is processed, with -1 stored again before selection. */
    if(v)return NBA97_BODY_OK;
    TRY(rd(r,0x8004a070,0x800fdb58,4,&v));TRY(wr(r,0x8004a078,0x800fac20,4,UINT32_MAX));
    if(!v)goto fallback;
    TRY(actor(r,0x8004a088,0x8004a090,0x8004a09c,&i,&a));
    TRY(rd(r,0x8004a0a4,a+4,2,&controller));if(controller&0x8000)goto computer;
    TRY(rd(r,0x8004a0b8,0x800fc634,4,&slot));TRY(rd(r,0x8004a0bc,a,4,&id));
    TRY(rd(r,0x8004a0c0,slot,2,&v));if(id!=sx16(v))goto unselected;
    TRY(blink(r,0,&hidden));if(hidden)goto fallback;
    TRY(rd(r,0x8004a138,0x801029b0,4,&i));TRY(rd(r,0x8004a140,0x800fc650,4,&slot));slot+=i<<2;
    TRY(rd(r,0x8004a14c,slot,4,&a));
    /* Two independent table-slot loads precede actor id/stats reads. */
    TRY(rd(r,0x8004a150,slot,4,&b));TRY(rd(r,0x8004a154,a,4,&id));
    TRY(rd(r,0x8004a158,b+0x1c,4,&stats));TRY(rd(r,0x8004a164,stats+0x20,2,&v));
    palette=0x800eba50+id*48u;strength_value=shr(v<<16,26);TRY(wp(r,0x8004a17c,0x800fed1c,palette));
    TRY(strength(r,0x8004a180,0x8004a198,0x8004a1b0,b,&strength_value));
    TRY(rd(r,0x8004a1ec,0x801029b0,4,&i));v=(strength_value&255)*1057u;
    TRY(wr(r,0x8004a1f0,palette+0x14,2,v));TRY(wr(r,0x8004a1f4,palette+0x16,2,v));
    TRY(rd(r,0x8004a1fc,0x800fc650,4,&slot));TRY(rd(r,0x8004a208,slot+(i<<2),4,&a));
    TRY(rd(r,0x8004a210,a+0x1c,4,&stats));TRY(rd(r,0x8004a218,stats+0x20,2,&v));
    TRY(wr(r,0x8004a240,palette+0x10,2,s32(shr(v<<16,26))<16?0x6318:1));
    TRY(controller_colors(r,0,&blue,&p));goto finish_color;
unselected:
    palette=0x800eba50+id*48u;TRY(wr(r,0x8004a35c,palette+0x14,2,0));
    TRY(rd(r,0x8004a364,0x800fc650,4,&v));TRY(rd(r,0x8004a370,v+(i<<2),4,&a));
    TRY(rd(r,0x8004a378,a+0x1c,4,&stats));TRY(rd(r,0x8004a380,stats+0x20,2,&v));
    TRY(wp(r,0x8004a388,0x800fed1c,palette));
    TRY(wr(r,0x8004a3ac,palette+0x10,2,s32(shr(v<<16,26))<16?0x6318:1));
    TRY(actor(r,0x8004a3b4,0x8004a3bc,0x8004a3c8,&i,&a));
    TRY(rd(r,0x8004a3d0,a+0x1c,4,&stats));TRY(rd(r,0x8004a3d4,a+0xde,1,&c));
    TRY(rd(r,0x8004a3d8,stats+0x20,2,&v));strength_value=shr(v<<16,26);
    if(c<3){TRY(rd(r,0x8004a3ec,a+0xdf,1,&c));if(c<3)goto unselected_clamp;}
    TRY(rd(r,0x8004a404,0x800b72d4,4,&v));strength_value=((strength_value&255)*v)>>4;
unselected_clamp:
    if(!(strength_value&255))strength_value=1;
    TRY(wr(r,0x8004a43c,palette+0x16,2,(strength_value&255)*1057u));
    TRY(controller_colors(r,0x1fc,&blue,&p));goto finish_color;
computer:
    TRY(rd(r,0x8004a548,0x800fc654,4,&physical));TRY(rd(r,0x8004a550,0x800fc65c,4,&slot));
    TRY(rd(r,0x8004a560,slot,4,&c));a=physical+i*244u;
    TRY(rd(r,0x8004a56c,a,4,&id));TRY(rd(r,0x8004a570,c,4,&v));
    if(id!=v)goto computer_other;
    TRY(blink(r,0x4b0,&hidden));if(hidden)goto fallback;
    TRY(live_packet(r,0x8004a5e8,0x8004a5f0,&p));TRY(wr(r,0x8004a61c,p+4,1,128));
    TRY(live_packet(r,0x8004a624,0x8004a62c,&p));TRY(wr(r,0x8004a654,p+5,1,128));
    TRY(rd(r,0x8004a65c,0x801029b0,4,&i));goto computer_blue;
computer_other:
    TRY(rd(r,0x8004a668,a+0xce,1,&v));if(!v)goto computer_test;
    TRY(rd(r,0x8004a67c,0x800fc63c,4,&v));TRY(rd(r,0x8004a684,v,2,&v));if(!v)goto computer_test;
    TRY(rd(r,0x8004a698,0x8001ede8,4,&b));TRY(wr(r,0x8004a6bc,packet(i,b)+4,1,128));
    TRY(live_packet(r,0x8004a6c4,0x8004a6cc,&p));TRY(wr(r,0x8004a6f4,p+5,1,128));
    TRY(rd(r,0x8004a6fc,0x801029b0,4,&i));
computer_blue:
    TRY(rd(r,0x8004a704,0x8001ede8,4,&b));TRY(wr(r,0x8004a72c,packet(i,b)+6,1,128));goto computer_palette;
computer_test:
    TRY(rd(r,0x8004a73c,0x800fc660,4,&v));TRY(rd(r,0x8004a744,v,2,&v));if(!v)goto fallback;
    TRY(rd(r,0x8004a758,0x801029b0,4,&i));TRY(rd(r,0x8004a760,0x800fc654,4,&physical));
    TRY(rd(r,0x8004a77c,physical+i*244u,4,&id));TRY(rd(r,0x8004a784,0x800fa03c,4,&v));if(id!=v)goto fallback;
computer_palette:
    TRY(rd(r,0x8004a798,0x801029b0,4,&i));TRY(rd(r,0x8004a7a0,0x800fc654,4,&physical));a=physical+i*244u;
    TRY(rd(r,0x8004a7bc,a,4,&id));palette=0x800eba50+id*48u;TRY(wp(r,0x8004a7e0,0x800fed1c,palette));
    TRY(rd(r,0x8004a7e4,a+0xde,1,&v));strength_value=15;
    if(v<3){TRY(rd(r,0x8004a7f8,a+0xdf,1,&v));if(v<3)goto computer_palette_store;}
    TRY(rd(r,0x8004a810,0x800b72d4,4,&v));strength_value=(v*15u)>>4;
computer_palette_store:
    /* Unlike the controller routes, the computer route has no zero clamp. */
    TRY(wr(r,0x8004a828,palette+0x10,2,1));
    TRY(rd(r,0x8004a838,0x801029b0,4,&i));TRY(rd(r,0x8004a844,0x8001ede8,4,&b));
    v=(strength_value&255)*1057u;TRY(wr(r,0x8004a854,palette+0x16,2,v));TRY(wr(r,0x8004a858,palette+0x14,2,v));
    TRY(wr(r,0x8004a880,packet(i,b)+4,1,128));
    TRY(live_packet(r,0x8004a888,0x8004a898,&p));TRY(wr(r,0x8004a8c0,p+5,1,128));
    TRY(rd(r,0x8004a8c8,0x801029b0,4,&i));goto final_blue;
fallback:
    TRY(rd(r,0x8004a8dc,0x801029b0,4,&i));TRY(rd(r,0x8004a8e4,0x8001ede8,4,&b));
    TRY(wp(r,0x8004a8f8,0x800fed1c,0x80109b90));TRY(wr(r,0x8004a920,packet(i,b)+4,1,128));
    TRY(live_packet(r,0x8004a928,0x8004a938,&p));TRY(wr(r,0x8004a960,p+5,1,128));
    TRY(rd(r,0x8004a968,0x801029b0,4,&i));mode=1;abr=2;x=0x2d0;
final_blue:
    TRY(rd(r,0x8004a978,0x8001ede8,4,&b));p=packet(i,b);
finish_color:
    TRY(wr(r,0x8004a9a0,p+6,1,blue));TRY(page(r,mode,abr,x,160,&v));
    TRY(rd(r,0x8004a9b0,0x8001ede8,4,&b));TRY(rd(r,0x8004a9b8,0x801029b0,4,&i));
    TRY(wr(r,0x8004a9e0,packet(i,b)+0x16,2,v));
    TRY(rd(r,0x8004a9ec,0x801029b0,4,&i));TRY(rd(r,0x8004a9f4,0x800fed1c,4,&palette));
    TRY(upload_sync(r,palette,0,0,i+0xe2));
    TRY(rd(r,0x8004aa10,0x801029b0,4,&i));v=clut(512,i+0xe2);
    TRY(rd(r,0x8004aa2c,0x8001ede8,4,&b));TRY(rd(r,0x8004aa38,0x801029b0,4,&i));p=packet(i,b);
    TRY(wr(r,0x8004aa60,p+0xe,2,v));TRY(page(r,1,2,0x2d0,160,&v));TRY(rd(r,0x8004aa74,p+0x16,2,&c));
    u=c==(v&65535)?32:128;
    {
        static const uint32_t ips[]={0x8004aa90,0x8004aac8,0x8004ab00,0x8004ab3c,0x8004ab74,0x8004abb0,0x8004abe8,0x8004ac20};
        static const uint32_t bps[]={0x8004aa98,0x8004aad0,0x8004ab0c,0x8004ab44,0x8004ab7c,0x8004abb8,0x8004abf0,0x8004ac28};
        static const uint32_t wps[]={0x8004aac0,0x8004aaf8,0x8004ab34,0x8004ab6c,0x8004aba8,0x8004abe0,0x8004ac18,0x8004ac50};
        static const unsigned offsets[]={12,13,20,21,28,29,36,37};unsigned n;
        for(n=0;n<8;++n){TRY(live_packet(r,ips[n],bps[n],&p));v=(n&1)?(n>=4?191u:160u):u+((n==2||n==6)?31u:0u);TRY(wr(r,wps[n],p+offsets[n],1,v));}
    }
    ++r->out->packets;return NBA97_BODY_OK;
}
int nba97_game_player_marker_update(Nba97PlayerMarkerContext* c,Nba97PlayerMarkerProgress* p){
    Run r;int status;if(!c||!c->access||!p)return NBA97_BODY_ARGUMENT;
    memset(p,0,sizeof *p);r.in=c;r.out=p;status=update(&r);
    if(status==NBA97_BODY_OK){p->completed=1;p->stopped_pc=0;p->stopped_address=0;}
    return status;
}
