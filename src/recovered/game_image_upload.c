#include "game_image_upload.h"
#include <string.h>

typedef struct Context {
    Nba97GameImageUploadState* state;
    Nba97GameImageTransferIo io;
    void* user;
    Nba97GameImageUploadProgress* progress;
    size_t budget;
} Context;
static int32_t s32(uint32_t u) {return u<=0x7fffffffu?(int32_t)u:-1-(int32_t)~u;}
static int32_t s16(uint32_t u) {return u<32768u?(int32_t)u:(int32_t)u-65536;}
static int16_t low16(uint32_t u) {return (int16_t)s16(u&65535u);}
static int32_t sar(uint32_t u,unsigned shift) {
    return s32((u>>shift)|(u&0x80000000u?(~0u<<(32-shift)):0));
}
static int add(int64_t base,int64_t delta,int64_t* out) {
    if((delta>0&&base>INT64_MAX-delta)||(delta<0&&base<INT64_MIN-delta))return NBA97_IMAGE_RESOURCE;
    *out=base+delta;return NBA97_IMAGE_COMPLETE;
}
static int access(Context* c,Nba97GameImageReference r,int64_t delta,size_t n,int write,uint8_t** out) {
    Nba97GameImageMemory* m=r.memory;size_t i;int unknown=0;int64_t pos;int result=add(r.offset,delta,&pos);
    if(result!=1)return result;
    c->progress->stopped_offset=pos;
    if(!m||!m->data||pos<0||(uint64_t)pos>SIZE_MAX||(size_t)pos>m->size||n>m->size-(size_t)pos)return NBA97_IMAGE_RESOURCE;
    /* Native metadata validation applies to the entire reached access, even
     * write-only fields. Do not erase malformed metadata with a source store,
     * or let an earlier unknown byte hide a later noncanonical knownness byte.
     * This is not a preflight of later source accesses or the allocation. */
    if(m->known)for(i=0;i<n;++i) {
        if(m->known[(size_t)pos+i]>1)return NBA97_IMAGE_ARGUMENT;
        if(!m->known[(size_t)pos+i])unknown=1;
    }
    if(n>1) {
        if(!m->address_mod4_known)return NBA97_IMAGE_UNKNOWN;
        if(((uint64_t)pos+m->address_mod4)&(n-1u))return NBA97_IMAGE_ALIGNMENT_TRAP;
    }
    if(!write&&unknown)return NBA97_IMAGE_UNKNOWN;
    *out=m->data+(size_t)pos;return NBA97_IMAGE_COMPLETE;
}
static int read_value(Context* c,Nba97GameImageReference r,int64_t delta,size_t n,uint32_t* value) {
    uint8_t* p;size_t i;uint32_t v=0;int result=access(c,r,delta,n,0,&p);if(result!=1)return result;
    for(i=0;i<n;++i)v|=(uint32_t)p[i]<<(i*8);
    *value=v;return NBA97_IMAGE_COMPLETE;
}
static int write_value(Context* c,Nba97GameImageReference r,int64_t delta,size_t n,uint32_t value) {
    uint8_t* p;size_t i;int result=access(c,r,delta,n,1,&p);if(result!=1)return result;
    for(i=0;i<n;++i) {
        p[i]=(uint8_t)(value>>(i*8));
        if(r.memory->known)r.memory->known[(size_t)(r.offset+delta)+i]=1;
    }
    return NBA97_IMAGE_COMPLETE;
}
int nba97_game_image_bits(uint8_t byte,uint32_t* bits) {
    uint32_t value;if(!bits)return NBA97_IMAGE_ARGUMENT;
    /* A3BF8 masks80/08. Its fallback BEQ has JRra in its delay slot at
     * A3C34/A3C38. Do not silently impose intended pseudocode or our custom
     * oracle's nested-delay behavior on an unproved original CPU domain. */
    switch(byte&0x77u) {
    case 0x40:value=4;break;
    case 0x41:value=8;break;
    case 0x42:case 0x23:value=16;break;
    case 0x43:value=24;break;
    case 0x44:value=1;break;
    default:return NBA97_IMAGE_FORMAT_UNRESOLVED;
    }
    *bits=value;return NBA97_IMAGE_COMPLETE;
}
void nba97_game_image_prepare_rect(Nba97GameImageRect* rect) {
    if(rect&&((uint16_t)rect->w&1u))rect->h=low16((uint16_t)rect->h|1u);
}
static int transfer(Context* c,Nba97GameImageReference source,Nba97GameImageRect* rect,int wrapped) {
    Nba97GameImageTransfer event;
    if(wrapped)nba97_game_image_prepare_rect(rect);
    event.rect=*rect;event.source=source;event.through_944f4=(uint8_t)wrapped;
    event.footprint_known=(uint8_t)(rect->w>0&&rect->h>0);
    event.pixel_words=event.footprint_known?(uint32_t)rect->w*(uint32_t)rect->h:0;
    event.cpu_words=(event.pixel_words+1u)/2u;
    c->progress->stopped_offset=source.offset;
    if(c->io(c->user,&event)!=1)return NBA97_IMAGE_IO_REFUSED;
    ++c->progress->uploads_completed;
    /* 94524 writes AFTER9971C returns. Its old value need not be known.
     * 946B8's two direct9971C calls do not perform this write. */
    if(wrapped){c->state->pending_d7b14=1;c->state->pending_known=1;}
    return NBA97_IMAGE_COMPLETE;
}
static int chain(Context* c,Nba97GameImageReference image,Nba97GameImagePlacement placement) {
    int result;uint32_t byte,width,height,old_x,raw,bits,product;Nba97GameImageRect rect;Nba97GameImageReference pixels;
    if(!image.memory)return NBA97_IMAGE_COMPLETE; /* 9456C source NULL branch. */
    for(;;) {
        c->progress->stopped_offset=image.offset;
        if(c->progress->headers_visited>=c->budget)return NBA97_IMAGE_HEADER_LIMIT;
        ++c->progress->headers_visited;
        result=read_value(c,image,0,1,&byte);if(result!=1)return result;
        if((byte&0xf7u)>=0x40u&&(byte&0xf7u)<0x44u) {
            result=read_value(c,image,12,2,&old_x);if(result!=1)return result;
            result=read_value(c,image,0,1,&byte);if(result!=1)return result;
            result=write_value(c,image,14,2,(uint32_t)placement.y);if(result!=1)return result;
            /* 945B0 preserves old top-coordinate bits, even when x supplies
             * its own top bits. Do not replace this OR with an x-bit clamp. */
            result=write_value(c,image,12,2,(uint32_t)placement.x|(old_x&0xc000u));if(result!=1)return result;
            result=write_value(c,image,0,1,byte|8u);if(result!=1)return result;
            result=read_value(c,image,0,1,&byte);if(result!=1)return result;
            result=nba97_game_image_bits((uint8_t)byte,&bits);if(result!=1)return result;
            result=read_value(c,image,4,2,&width);if(result!=1)return result;
            product=(uint32_t)s16(width)*bits;
            raw=product+15u;
            /* 945EC negative rounding is literal wrapped product+30, not an
             * unsigned ceil division or a positive-width repair. */
            if(s32(raw)<0)raw=product+30u;
            rect.x=low16((uint32_t)placement.x);rect.y=low16((uint32_t)placement.y);
            rect.w=low16((uint32_t)sar(raw,4));
            result=read_value(c,image,6,2,&height);if(result!=1)return result;
            rect.h=low16(height);pixels=image;result=add(image.offset,16,&pixels.offset);if(result!=1)return result;
            result=transfer(c,pixels,&rect,1);if(result!=1)return result;
        }else if((byte&0xf7u)==0x23u) {
            result=read_value(c,image,0,1,&byte);if(result!=1)return result;
            result=write_value(c,image,12,2,(uint32_t)placement.clut_x);if(result!=1)return result;
            result=write_value(c,image,14,2,(uint32_t)placement.clut_y);if(result!=1)return result;
            result=write_value(c,image,0,1,byte|8u);if(result!=1)return result;
            result=read_value(c,image,4,2,&width);if(result!=1)return result;
            rect.x=low16((uint32_t)placement.clut_x);rect.y=low16((uint32_t)placement.clut_y);
            rect.w=low16(width);rect.h=1;
            /* 94634/9463C suppress transfer only when both FULL argument
             * words are zero. Header writes/width read still happen. */
            if(placement.clut_x||placement.clut_y) {
                pixels=image;result=add(image.offset,16,&pixels.offset);if(result!=1)return result;
                result=transfer(c,pixels,&rect,1);if(result!=1)return result;
            }
        }
        /* Read after callbacks/header writes: links may have changed. Signed
         * backward links and aliases are intentional. Cycles reach only the
         * caller's explicit native budget, not an invented source terminator. */
        result=read_value(c,image,0,4,&raw);if(result!=1)return result;
        if(!(raw&0xffffff00u))return NBA97_IMAGE_COMPLETE;
        result=add(image.offset,sar(raw,8),&image.offset);if(result!=1)return result;
    }
}
static int valid(Nba97GameImageUploadState* state,Nba97GameImageReference image,
    Nba97GameImageTransferIo io,Nba97GameImageUploadProgress* progress) {
    if(!state||!io||!progress||state->pending_known>1)return 0;
    if(!image.memory)return image.offset==0;
    return image.memory->address_mod4<4&&image.memory->address_mod4_known<2;
}
static Context start(Nba97GameImageUploadState* state,Nba97GameImageTransferIo io,void* user,
    Nba97GameImageUploadProgress* progress,size_t budget) {
    Context c;c.state=state;c.io=io;c.user=user;c.progress=progress;c.budget=budget;
    memset(progress,0,sizeof *progress);return c;
}
int nba97_game_image_upload_chain(Nba97GameImageUploadState* state,Nba97GameImageReference image,
    Nba97GameImagePlacement placement,size_t budget,Nba97GameImageTransferIo io,void* user,
    Nba97GameImageUploadProgress* progress) {
    Context c;if(!valid(state,image,io,progress))return NBA97_IMAGE_ARGUMENT;
    c=start(state,io,user,progress,budget);return chain(&c,image,placement);
}
int nba97_game_image_upload_rect(Nba97GameImageUploadState* state,Nba97GameImageReference pixels,
    Nba97GameImageRect* rect,Nba97GameImageTransferIo io,void* user,Nba97GameImageUploadProgress* progress) {
    Context c;if(!rect||!valid(state,pixels,io,progress))return NBA97_IMAGE_ARGUMENT;
    c=start(state,io,user,progress,0);return transfer(&c,pixels,rect,1);
}
int nba97_game_image_upload(Nba97GameImageUploadState* state,Nba97GameImageReference image,
    Nba97GameImagePlacement placement,size_t budget,Nba97GameImageTransferIo io,void* user,
    Nba97GameImageUploadProgress* progress) {
    Context c;uint32_t byte,width,height,bits,rounded,product,end_y;int32_t row_bytes,saved_height;
    int result;Nba97GameImageRect rect;Nba97GameImageReference pixels;
    if(!valid(state,image,io,progress))return NBA97_IMAGE_ARGUMENT;
    c=start(state,io,user,progress,budget);
    result=read_value(&c,image,0,1,&byte);if(result!=1)return result;
    result=nba97_game_image_bits((uint8_t)byte,&bits);if(result!=1)return result;
    result=read_value(&c,image,4,2,&width);if(result!=1)return result;
    rounded=((uint32_t)s16(width)*bits+15u)&~15u;
    row_bytes=sar(rounded,3);
    if((uint32_t)row_bytes&2u) {
        result=read_value(&c,image,6,2,&height);if(result!=1)return result;
        if(!(height&1u)) {
            saved_height=s16(height);
            product=((uint32_t)saved_height-1u)*(uint32_t)row_bytes;
            end_y=(uint32_t)placement.y+(uint32_t)saved_height;
            rect.x=low16((uint32_t)placement.x);rect.y=low16(end_y-2u);rect.w=1;rect.h=2;
            pixels=image;result=add(image.offset,s32(product+14u),&pixels.offset);if(result!=1)return result;
            result=transfer(&c,pixels,&rect,0);if(result!=1)return result;
            rect.x=low16((uint32_t)placement.x+1u);rect.y=low16(end_y-1u);
            rect.w=low16((uint32_t)sar(rounded,4)-1u);rect.h=1;
            pixels=image;result=add(image.offset,s32(product+18u),&pixels.offset);if(result!=1)return result;
            result=transfer(&c,pixels,&rect,0);if(result!=1)return result;
            /* 947A8 rereads height AFTER both direct uploads. Callback changes
             * therefore affect this decrement, but restoration uses saved s1. */
            result=read_value(&c,image,6,2,&height);if(result!=1)return result;
            result=write_value(&c,image,6,2,height-1u);if(result!=1)return result;
            progress->temporary_height_active=1;
            result=chain(&c,image,placement);if(result!=1)return result;
            result=write_value(&c,image,6,2,(uint32_t)saved_height);if(result!=1)return result;
            progress->temporary_height_active=0;return NBA97_IMAGE_COMPLETE;
        }
    }
    return chain(&c,image,placement);
}
