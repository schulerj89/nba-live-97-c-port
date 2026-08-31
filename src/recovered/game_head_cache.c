#include "game_head_cache.h"
#include <string.h>
static int fits(Nba97GameRenderBuffer b,size_t o,size_t n) {return b.data&&o<=b.size&&n<=b.size-o;}
static uint32_t w(const uint8_t* p) {return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int32_t s32(uint32_t u) {return u<=0x7fffffffu?(int32_t)u:-1-(int32_t)~u;}
static int16_t low16(int32_t v) {uint32_t u=(uint32_t)v&65535u;return (int16_t)(u<32768?(int32_t)u:(int32_t)u-65536);}
static int event(Nba97GameRenderIo io,void* ctx,int kind) {
    Nba97GameRenderIoEvent e;memset(&e,0,sizeof e);e.kind=kind;return io(ctx,&e)==1?1:NBA97_RENDER_IO_REFUSED;
}
static int sync_io(Nba97GameRenderIo io,void* ctx) {return event(io,ctx,NBA97_RENDER_SYNC_994F4);}
static Nba97GameRenderRect rect(Nba97GameHeadCache* s,unsigned index,int palette) {
    Nba97GameRenderRect r;
    r.x=palette?768:low16(s->xy[index][0]);r.y=palette?(int16_t)(246+index):low16(s->xy[index][1]);
    r.w=palette?256:38;r.h=palette?1:48;return r;
}
static int store(Nba97GameHeadCache* s,unsigned index,int palette,Nba97GameRenderBuffer dst,size_t base,Nba97GameRenderIo io,void* ctx) {
    Nba97GameRenderIoEvent e;size_t off=base+(palette?0xe78u:0x28u),length=palette?512u:3648u;int r;
    if(!fits(dst,off,length))return NBA97_RENDER_RESOURCE;
    memset(&e,0,sizeof e);e.kind=NBA97_RENDER_STORE_99780;e.rect=rect(s,index,palette);
    e.destination.data=dst.data+off;e.destination.size=length;r=io(ctx,&e)==1?1:NBA97_RENDER_IO_REFUSED;
    return r==1?sync_io(io,ctx):r;
}
static int move(Nba97GameHeadCache* s,unsigned from,unsigned to,int palette,Nba97GameRenderIo io,void* ctx) {
    Nba97GameRenderIoEvent e;int r;memset(&e,0,sizeof e);e.kind=NBA97_RENDER_MOVE_997E4;e.rect=rect(s,from,palette);
    e.x=palette?768:s->xy[to][0];e.y=palette?(int32_t)(246+to):s->xy[to][1];
    r=io(ctx,&e)==1?1:NBA97_RENDER_IO_REFUSED;return r==1?sync_io(io,ctx):r;
}
static int upload(Nba97GameHeadCache* s,unsigned slot,Nba97GameRenderIo io,void* ctx) {
    Nba97GameRenderIoEvent e;uint32_t offset,raw;int32_t rel;int64_t pal;int r;
    if(!fits(s->scratch,8,4))return NBA97_RENDER_RESOURCE;
    /* A3FEC returns NULL for a zero-record container; original dereferences
     * it at3877C. This native owner refuses instead of forging an image. */
    if(!w(s->scratch.data+8)||!fits(s->scratch,20,4))return NBA97_RENDER_RESOURCE;
    offset=w(s->scratch.data+20);if(!fits(s->scratch,offset,4))return NBA97_RENDER_RESOURCE;
    raw=w(s->scratch.data+offset);rel=s32((raw>>8)|(raw&0x80000000u?0xff000000u:0));
    pal=(int64_t)offset+rel+0x20e;
    if(pal<0||!fits(s->scratch,(size_t)pal,2))return NBA97_RENDER_RESOURCE;
    s->scratch.data[(size_t)pal]=0xf7;s->scratch.data[(size_t)pal+1]=0x6a;
    memset(&e,0,sizeof e);e.kind=NBA97_RENDER_UPLOAD_946B8;e.image.storage=s->scratch;e.image.offset=offset;
    e.x=s->xy[slot][0];e.y=s->xy[slot][1];e.clut_x=768;e.clut_y=(int32_t)(246+slot);
    r=io(ctx,&e)==1?1:NBA97_RENDER_IO_REFUSED;return r==1?sync_io(io,ctx):r;
}
static int copy(Nba97GameRenderBuffer from,size_t a,Nba97GameRenderBuffer to,size_t b) {
    if(!fits(from,a,0x107c)||!fits(to,b,0x107c))return NBA97_RENDER_RESOURCE;
    if(from.data+a!=to.data+b) {
        uintptr_t x=(uintptr_t)(from.data+a),y=(uintptr_t)(to.data+b);
        if(x>y?x-y<0x107cu:y-x<0x107cu)return NBA97_RENDER_RESOURCE;
        memcpy(to.data+b,from.data+a,0x107c);
    }
    return 1;
}
static int swap(Nba97GameHeadCache* s,unsigned side,unsigned target,unsigned source,Nba97GameRenderIo io,void* ctx) {
    int r;int32_t saved;unsigned first=side*5;size_t a,b;
    r=event(io,ctx,NBA97_RENDER_SERVICE_8892C);if(r!=1)return r;
    if(source>=s->count[side])return 1; /* Source checks count only AFTER service. */
    if(source<5) {
        /* Original does not check target<5 in this branch; an invalid target
         * outside ten owned coordinate rows refuses instead of inventing it. */
        if(first+target>=10)return NBA97_RENDER_RESOURCE;
        r=store(s,first+source,0,s->scratch,0,io,ctx);if(r!=1)return r;
        r=store(s,first+source,1,s->scratch,0,io,ctx);if(r!=1)return r;
        saved=s->current[side][source];
        r=move(s,first+target,first+source,0,io,ctx);if(r!=1)return r;
        r=move(s,first+target,first+source,1,io,ctx);if(r!=1)return r;
        s->current[side][source]=s->current[side][target];
        r=upload(s,first+target,io,ctx);if(r!=1)return r;
        s->current[side][target]=saved;
    }else if(target<5) {
        a=(source-5u)*0x107cu;
        r=copy(s->bench[side],a,s->scratch,0);if(r!=1)return r;
        saved=s->current[side][source];
        r=store(s,first+target,0,s->bench[side],a,io,ctx);if(r!=1)return r;
        r=store(s,first+target,1,s->bench[side],a,io,ctx);if(r!=1)return r;
        s->current[side][source]=s->current[side][target];
        r=upload(s,first+target,io,ctx);if(r!=1)return r;
        s->current[side][target]=saved;
    }else {
        a=(source-5u)*0x107cu;b=(target-5u)*0x107cu;
        r=copy(s->bench[side],a,s->scratch,0);if(r!=1)return r;
        /* 38DF4/38E40/38E74 sync only home. The away branch deliberately
         * lacks all three calls; do not make the paths artificially symmetric. */
        if(!side){r=sync_io(io,ctx);if(r!=1)return r;}
        saved=s->current[side][source];
        r=copy(s->bench[side],b,s->bench[side],a);if(r!=1)return r;
        if(!side){r=sync_io(io,ctx);if(r!=1)return r;}
        s->current[side][source]=s->current[side][target];
        r=copy(s->scratch,0,s->bench[side],b);if(r!=1)return r;
        if(!side){r=sync_io(io,ctx);if(r!=1)return r;}
        s->current[side][target]=saved;
    }
    return 1;
}
int nba97_game_head_cache(Nba97GameHeadCache* s,int32_t argument,Nba97GameRenderIo io,void* ctx) {
    unsigned target,side,source;int r;int32_t wanted;
    if(!s||!io)return NBA97_RENDER_ARGUMENT;
    r=sync_io(io,ctx);if(r!=1)return r;
    if(argument>=0) {
        side=argument<12?0u:1u;wanted=argument<12?argument:argument-12;
        for(source=0;source<12&&s->current[side][source]!=wanted;++source){}
        if(source==12)return NBA97_RENDER_SEARCH_OUTSIDE_OWNER;
        return swap(s,side,0,source,io,ctx);
    }
    for(target=0;target<12;++target)for(side=0;side<2;++side) {
        if(target>=s->count[side])continue;
        wanted=s->lineup[side][target];if(s->current[side][target]==wanted)continue;
        for(source=target+1;source<12&&s->current[side][source]!=wanted;++source){}
        if(source==12)return NBA97_RENDER_SEARCH_OUTSIDE_OWNER;
        r=swap(s,side,target,source,io,ctx);if(r!=1)return r;
    }
    return NBA97_RENDER_COMPLETE;
}
