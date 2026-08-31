#include "game_render_textures.h"
#include <string.h>

static int32_t s32(uint32_t v) { return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)~v; }
static int32_t s16(uint32_t v) { return v<0x8000u?(int32_t)v:(int32_t)v-65536; }
static int32_t sar(int32_t v,unsigned n) {
    uint32_t u=(uint32_t)v;return s32((u>>n)|(v<0?(~0u<<(32-n)):0));
}
static int fits(Nba97GameRenderBuffer b,size_t o,size_t n) { return b.data&&o<=b.size&&n<=b.size-o; }
static uint16_t h(const uint8_t* p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static uint32_t w(const uint8_t* p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void ph(uint8_t* p,uint16_t v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }
static void pw(uint8_t* p,uint32_t v) { ph(p,(uint16_t)v);ph(p+2,(uint16_t)(v>>16)); }
static uint8_t* at(Nba97GameRenderImage im,int64_t off,size_t n) {
    int64_t pos;if(im.offset>(size_t)INT64_MAX)return NULL;
    if(off>0&&(int64_t)im.offset>INT64_MAX-off)return NULL;
    pos=(int64_t)im.offset+off;
    return pos>=0&&fits(im.storage,(size_t)pos,n)?im.storage.data+(size_t)pos:NULL;
}
static Nba97GameRenderBuffer player(Nba97GameRenderTextures* s,unsigned i) {
    Nba97GameRenderBuffer none={NULL,0};return s->player[i]?s->player[i]->record:none;
}
static int emit(Nba97GameRenderIo io,void* ctx,Nba97GameRenderIoEvent* e) { return io(ctx,e)==1?1:NBA97_RENDER_IO_REFUSED; }
static int sync_io(Nba97GameRenderIo io,void* ctx) {
    Nba97GameRenderIoEvent e;memset(&e,0,sizeof e);e.kind=NBA97_RENDER_SYNC_994F4;return emit(io,ctx,&e);
}
static int upload(Nba97GameRenderImage image,int32_t x,int32_t y,int32_t cx,int32_t cy,Nba97GameRenderIo io,void* ctx) {
    Nba97GameRenderIoEvent e;int r;
    if(!at(image,0,16))return NBA97_RENDER_RESOURCE;
    memset(&e,0,sizeof e);e.kind=NBA97_RENDER_UPLOAD_946B8;e.image=image;
    e.x=x;e.y=y;e.clut_x=cx;e.clut_y=cy;r=emit(io,ctx,&e);
    return r==1?sync_io(io,ctx):r;
}
static unsigned glyph(uint8_t c) {
    return c>='a'&&c<='z'?(unsigned)(c-'a'):c>='A'&&c<='Z'?(unsigned)(c-'A'):26u;
}
int nba97_game_render_name_tracked(Nba97GameRenderTextures* s,unsigned i,Nba97GameRenderIo io,void* ctx,uint8_t* centers_written) {
    Nba97GameRenderBuffer p;uint8_t *out,*g,*poly0,*poly1;unsigned j;int32_t half;
    if(!centers_written)return NBA97_RENDER_ARGUMENT;
    *centers_written=0;
    if(!s||!io||i>=10)return NBA97_RENDER_ARGUMENT;
    out=at(s->name_scratch,16,1500);if(!out)return NBA97_RENDER_RESOURCE;
    memset(out,255,1500);s->name_position=0;s->name_cursor=0;s->name_zero=0;s->name_spacing=0;
    p=player(s,i);
    for(;;) {
        uint8_t c;if(!fits(p,0x29+s->name_position,1))return NBA97_RENDER_RESOURCE;
        c=p.data[0x29+s->name_position];if(!c)break;
        s->name_glyph=glyph(c);
        if(s->name_glyph<26) {
            g=at(s->glyph[s->name_glyph],4,2);if(!g)return NBA97_RENDER_RESOURCE;
            s->name_cursor+=2u+(uint32_t)s16(h(g));
        }else s->name_cursor+=6;
        ++s->name_spacing;++s->name_position; /* Original DCF14 byte wraps. */
    }
    if(s32(s->name_cursor)<91)s->name_spacing=2;
    else {s->name_cursor+=1u-s->name_spacing;s->name_spacing=1;}
    half=sar(s32(s->name_cursor),1);s->name_cursor=(uint32_t)half;
    for(j=0;j<4;j+=2) {
        if(s->bypass_name_uv) {
            s->name_center[i][j]=(uint32_t)half;*centers_written|=(uint8_t)(1u<<j);
            s->name_center[i][j+1]=(uint32_t)half;*centers_written|=(uint8_t)(1u<<(j+1));
        }else {
            if(!fits(s->name_polygon[i][j],0,29)||!fits(s->name_polygon[i][j+1],0,29))return NBA97_RENDER_RESOURCE;
            poly0=s->name_polygon[i][j].data;poly1=s->name_polygon[i][j+1].data;
            /* Both centers read before this pair's writes. A later pair can
             * alias an earlier pair and must observe its changed UVs. */
            s->name_center[i][j]=(uint32_t)(sar((int32_t)poly0[28]-poly0[12],1)+poly0[12]+1);
            *centers_written|=(uint8_t)(1u<<j);
            s->name_center[i][j+1]=(uint32_t)(sar((int32_t)poly1[28]-poly1[12],1)+poly1[12]+1);
            *centers_written|=(uint8_t)(1u<<(j+1));
            poly0[12]=(uint8_t)(s->name_center[i][j]-(uint32_t)half);
            poly0[20]=(uint8_t)(s->name_center[i][j]-(uint32_t)half);
            poly0[28]=(uint8_t)(s->name_center[i][j]+(uint32_t)half-1);
            poly1[12]=(uint8_t)(s->name_center[i][j+1]+(uint32_t)half-1);
            poly1[20]=(uint8_t)(s->name_center[i][j+1]+(uint32_t)half-1);
            poly1[28]=(uint8_t)(s->name_center[i][j+1]-(uint32_t)half);
        }
    }
    s->name_cursor=50u-(uint32_t)half;s->name_position=0;p=player(s,i);
    for(;;) {
        uint8_t c;if(!fits(p,0x29+s->name_position,1))return NBA97_RENDER_RESOURCE;
        c=p.data[0x29+s->name_position];if(!c)break;
        s->name_glyph=glyph(c);
        if(s->name_glyph<26) {
            Nba97GameRenderImage im=s->glyph[s->name_glyph];int32_t x=0;
            g=at(im,4,4);if(!g)return NBA97_RENDER_RESOURCE;
            while(x<s16(h(g))) {
                int32_t y=0;
                while(y<s16(h(g+2))) {
                    int32_t stride=(sar(s16(h(g))-1,2)+1)*4;
                    int32_t src=sar(s32((uint32_t)x+(uint32_t)y*(uint32_t)stride),1);
                    uint32_t xpos=s->name_cursor+(uint32_t)x;
                    int32_t dst=sar(s32(xpos+(uint32_t)y*100u),1);
                    uint8_t* a=at(im,16+(int64_t)src,1);
                    uint8_t* b=at(s->name_scratch,16+(int64_t)dst,1);
                    if(!a||!b)return NBA97_RENDER_RESOURCE;
                    s->name_nibble=(uint8_t)((*a>>(x&1?4:0))&15);
                    if(xpos&1) {s->name_nibble=(uint8_t)(s->name_nibble<<4);*b=(uint8_t)((*b&15)+s->name_nibble);}
                    else *b=(uint8_t)((*b&240)+s->name_nibble);
                    ++y;
                }
                ++x;
            }
            s->name_cursor+=(uint32_t)s16(h(g))+s->name_spacing;
        }else s->name_cursor+=6;
        ++s->name_position;
    }
    out=at(s->name_scratch,0,4);if(!out)return NBA97_RENDER_RESOURCE;
    /* 4EA50 deliberately clears upper24 bits; do not retain authored flags. */
    pw(out,out[0]);
    return upload(s->name_scratch,s->name_xy[i][0],s->name_xy[i][1],0,0,io,ctx);
}
int nba97_game_render_name(Nba97GameRenderTextures* s,unsigned i,Nba97GameRenderIo io,void* ctx) {
    uint8_t centers_written;
    return nba97_game_render_name_tracked(s,i,io,ctx,&centers_written);
}
static int number_digit(Nba97GameRenderTextures* s,unsigned side,int32_t digit,int32_t x,int32_t y,Nba97GameRenderIo io,void* ctx) {
    if(digit<0||digit>=10)return NBA97_RENDER_RESOURCE;
    return upload(s->digit[side][digit],x,y,0,0,io,ctx);
}
int nba97_game_render_number(Nba97GameRenderTextures* s,unsigned i,Nba97GameRenderIo io,void* ctx) {
    unsigned side;Nba97GameRenderBuffer p;Nba97GameRenderImage base;uint8_t* q;int32_t value,x,y,cx,cy;int r;
    if(!s||!io||i>=10)return NBA97_RENDER_ARGUMENT;
    side=i<5?0u:1u;cx=s->number_clut_xy[i][0];cy=s->number_clut_xy[i][1];
    base=s->number_base[side];q=at(base,0,4);if(!q)return NBA97_RENDER_RESOURCE;
    {int32_t relative=sar(s32(w(q)),8);int64_t offset;
     if(relative>0&&(int64_t)base.offset>INT64_MAX-relative)return NBA97_RENDER_RESOURCE;
     offset=(int64_t)base.offset+relative;
     if(offset<0||(uint64_t)offset>SIZE_MAX)return NBA97_RENDER_RESOURCE;
     s->number_palette=base;s->number_palette.offset=(size_t)offset;}
    p=player(s,i);if(!fits(p,7,1))return NBA97_RENDER_RESOURCE;
    value=p.data[7]<128?p.data[7]:(int32_t)p.data[7]-256;s->number_value=value;
    x=s->number_xy[i][0];y=s->number_xy[i][1];
    /* 539FC treats byteFF as two zeros; other negative signed bytes are not
     * unsigned jersey numbers. Negative unowned table indices refuse. */
    if(value==-1) {
        r=number_digit(s,side,0,x,y,io,ctx);if(r!=1)return r;
        r=number_digit(s,side,0,s32((uint32_t)x+8),y,io,ctx);if(r!=1)return r;
    }else if(value>=10) {
        r=number_digit(s,side,(value/10)%10,x,y,io,ctx);if(r!=1)return r;
        r=number_digit(s,side,s->number_value%10,s32((uint32_t)x+8),y,io,ctx);if(r!=1)return r;
    }else {
        int32_t digit=value%10;unsigned transparent=0;Nba97GameRenderImage scratch;
        if(digit<0||digit>=10)return NBA97_RENDER_RESOURCE;
        q=at(s->digit[side][digit],0,0x410);
        if(!q||!fits(s->number_scratch,0,0x410))return NBA97_RENDER_RESOURCE;
        /* AA468 copies source to destination. Owned source/destination may be
         * identical; partial overlaps are outside this resource contract. */
        if(q!=s->number_scratch.data) {
            uintptr_t a=(uintptr_t)q,b=(uintptr_t)s->number_scratch.data;
            if(a>b?a-b<0x410u:b-a<0x410u)return NBA97_RENDER_RESOURCE;
            memcpy(s->number_scratch.data,q,0x410);
        }
        while(transparent<16) {
            q=at(s->number_palette,16+transparent*2,2);if(!q)return NBA97_RENDER_RESOURCE;
            if(!h(q))break;++transparent;
        }
        /* No transparent entry is original index16 => packed byte0x10, not
         * a corrected fallback palette index. 53C00/53E64 preserve this bug. */
        memset(s->number_scratch.data+16,(uint8_t)(transparent*17u),1024);
        pw(s->number_scratch.data,s->number_scratch.data[0]);
        scratch.storage=s->number_scratch;scratch.offset=0;
        r=upload(scratch,x,y,0,0,io,ctx);if(r!=1)return r;
        r=upload(scratch,s32((uint32_t)x+8),y,0,0,io,ctx);if(r!=1)return r;
        r=number_digit(s,side,s->number_value%10,s32((uint32_t)x+4),y,io,ctx);if(r!=1)return r;
    }
    /* 4D8C0 passes these on the stack;539FC retains both across uploads. */
    return upload(s->number_palette,0,0,cx,cy,io,ctx);
}
int nba97_game_render_palette(Nba97GameRenderTextures* s,unsigned i,Nba97GameRenderIo io,void* ctx) {
    Nba97GameRenderBuffer p;Nba97GameRenderImage target;unsigned side,j;size_t source;uint8_t *a,*b;
    if(!s||!io||i>=10)return NBA97_RENDER_ARGUMENT;
    side=i<5?0u:1u;target=s->team_palette[side];p=player(s,i);
    if(!fits(p,11,1))return NBA97_RENDER_RESOURCE;
    source=side*0xc60u+(p.data[11]>>1)*0x210u+0x1b0u;
    for(j=0;j<48;++j) {
        a=at(target,0x1b0+j*2,2);if(!a||!fits(s->skin_bank,source+j*2,2))return NBA97_RENDER_RESOURCE;
        ph(a,h(s->skin_bank.data+source+j*2));
    }
    for(j=0;j<256;++j) {
        uint16_t v;b=at(target,16+j*2,2);if(!b)return NBA97_RENDER_RESOURCE;v=h(b);
        /* Original4DBB8 replaces zero with9084 and mutates the shared template. */
        ph(b,v?(uint16_t)(v|0x8000u):0x9084u);
    }
    return upload(target,0,0,512,(int32_t)(3*i+192),io,ctx);
}
int nba97_game_render_patch(Nba97GameRenderTextures* s,unsigned i,unsigned patch,Nba97GameRenderIo io,void* ctx) {
    Nba97GameRenderIoEvent e;int r;if(!s||!io||i>=10||patch>=24)return NBA97_RENDER_ARGUMENT;
    memset(&e,0,sizeof e);e.kind=NBA97_RENDER_MOVE_997E4;e.rect=s->patch_rect[patch];
    e.x=s->patch_rect[24+i].x;e.y=s->patch_rect[24+i].y;r=emit(io,ctx,&e);if(r!=1)return r;
    return upload(s->patch_palette[patch],0,0,s->patch_clut_xy[i][0],s->patch_clut_xy[i][1],io,ctx);
}
int nba97_game_render_textures(Nba97GameRenderTextures* s,Nba97GameRenderIo io,void* ctx,unsigned* completed) {
    unsigned i,j;int r;if(!s||!io||!completed)return NBA97_RENDER_ARGUMENT;*completed=0;
    for(i=0;i<10;++i) {
        Nba97GameRenderBuffer p;
        r=nba97_game_render_name(s,i,io,ctx);if(r!=1)return r;
        r=nba97_game_render_number(s,i,io,ctx);if(r!=1)return r;
        r=nba97_game_render_palette(s,i,io,ctx);if(r!=1)return r;
        p=player(s,i);if(!fits(p,9,1))return NBA97_RENDER_RESOURCE;s->height[i]=(uint32_t)p.data[9]*624u;
        for(j=0;j<24;++j) {
            p=player(s,i);if(!fits(p,0,2))return NBA97_RENDER_RESOURCE;
            if(s->patch_id[j]==s16(h(p.data))) {r=nba97_game_render_patch(s,i,j,io,ctx);if(r!=1)return r;break;}
        }
        r=sync_io(io,ctx);if(r!=1)return r;*completed=i+1;
    }
    return NBA97_RENDER_COMPLETE;
}
int nba97_game_render_bindings(Nba97GameRenderBindings* out,const uint32_t a[12],const uint32_t b[12]) {
    static const int16_t offsets[12]={-0xba,-0x120,-0x170,-0x1a0,-0x150,-0x194,-0x14c,-0x18c,INT16_MIN,0,-0xa4,-0xb8};
    unsigned i;if(!out||!a||!b)return NBA97_RENDER_ARGUMENT;
    memcpy(out->entity_offset,offsets,sizeof offsets);out->render_first=0;
    for(i=0;i<12;++i){out->copied20b8c[i]=a[i];out->copied20bbc[i]=b[i];}
    return NBA97_RENDER_COMPLETE;
}
