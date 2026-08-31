#include "game_font_loader.h"
#include <string.h>

typedef struct Run {Nba97GameFontContext* c;Nba97GameFontProgress* p;Nba97GameFontScratch* s;} Run;
static int32_t s32(uint32_t v){return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)~v;}
static int32_t s16(uint32_t v){v&=65535u;return v<32768u?(int32_t)v:(int32_t)v-65536;}
static uint32_t asr(uint32_t v,unsigned n){return(v>>n)|((v&0x80000000u)?(~0u<<(32-n)):0);}
static int step(Run* r,uint32_t at){r->p->stopped_address=at;r->p->stopped_in_scratch=0;if(r->p->steps>=r->c->step_budget)return NBA97_FONT_LIMIT;++r->p->steps;return 1;}
static int bytes(Run* r,uint32_t at,size_t n,uint8_t** p,uint8_t** k){
    size_t i,j;int result=step(r,at);if(result!=1)return result;
    for(i=0;i<r->c->memory.count;++i){Nba97GameTextRegion* m=&r->c->memory.region[i];uint64_t off=(uint64_t)at-m->base;
        if(at<m->base||off>m->size||n>m->size-(size_t)off)continue;
        *p=m->data+(size_t)off;*k=m->known?m->known+(size_t)off:NULL;
        if(*k)for(j=0;j<n;++j)if((*k)[j]>1)return NBA97_FONT_ARGUMENT;
        return 1;
    }return NBA97_FONT_RESOURCE;
}
static int read_value(Run* r,uint32_t at,unsigned n,uint32_t* out){
    uint8_t *p,*k;uint32_t v=0;unsigned i;int result;
    if(at&(n-1u)){r->p->stopped_address=at;r->p->stopped_in_scratch=0;return NBA97_FONT_ALIGNMENT_TRAP;}
    result=bytes(r,at,n,&p,&k);if(result!=1)return result;
    if(k)for(i=0;i<n;++i)if(!k[i])return NBA97_FONT_UNKNOWN;
    for(i=0;i<n;++i)v|=(uint32_t)p[i]<<(i*8);*out=v;return 1;
}
static int write_value(Run* r,uint32_t at,unsigned n,uint32_t v){
    uint8_t *p,*k;unsigned i;int result;
    if(at&(n-1u)){r->p->stopped_address=at;r->p->stopped_in_scratch=0;return NBA97_FONT_ALIGNMENT_TRAP;}
    result=bytes(r,at,n,&p,&k);if(result!=1)return result;
    for(i=0;i<n;++i){p[i]=(uint8_t)(v>>(i*8));if(k)k[i]=1;}return 1;
}
#define TRY(x) do{int fr=(x);if(fr!=1)return fr;}while(0)
#define R8(a,v) TRY(read_value(r,(a),1,&(v)))
#define R16(a,v) TRY(read_value(r,(a),2,&(v)))
#define R32(a,v) TRY(read_value(r,(a),4,&(v)))
#define W8(a,v) TRY(write_value(r,(a),1,(uint32_t)(v)))
#define W16(a,v) TRY(write_value(r,(a),2,(uint32_t)(v)))
#define W32(a,v) TRY(write_value(r,(a),4,(uint32_t)(v)))
static int scratch(Run* r,int which,unsigned at,unsigned n,int write,uint32_t* value){
    uint8_t *p=which==1?r->s->packet:r->s->name,*k=which==1?r->s->packet_known:r->s->name_known;unsigned i;
    TRY(step(r,at));r->p->stopped_in_scratch=(uint8_t)which;
    for(i=0;i<n;++i)if(k[at+i]>1)return NBA97_FONT_ARGUMENT;
    if(write){for(i=0;i<n;++i){p[at+i]=(uint8_t)(*value>>(i*8));k[at+i]=1;}}
    else{for(i=0;i<n;++i)if(!k[at+i])return NBA97_FONT_UNKNOWN;
        *value=0;for(i=0;i<n;++i)*value|=(uint32_t)p[at+i]<<(i*8);}
    return 1;
}
static int sw(Run* r,unsigned at,unsigned n,uint32_t v){return scratch(r,1,at,n,1,&v);}
static int name_byte(Run* r,unsigned at,uint32_t* v){return scratch(r,2,at,1,0,v);}
static int tag_copy(Run* r,uint32_t dest){
    uint8_t value[4],known[4],*p,*k;unsigned i;
    TRY(step(r,0));r->p->stopped_in_scratch=1;
    for(i=0;i<4;++i){if(r->s->packet_known[i]>1)return NBA97_FONT_ARGUMENT;
        value[i]=r->s->packet[i];known[i]=r->s->packet_known[i];}
    if(dest&3u){r->p->stopped_address=dest;r->p->stopped_in_scratch=0;return NBA97_FONT_ALIGNMENT_TRAP;}
    TRY(bytes(r,dest,4,&p,&k));if(!k)for(i=0;i<4;++i)if(!known[i])return NBA97_FONT_UNKNOWN;
    for(i=0;i<4;++i){p[i]=value[i];if(k)k[i]=known[i];}return 1;
}
static int start(Nba97GameFontContext* c,Nba97GameFontProgress* p,Nba97GameFontScratch* s,Run* r){
    size_t i,j;if(!c||!p||(!c->memory.region&&c->memory.count))return 0;
    for(i=0;i<c->memory.count;++i){const Nba97GameTextRegion* a=&c->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return 0;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&c->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return 0;}
    }
    memset(p,0,sizeof *p);r->c=c;r->p=p;r->s=s;return 1;
}
static uint32_t nibble(uint32_t c){c&=255u;if(c-0x31u<9u)return c-0x30u;if(c-0x61u<6u)return c-0x57u;if(c-0x41u<6u)return c-0x37u;return 0;}
int32_t nba97_game_font_decode(uint32_t high,uint32_t low){return s16((nibble(high)<<4)+nibble(low));}
static int count(Run* r,uint32_t resource,uint32_t* n){R32(resource+8u,*n);return 1;}
static int entry(Run* r,uint32_t resource,uint32_t index,uint32_t* out){
    uint32_t n,offset;TRY(count(r,resource,&n));if(index>=n){*out=0;return 1;}
    R32(resource+(index<<3)+0x14u,offset);*out=resource+offset;return 1;
}
static int name(Run* r,uint32_t resource,uint32_t index){
    uint32_t n,v=0;TRY(count(r,resource,&n));if(index<n)R32(resource+(index<<3)+0x10u,v);
    /*A4020's OOBbranch targetsA4038, then falls throughA403C's SWzero.
     * The function-table extent omits that final three-instruction tail.*/
    return scratch(r,2,0,4,1,&v);
}
static int event(Run* r,int kind,uint32_t resource,uint32_t flags,int32_t x,int32_t y,int32_t cx,int32_t cy,uint32_t* loaded){
    Nba97GameFontEvent e;e.kind=kind;e.resource=resource;e.flags=flags;e.x=x;e.y=y;e.clut_x=cx;e.clut_y=cy;
    TRY(step(r,resource));if(!r->c->io||r->c->io(r->c->user,&e,loaded)!=1)return NBA97_FONT_IO_REFUSED;
    ++r->p->callbacks_completed;return 1;
}
static int load(Run* r,uint32_t filename,uint32_t* out){
    uint32_t resource;do{resource=0;TRY(event(r,NBA97_FONT_LOAD_ATTEMPT_941C8,filename,0x20,0,0,0,0,&resource));}while(!resource);
    *out=resource;return 1; /*29C20 retries forever onNULL; nativebudget is not repair.*/
}
static int release(Run* r,uint32_t resource){uint32_t unused=0;if(!resource)return 1;return event(r,NBA97_FONT_RELEASE_90698,resource,0,0,0,0,0,&unused);}
static int upload(Run* r,int chain,uint32_t resource,int32_t x,int32_t y,int32_t cx,int32_t cy){uint32_t unused=0;return event(r,chain?NBA97_FONT_UPLOAD_CHAIN_94540:NBA97_FONT_UPLOAD_946B8,resource,0,x,y,cx,cy,&unused);}
static int is_special(Run* r,uint32_t filename,uint32_t* out){
    uint32_t a,b,other=0x80024920u;/*9CB5C BIOSstrcmp semantic boundary.*/
    for(;;){R8(filename,a);R8(other,b);if(a!=b){*out=0;return 1;}if(!a){*out=1;return 1;}++filename;++other;}
}
static int logo_color(Run* r,uint32_t logos,uint32_t team,uint32_t field,uint32_t* last){
    uint32_t index,image,word,palette,color,style,v;R32(team,index);TRY(entry(r,logos,index+31u,&image));
    R32(image,word);palette=image+asr(word,8);*last=palette;R16(palette+0x20eu,color);R32(0x800b2048u,style);
    v=((color&31u)<<26)+(((color>>5)&31u)<<18)+(color&0x7c00u);W32(style+field,v);return 1;
}
static int tpage(Run* r,uint32_t format,uint32_t x,uint32_t y,uint32_t* out){
    uint32_t mode;R8(0x800c55c0u,mode);if(mode!=1)R8(0x800c55c0u,mode);
    if(mode==1||mode==2)*out=((format&3u)<<9)|((y&0x300u)>>3)|((x&0x3ffu)>>6);
    else *out=((format&3u)<<7)|((y&0x100u)>>4)|((x&0x3ffu)>>6)|((y&0x200u)<<2);
    return 1;
}
static int run_load(Run* r,uint32_t filename,uint32_t spacing,uint32_t kerning,uint32_t cx,uint32_t cy,uint32_t recolor){
    uint8_t palette_used[256];uint32_t special,logos=0,last=0,style,font,n,descriptor,map,glyphs,v,a,b,i;
    TRY(is_special(r,filename,&special));
    if(special){TRY(load(r,0x80024930u,&logos));TRY(logo_color(r,logos,0x80021d74u,0,&last));TRY(logo_color(r,logos,0x80021d78u,4,&last));}
    memset(palette_used,0,sizeof palette_used);R32(0x800b2048u,style);TRY(load(r,filename,&font));TRY(count(r,font,&n));
    R16(style+0x26u,v);W8(style+asr((uint32_t)s16(v),8)+0x42u,spacing);
    R16(style+0x26u,v);W8(style+asr((uint32_t)s16(v),8)+0x4au,kerning);
    R16(style+0x24u,glyphs);R16(style+0x26u,v);R32(style+0xcu,map);R32(style+8u,descriptor);
    descriptor+=(uint32_t)(s16(glyphs)*20);map+=(uint32_t)(s16(v)*2);
    cx=(uint32_t)s16(cx);cy=(uint32_t)s16(cy);recolor&=65535u;
    for(i=0;s32(i)<s32(n);++i){
        uint32_t image,replaced=0,hi,lo,grid,width,height,u,texture_x,uv_y,flipped,format,tp,clut;
        TRY(name(r,font,i));TRY(entry(r,font,i,&image));
        if(special){TRY(name_byte(r,2,&a));if(a==0x35u){TRY(name_byte(r,3,&b));
            if(b==0x34u||b==0x35u){R32(b==0x34u?0x80021d74u:0x80021d78u,v);TRY(entry(r,logos,v,&last));
                R16(image+0xcu,v);W16(last+0xcu,v);R16(image+0xeu,v);image=last;replaced=1;W16(last+0xeu,v);}
        }}
        TRY(name_byte(r,1,&a));hi=(uint32_t)nba97_game_font_decode(0x30u,a);
        TRY(name_byte(r,0,&a));lo=(uint32_t)nba97_game_font_decode(0x30u,a);grid=(hi<<4)+lo;
        if(!palette_used[grid]){
            palette_used[grid]=1;R32(image,v);last=image+asr(v,8);
            if(recolor&&s32(lo)>=6)W16(last+0x20eu,0x6af7u);
            R16(last+0xcu,a);R16(last+0xeu,b);
            TRY(upload(r,1,last,s16(a),s16(b),s32(cx+(hi<<4)),s32(cy+lo)));
        }
        TRY(name_byte(r,2,&a));TRY(name_byte(r,3,&b));v=(uint32_t)nba97_game_font_decode(a,b);
        R16(style+0x24u,glyphs);W16(map+v*2u,glyphs+i);
        R8(image,format);R16(image+4u,width);R16(image+6u,height);
        R16(image+0xcu,v);u=(v&63u)<<((format&3u)==0?2:(format&3u)==1?1:0);
        R16(image+0xcu,flipped);flipped>>=15;R8(image+0xeu,uv_y);
        R16(image+0xcu,a);R16(image+0xeu,b);R8(image,v);
        /*2E964 clears the entire signedheaderlink before946B8. It is only
         * rebuilt for replacedlogos when image<last (unsigned comparison).*/
        W32(image,v);TRY(upload(r,0,image,(int32_t)(a&0x3fffu),(int32_t)(b&0x3fffu),0,0));
        if(replaced&&image<last){R8(image,v);W32(image,v|((last-image)<<8));}
        TRY(sw(r,3,1,9));TRY(sw(r,7,1,0x2c));
        R8(image,format);R16(image+0xcu,texture_x);R16(image+0xeu,v);
        TRY(tpage(r,format&3u,texture_x&0x3fc0u,v&0x3f00u,&tp));TRY(sw(r,0x16,2,tp));
        clut=(((cy+lo)<<6)|((asr(cx+(hi<<4),4))&63u))&65535u;TRY(sw(r,0xe,2,clut));
        TRY(scratch(r,1,7,1,0,&v));TRY(sw(r,7,1,v&0xfdu));
        a=u+(uint32_t)s16(width);b=uv_y+(uint32_t)s16(height);
        if(flipped){W8(descriptor+13u,u);W8(descriptor+12u,u);W8(descriptor+15u,a);W8(descriptor+14u,a);
            W8(descriptor+19u,b);W8(descriptor+17u,b);W8(descriptor+18u,uv_y);W8(descriptor+16u,uv_y);
            W8(descriptor+9u,height);W8(descriptor+10u,width);
        }else{W8(descriptor+14u,u);W8(descriptor+12u,u);W8(descriptor+15u,a);W8(descriptor+13u,a);
            W8(descriptor+17u,uv_y);W8(descriptor+16u,uv_y);W8(descriptor+19u,b);W8(descriptor+18u,b);
            W8(descriptor+9u,width);W8(descriptor+10u,height);}
        R8(image+0xau,v);W8(descriptor+8u,0u-v);
        /*Confirmedsourcequirk2EA80: incomingstack low24 copied unchanged,
         * not a manufactured FFFFFF primitive-list terminator.*/
        TRY(tag_copy(r,descriptor));TRY(scratch(r,1,0xe,2,0,&v));W16(descriptor+4u,v);
        TRY(scratch(r,1,0x16,2,0,&v));W16(descriptor+6u,v);TRY(scratch(r,1,7,1,0,&v));
        descriptor+=20u;W8(descriptor-9u,v);++r->p->glyphs_written;
    }
    TRY(release(r,font));if(special)TRY(release(r,logos));
    R16(style+0x26u,a);R16(style+0x24u,b);W16(style+0x26u,a+0x100u);W16(style+0x24u,b+n);return 1;
}
int nba97_game_font_shpp_count(Nba97GameFontContext* c,uint32_t resource,uint32_t* out,Nba97GameFontProgress* p){
    Run run;if(!out)return 0;TRY(start(c,p,NULL,&run));return count(&run,resource,out);
}
int nba97_game_font_shpp_entry(Nba97GameFontContext* c,uint32_t resource,uint32_t index,uint32_t* out,Nba97GameFontProgress* p){
    Run run;if(!out)return 0;TRY(start(c,p,NULL,&run));return entry(&run,resource,index,out);
}
int nba97_game_font_shpp_name(Nba97GameFontContext* c,uint32_t resource,uint32_t index,uint32_t destination,Nba97GameFontProgress* p){
    Run run;Run* r=&run;uint32_t n,v=0;TRY(start(c,p,NULL,r));TRY(count(r,resource,&n));
    if(index<n)R32(resource+(index<<3)+0x10u,v);W32(destination,v);return 1;
}
int nba97_game_font_load(Nba97GameFontContext* c,uint32_t filename,uint32_t spacing,uint32_t kerning,uint32_t x,uint32_t y,uint32_t recolor,Nba97GameFontScratch* s,Nba97GameFontProgress* p){
    Run run;if(!s)return 0;TRY(start(c,p,s,&run));return run_load(&run,filename,spacing,kerning,x,y,recolor);
}
