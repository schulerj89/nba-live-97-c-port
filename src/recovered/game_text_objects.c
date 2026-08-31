#include "game_text_objects.h"
#include <string.h>

typedef struct Run {Nba97GameTextContext* api;Nba97GameTextProgress* p;const Nba97GameTextSpan* text;} Run;
static int32_t s32(uint32_t v){return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)~v;}
static int32_t s16(uint32_t v){v&=65535u;return v<32768u?(int32_t)v:(int32_t)v-65536;}
static int32_t s8(uint32_t v){v&=255u;return v<128u?(int32_t)v:(int32_t)v-256;}
static uint32_t asr(uint32_t v,unsigned n){return (v>>n)|((v&0x80000000u)?(~0u<<(32-n)):0);}
static int step(Run* r,uint32_t at){r->p->stopped_address=at;r->p->stopped_in_text=0;if(r->p->steps>=r->api->step_budget)return NBA97_TEXT_LIMIT;++r->p->steps;return 1;}
static int bytes(Run* r,uint32_t at,size_t size,uint8_t** data,uint8_t** known){
    size_t i,j;int result=step(r,at);if(result!=1)return result;
    for(i=0;i<r->api->memory.count;++i){
        Nba97GameTextRegion* m=&r->api->memory.region[i];uint64_t off=(uint64_t)at-m->base;
        if(at<m->base||off>m->size||size>m->size-(size_t)off)continue;
        *data=m->data+(size_t)off;*known=m->known?m->known+(size_t)off:NULL;
        if(*known)for(j=0;j<size;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return 1;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read_value(Run* r,uint32_t at,unsigned n,uint32_t* out){
    uint8_t *p,*known;unsigned i;uint32_t value=0;int result;
    if(at&(n-1u)){r->p->stopped_address=at;r->p->stopped_in_text=0;return NBA97_TEXT_ALIGNMENT_TRAP;}
    result=bytes(r,at,n,&p,&known);if(result!=1)return result;
    if(known)for(i=0;i<n;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<n;++i)value|=(uint32_t)p[i]<<(i*8);*out=value;return 1;
}
static int write_value(Run* r,uint32_t at,unsigned n,uint32_t value){
    uint8_t *p,*known;unsigned i;int result;
    if(at&(n-1u)){r->p->stopped_address=at;r->p->stopped_in_text=0;return NBA97_TEXT_ALIGNMENT_TRAP;}
    result=bytes(r,at,n,&p,&known);if(result!=1)return result;
    for(i=0;i<n;++i){p[i]=(uint8_t)(value>>(i*8));if(known)known[i]=1;}return 1;
}
static int text_byte(Run* r,uint32_t at,uint32_t* out){
    int result;if(!r->text)return read_value(r,at,1,out);
    result=step(r,at);r->p->stopped_in_text=1;if(result!=1)return result;
    if(!r->text->data||at>=r->text->size)return NBA97_TEXT_RESOURCE;
    if(r->text->known){if(r->text->known[at]>1)return NBA97_TEXT_ARGUMENT;if(!r->text->known[at])return NBA97_TEXT_UNKNOWN;}
    *out=r->text->data[at];return 1;
}
#define TRY(expr) do {int text_result=(expr);if(text_result!=1)return text_result;} while(0)
#define R8(at,v) TRY(read_value(r,(at),1,&(v)))
#define R16(at,v) TRY(read_value(r,(at),2,&(v)))
#define R32(at,v) TRY(read_value(r,(at),4,&(v)))
#define T8(at,v) TRY(text_byte(r,(at),&(v)))
#define W8(at,v) TRY(write_value(r,(at),1,(uint32_t)(v)))
#define W16(at,v) TRY(write_value(r,(at),2,(uint32_t)(v)))
#define W32(at,v) TRY(write_value(r,(at),4,(uint32_t)(v)))
static int start(Nba97GameTextContext* c,Nba97GameTextProgress* p,Run* r){
    size_t i,j;if(!c||!p||(!c->memory.region&&c->memory.count))return 0;
    for(i=0;i<c->memory.count;++i){
        const Nba97GameTextRegion* a=&c->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return 0;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&c->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return 0;}
    }
    memset(p,0,sizeof *p);r->api=c;r->p=p;r->text=NULL;return 1;
}
static int callback(Run* r,int kind,uint32_t target,uint32_t object,uint32_t count){
    Nba97GameTextEvent e;e.kind=kind;e.target=target;e.object=object;e.count=count;
    if(!r->api->io||r->api->io(r->api->user,&e)!=1)return NBA97_TEXT_IO_REFUSED;
    ++r->p->callbacks_completed;return 1;
}
static int reset_packet(Run* r,uint32_t object,uint32_t count){
    uint32_t mode,target,table;R8(0x800c55c2u,mode);
    if(mode>=2){R32(0x800c55bcu,target);TRY(callback(r,NBA97_TEXT_DIAGNOSTIC_99960,target,object,count));}
    R32(0x800c55b8u,table);R32(table+0x2cu,target);
    TRY(callback(r,NBA97_TEXT_PACKET_CLEAR_DISPATCH,target,object,count));
    /*999DC overwrites the first word afterdispatch, even sourceSDKfailure.
     * Nativecallbackrefusal is distinct and must stop before this store. */
    W32(object,0x000c567cu);return 1;
}
static int reset_group(Run* r,int32_t group){
    uint32_t style,index,pool;uint32_t g=(uint32_t)s16((uint32_t)group),field;
    R32(0x800b2048u,style);field=g==1?0x34u:g==2?0x38u:g==3?0x3cu:0x30u;
    R16(style+field,index);
    while(s16(index)>=0){
        uint32_t offset=index<<6;R32(style+0x10u,pool);
        /*30658 only deactivates. It does not unlink/free or write negative
         * status.30D18's free-slot search requires negative status. */
        if(pool+offset)W16(pool+offset+0x12u,0);
        ++r->p->objects_reset;R32(style+0x10u,pool);R16(pool+offset+0x1cu,index);
    }
    return 1;
}
static int allocate(Run* r,int32_t glyph_count,uint32_t* out){
    uint32_t style,map,p,limit,v,base,end,scan;int32_t need=s32(asr((uint32_t)s16((uint32_t)glyph_count)-1u,1));
    R32(0x800b2048u,style);R32(style+0x1cu,map);p=map;
    if(!need){
        R8(p,v);while(v){++p;R8(p,v);}
        R32(style+0x1cu,map);R16(style+0x20u,limit);
        /*2EFE4 is strict<, so offset==limit is still writable. */
        if(s16(limit)<s32(p-map)){*out=0;return 1;}W8(p,1);
    }else{
        for(;;){
            do{R8(p,v);++p;}while(v);--p;scan=0;end=p;
            if(need>=0){
                R8(p,v);
                if(!v){
                    scan=1;end=p+1u;
                    if(need>=1){
                        R8(end,v);
                        if(!v){
                            for(;;){
                                ++scan;++end;
                                if(s16((uint32_t)need)<s16(scan))break;
                                R8(end,v);if(v)break;
                            }
                        }
                    }
                }
                if(s16(scan)<=need){p=end;continue;}
            }
            R32(style+0x1cu,map);R16(style+0x20u,limit);
            if(s16(limit)<s32(end-map)){*out=0;return 1;}
            /*Zero glyphs give need=-1: no bitmapbyteswritten, but a real pool
             * pointer is returned. Do not turn this into inventedNULLfailure. */
            for(scan=p;scan<end;++scan)W8(scan,1);
            break;
        }
    }
    R32(style+0x1cu,map);R32(style+0x18u,base);*out=base+(p-map)*160u;return 1;
}

/* Metrics2EB50 and prefixwidth2ECD4 share source glyph fallback rules. */
static int glyph_index(Run* r,uint32_t map,uint32_t ch,uint32_t* glyph){
    uint32_t g;R16(map+ch*2u,g);
    if(s16(g)<0){
        if(ch-0x61u<26u){R16(map+ch*2u-0x40u,g);if(s16(g)>=0){*glyph=g;return 1;}}
        R16(map+0x7eu,g);
    }
    *glyph=g;return 1;
}
static int metrics(Run* r,uint32_t text,uint32_t count_address,uint32_t* local_count,uint16_t* length,uint16_t* width){
    uint32_t style,font,map,space,kern,ch,g,fonts,total=0,p=text,first=65535u,c;
    R32(0x800b2048u,style);R16(style+0x26u,font);R32(style+0xcu,map);
    R8(style+asr((uint32_t)s16(font),8)+0x42u,space);R8(style+asr((uint32_t)s16(font),8)+0x4au,kern);
    if(local_count)*local_count=0;else W16(count_address,0);map+=(uint32_t)(s16(font)*2);
    T8(p,ch);
    while(ch){
        T8(p,ch);
        if(ch==0x20u)total+=space;
        else if(ch==10u){if(s16(first)<0)first=total&65535u;}
        else if(ch==31u){++p;T8(p,ch);total+=ch;}
        else if(ch==29u){++p;T8(p,ch);R32(style+0xcu,map);map+=(ch-1u)<<9;}
        else if(ch==30u||ch==28u)++p;
        else{
            TRY(glyph_index(r,map,ch,&g));
            if(s16(g)<0)total+=space;
            else{R32(style+8u,fonts);if(local_count)c=*local_count;else R16(count_address,c);
                R8(fonts+g*20u+9u,ch);total+=ch-kern;
                if(local_count)*local_count=(c+1u)&65535u;else W16(count_address,c+1u);}
        }
        ++p;T8(p,ch);
    }
    *length=(uint16_t)(p-text);if(local_count)*local_count=*length;
    if(s16(first)<0)first=total;*width=(uint16_t)first;return 1;
}
static int prefix_width(Run* r,uint32_t text,uint32_t stop,uint32_t* width){
    uint32_t style,font,map,space,kern,ch,g,fonts,total=0,p=text;
    R32(0x800b2048u,style);R16(style+0x26u,font);R32(style+0xcu,map);T8(p,ch);
    R8(style+asr((uint32_t)s16(font),8)+0x42u,space);R8(style+asr((uint32_t)s16(font),8)+0x4au,kern);
    map+=(uint32_t)(s16(font)*2);
    while(ch&&ch!=(stop&255u)&&ch!=10u){
        if(ch==32u)total+=space;
        else if(ch==31u){++p;T8(p,ch);total+=ch;}
        else if(ch==29u){++p;T8(p,ch);R32(style+0xcu,map);map+=(ch-1u)<<9;}
        else if(ch==30u||ch==28u)++p;
        else{TRY(glyph_index(r,map,ch,&g));if(s16(g)<0)total+=space;
            else{R32(style+8u,fonts);R8(fonts+g*20u+9u,ch);total+=ch-kern;}}
        ++p;T8(p,ch);
    }
    *width=(uint32_t)s16(total);return 1;
}
/* AA468's reached aligned40-byte copy: two groups of fourLW before fourSW,
 * then two independentwords. Copy opaque unknownbytes withtheirknownness;
 * never establish knownness for stale untouched packet padding. */
static int copy_tag(Run* r,uint32_t source,uint32_t destination){
    uint8_t data[4],known[4],*p,*q;unsigned i;
    /*31344 LW ->3134C SW moves opaque font tag bits.2EA80 can leave its
     * low24 unknown;56914 replaces those bits later. Preserve their knowledge
     * through this copy, including aliases, rather than inventing FFFFFF. */
    if(source&3u){r->p->stopped_address=source;r->p->stopped_in_text=0;return NBA97_TEXT_ALIGNMENT_TRAP;}
    TRY(bytes(r,source,4,&p,&q));for(i=0;i<4;++i){data[i]=p[i];known[i]=q?q[i]:1;}
    if(destination&3u){r->p->stopped_address=destination;r->p->stopped_in_text=0;return NBA97_TEXT_ALIGNMENT_TRAP;}
    TRY(bytes(r,destination,4,&p,&q));
    if(!q)for(i=0;i<4;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<4;++i){p[i]=data[i];if(q)q[i]=known[i];}return 1;
}
static int copy_packet(Run* r,uint32_t source){
    unsigned block,j,k,count;uint8_t data[16],known[16],*p,*q;
    for(block=0;block<4;++block){
        unsigned offset=block<2?block*16u:32u+(block-2u)*4u;count=block<2?4u:1u;
        for(j=0;j<count;++j){TRY(bytes(r,source+offset+j*4u,4,&p,&q));
            for(k=0;k<4;++k){data[j*4u+k]=p[k];known[j*4u+k]=q?q[k]:1;}}
        for(j=0;j<count;++j){TRY(bytes(r,source+40u+offset+j*4u,4,&p,&q));
            if(!q)for(k=0;k<4;++k)if(!known[j*4u+k])return NBA97_TEXT_UNKNOWN;
            for(k=0;k<4;++k){p[k]=data[j*4u+k];if(q)q[k]=known[j*4u+k];}}
    }
    return 1;
}
static int splice(Run* r,uint32_t head,uint32_t packet){
    uint8_t *p,*q,link[3],known[3];unsigned i;
    /*56914 LWL/SWL transfers exactly the low24address bytes; byte3 packet
     * length survives. Both call-site pointers are provenaligned by earlierSW. */
    TRY(bytes(r,head,3,&p,&q));for(i=0;i<3;++i){link[i]=p[i];known[i]=q?q[i]:1;}
    TRY(bytes(r,packet,3,&p,&q));if(!q)for(i=0;i<3;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<3;++i){p[i]=link[i];if(q)q[i]=known[i];}
    TRY(bytes(r,head,3,&p,&q));for(i=0;i<3;++i){p[i]=(uint8_t)(packet>>(i*8));if(q)q[i]=1;}
    return 1;
}
static int create(Run* r,int32_t id,uint32_t text,int32_t x,int32_t y,uint32_t mode,uint32_t* out){
    uint32_t style,cursor,capacity,pool,obj,index,v,w,fresh,old,group,field,tail;
    uint32_t packet,font,map,space,kern,color=0x80808000u,ch,g,glyph,p=text,px=(uint32_t)x,origin=(uint32_t)x;
    uint32_t py=(uint32_t)y&65535u,count,tmp;uint16_t length,width;
    mode&=255u;
    R32(0x800b2048u,style);R16(style+0x40u,cursor);R32(style+0x10u,pool);R16(style+0x22u,capacity);
    index=(uint32_t)s16(cursor);obj=pool+(index<<6);
    if(s16(cursor)<s16(capacity)){
        for(;;){R16(obj+0x12u,v);if(s16(v)<0)break;
            ++index;obj+=64u;if(s16(index)>=s16(capacity))break;}
        R16(style+0x22u,capacity);
        if(s16(index)<s16(capacity))goto found;
    }
    R16(style+0x40u,cursor);R32(style+0x10u,obj);index=0;
    if(s16(cursor)>0){
        for(;;){R16(obj+0x12u,v);if(s16(v)<0)break;
            ++index;obj+=64u;if(s16(index)>=s16(cursor))break;}
    }
found:
    /*30E04 has no final exhausted-object failure check. If both scans found
     * no negative status, the reached slot is reused anyway. Keep this bug. */
    W16(style+0x40u,index);TRY(metrics(r,text,obj+0xcu,NULL,&length,&width));
    R16(obj+0xcu,count);TRY(allocate(r,s16(count),&packet));W32(obj+8u,packet);
    if(!packet){*out=0;return 1;}
    W16(obj+0xeu,px);W16(obj+0x10u,py);R16(style+0x28u,v);
    W8(obj+0x3bu,0);W8(obj+0x2bu,0);W16(obj+0x20u,0);W16(obj+0x1eu,0);W16(obj+0x12u,v);
    if(s16((uint32_t)id)>=0&&s16((uint32_t)id)<200){
        R32(0x800b2048u,fresh);W16(fresh+(s16((uint32_t)id)<100?0x2cu:0x2eu),(uint32_t)id);
    }
    W16(obj+0x14u,(uint32_t)id);
    if(s16((uint32_t)id)>=0){
        W16(obj+0x16u,65535);R32(style+0x14u,pool);tmp=(uint32_t)s16((uint32_t)id)*2u;
        R16(pool+tmp,old);W16(obj+0x18u,old);
        if(s16(old)>=0){R32(style+0x10u,pool);W16(pool+(old<<6)+0x16u,index);}
        R32(style+0x14u,pool);W16(pool+tmp,index);
        if(s16((uint32_t)id)<100)W16(style+0x2cu,(uint32_t)id);
        else if(s16((uint32_t)id)<200)W16(style+0x2eu,(uint32_t)id);
    }
    R16(style+0x2au,group);field=group==1?0x34u:group==2?0x38u:group==3?0x3cu:0x30u;
    R16(style+field,old);
    if(s16(old)<0){W16(style+field+2u,index);W16(style+field,index);W16(obj+0x1au,65535);W16(obj+0x1cu,65535);}
    else{
        R16(style+field+2u,tail);W16(obj+0x1cu,65535);W16(obj+0x1au,tail);R32(style+0x10u,pool);
        W16(style+field+2u,index);W16(pool+((uint32_t)s16(tail)<<6)+0x1cu,index);
    }
    R16(style+0x26u,font);R32(style+0xcu,map);R8(style+asr((uint32_t)s16(font),8)+0x42u,space);
    R8(style+asr((uint32_t)s16(font),8)+0x4au,kern);map+=(uint32_t)(s16(font)*2);
    TRY(reset_packet(r,obj,1));TRY(reset_packet(r,obj+4u,1));
    if(mode==1)px-=asr((uint32_t)s16(width),1);
    else if(mode==2)px-=width;
    else if(mode==3||mode==4){TRY(prefix_width(r,text,mode==3?'.':'/',&v));px-=v;}
    T8(p,ch);
    while(ch){
        R16(map+ch*2u,g);
        if(s16(g)<0){
            T8(p,ch);
            if(ch==32u){px+=space;goto next_character;}
            if(ch==10u){
                TRY(metrics(r,p+1u,0,&tmp,&length,&width));
                if(mode==0)px=origin;
                else if(mode==1)px=origin-asr((uint32_t)s16(width),1);
                else if(mode==2)px=origin-width;
                else if(mode==3||mode==4){TRY(prefix_width(r,p+1u,mode==3?'.':'/',&v));px=origin-v;}
                R8(style+0x52u,v);py=(py+v)&65535u;goto next_character;
            }
            if(ch==31u){++p;T8(p,v);px+=v;goto next_character;}
            if(ch==30u){++p;T8(p,v);R32(0x800b204cu+(v&3u)*4u,color);goto next_character;}
            if(ch==29u){++p;T8(p,v);R32(style+0xcu,map);map+=(v-1u)<<9;goto next_character;}
            if(ch==28u){++p;T8(p,v);py=(py+(uint32_t)s8(v))&65535u;goto next_character;}
            if(ch-0x61u<26u){R16(map+ch*2u-0x40u,g);if(s16(g)>=0)goto have_glyph;}
            R16(map+0x7eu,g);if(s16(g)<0){px+=space;goto next_character;}
        }
have_glyph:
        if(s16(g)<0)goto next_character;
        R32(style+8u,pool);glyph=pool+g*20u;
        TRY(copy_tag(r,glyph,packet));R16(glyph+4u,v);W16(packet+0xeu,v);R16(glyph+6u,v);W16(packet+0x16u,v);
        R8(glyph+0xbu,v);W8(packet+7u,v);
        R8(glyph+0xcu,v);W8(packet+0xcu,v);R8(glyph+0xdu,v);W8(packet+0x14u,v);
        R8(glyph+0xeu,v);W8(packet+0x1cu,v);R8(glyph+0xfu,v);W8(packet+0x24u,v);
        R8(glyph+0x10u,v);W8(packet+0xdu,v);R8(glyph+0x11u,v);W8(packet+0x15u,v);
        R8(glyph+0x12u,v);W8(packet+0x1du,v);R8(glyph+0x13u,v);W8(packet+0x25u,v);
        if(color==0x80808000u){W8(packet+6u,128);W8(packet+5u,128);W8(packet+4u,128);}
        else{W8(packet+4u,color>>24);W8(packet+5u,color>>16);W8(packet+6u,color>>8);}
        W16(packet+0x1eu,0);W16(packet+0x18u,px);W16(packet+8u,px);R8(glyph+9u,v);
        W16(packet+0x20u,px+v);W16(packet+0x10u,px+v);R8(glyph+8u,v);
        W16(packet+0x12u,py+(uint32_t)s8(v));W16(packet+0xau,py+(uint32_t)s8(v));
        R8(glyph+8u,v);R8(glyph+0xau,w);v=py+(uint32_t)s8(v)+w;
        W16(packet+0x22u,v);W16(packet+0x1au,v);TRY(copy_packet(r,packet));
        R8(glyph+9u,v);packet+=80u;px+=v-kern;++r->p->glyphs_written;
next_character:
        ++p;T8(p,ch);
    }
    R16(obj+0xcu,count);
    if(count){packet-=80u;do{TRY(splice(r,obj,packet));TRY(splice(r,obj+4u,packet+40u));--count;packet-=80u;}while(count&65535u);}
    *out=obj;return 1;
}
int nba97_game_text_reset_group(Nba97GameTextContext* c,int32_t group,Nba97GameTextProgress* p){Run r;if(!start(c,p,&r))return 0;return reset_group(&r,group);}
int nba97_game_text_reset_packet(Nba97GameTextContext* c,uint32_t object,uint32_t count,Nba97GameTextProgress* p){Run r;if(!start(c,p,&r))return 0;return reset_packet(&r,object,count);}
int nba97_game_text_allocate_packets(Nba97GameTextContext* c,int32_t count,uint32_t* out,Nba97GameTextProgress* p){Run r;if(!out||!start(c,p,&r))return 0;return allocate(&r,count,out);}
int nba97_game_text_create(Nba97GameTextContext* c,int32_t id,uint32_t text,int32_t x,int32_t y,uint32_t mode,uint32_t* out,Nba97GameTextProgress* p){
    Run r;if(!out||!start(c,p,&r))return 0;return create(&r,id,text,x,y,mode,out);
}
int nba97_game_text_create_span(Nba97GameTextContext* c,int32_t id,Nba97GameTextSpan text,int32_t x,int32_t y,uint32_t mode,uint32_t* out,Nba97GameTextProgress* p){
    Run r;if(!out||text.size>UINT32_MAX||(!text.data&&text.size)||!start(c,p,&r))return 0;
    r.text=&text;return create(&r,id,0,x,y,mode,out);
}
