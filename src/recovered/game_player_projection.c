#include "game_player_projection.h"
#include <string.h>
typedef Nba97GameBodyReference Ref;
typedef struct Pointer {Ref ref;int unresolved;} Pointer;
typedef struct Run {const Nba97GamePlayerProjectionInput* in;Nba97GamePlayerGeometryWrite* journal;size_t capacity;Nba97GamePlayerProjectionProgress* out;} Run;
#define TRY(x) do{int r_=(x);if(r_!=NBA97_BODY_OK)return r_;}while(0)
static int32_t signed32(uint32_t v){return v<0x80000000u?(int32_t)v:-1-(int32_t)(~v);}
static int valid_ref(Ref r){return r.known<=1&&(r.known||(!r.allocation&&!r.offset));}
static Pointer pof(Ref r){Pointer p;p.ref=r;p.unresolved=0;return p;}
static Pointer add(Pointer p,uint32_t n){if(p.ref.known)p.ref.offset+=n;return p;}
static int access(Run* r,Pointer p,uint32_t pc,unsigned width,Nba97GameBodyBuffer** out,Nba97GameBodyCell** cell,int* unknown){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;uint64_t ci;unsigned i;
    r->out->stopped_pc=pc;r->out->stopped_reference=p.ref;
    if(p.unresolved)return p.unresolved;
    if(!valid_ref(p.ref))return NBA97_BODY_ARGUMENT;
    if(!p.ref.known)return NBA97_BODY_UNKNOWN;
    if(p.ref.allocation>=r->in->buffer_count)return NBA97_BODY_BOUNDS;
    b=&r->in->buffers[p.ref.allocation];
    if(!b->bytes||p.ref.offset>b->size||width>b->size-p.ref.offset)return NBA97_BODY_BOUNDS;
    if(b->address_mod4_known>1||b->address_mod4>3)return NBA97_BODY_ARGUMENT;
    if(!b->address_mod4_known)return NBA97_BODY_ALIGNMENT_UNKNOWN;
    if(((uint64_t)p.ref.offset+b->address_mod4)&3u)return width==3?NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT:NBA97_BODY_ALIGNMENT_TRAP;
    ci=((uint64_t)p.ref.offset+b->address_mod4)/4;
    if(!b->cells||ci>=b->cell_count)return NBA97_BODY_BOUNDS;
    c=&b->cells[(size_t)ci];
    if(c->is_reference>1||!valid_ref(c->reference)||(!c->is_reference&&c->reference.known))return NBA97_BODY_ARGUMENT;
    *unknown=0;
    if(b->known)for(i=0;i<width;++i){if(b->known[p.ref.offset+i]>1)return NBA97_BODY_ARGUMENT;if(!b->known[p.ref.offset+i])*unknown=1;}
    *out=b;*cell=c;return NBA97_BODY_OK;
}
static int raw(Run* r,Pointer p,uint32_t pc,unsigned width,uint32_t* value){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;int unknown;unsigned i;uint32_t v=0;
    TRY(access(r,p,pc,width,&b,&c,&unknown));
    if(c->is_reference)return c->reference.known?NBA97_BODY_ADDRESS_REQUIRED:NBA97_BODY_UNKNOWN;
    if(unknown)return NBA97_BODY_UNKNOWN;
    for(i=0;i<width;++i)v|=(uint32_t)b->bytes[p.ref.offset+i]<<(i*8);
    *value=v;return NBA97_BODY_OK;
}
static int low_word(Run* r,Pointer p,uint32_t pc,uint32_t* value){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;int unknown;
    TRY(access(r,p,pc,4,&b,&c,&unknown));
    if(c->is_reference)return c->reference.known?NBA97_BODY_ADDRESS_REQUIRED:NBA97_BODY_UNKNOWN;
    /* The original LW/LWC2 still reaches all4bytes, but CTC2 rotationword4
     * and vertexZ discard the upperhalf. Unknown padding need not become invented
     * known data. Bounds/alignment/canonical checks above cover the fullspan. */
    if(b->known&&(!b->known[p.ref.offset]||!b->known[p.ref.offset+1]))return NBA97_BODY_UNKNOWN;
    *value=(uint32_t)b->bytes[p.ref.offset]|((uint32_t)b->bytes[p.ref.offset+1]<<8);return NBA97_BODY_OK;
}
static int low24_word(Run* r,Pointer p,uint32_t pc,uint32_t* value){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;int unknown;unsigned i;uint32_t v=0;
    TRY(access(r,p,pc,4,&b,&c,&unknown));
    if(c->is_reference)return c->reference.known?NBA97_BODY_ADDRESS_REQUIRED:NBA97_BODY_UNKNOWN;
    /*548F0/54B7C LW reaches all4bytes, but the following AND discards the
     * highbyte. Validate its metadata/range without requiring a known value. */
    for(i=0;i<3;++i){if(b->known&&!b->known[p.ref.offset+i])return NBA97_BODY_UNKNOWN;v|=(uint32_t)b->bytes[p.ref.offset+i]<<(8*i);}
    *value=v;return NBA97_BODY_OK;
}
static int pointer(Run* r,Pointer p,uint32_t pc,Pointer* out){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;int unknown;Ref zero={0,0,0};
    TRY(access(r,p,pc,4,&b,&c,&unknown));
    /* A loaded pointer may never be used (54A64..6C final prefetch). Preserve
     * that dead/opaque read without inventing a native allocation identity. */
    if(c->is_reference){*out=pof(c->reference);return NBA97_BODY_OK;}
    *out=pof(zero);out->unresolved=unknown?NBA97_BODY_UNKNOWN:NBA97_BODY_REFERENCE_REQUIRED;return NBA97_BODY_OK;
}
static int write(Run* r,Pointer p,uint32_t pc,unsigned width,uint32_t value){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;Nba97GamePlayerGeometryWrite* e;int unknown;unsigned i;
    r->out->stopped_pc=pc;r->out->stopped_reference=p.ref;
    if(r->out->writes>=r->capacity)return NBA97_BODY_JOURNAL_LIMIT;
    TRY(access(r,p,pc,width,&b,&c,&unknown));
    if(width!=4&&c->is_reference)return NBA97_BODY_ADDRESS_REQUIRED;
    memset(c,0,sizeof *c);
    for(i=0;i<width;++i){b->bytes[p.ref.offset+i]=(uint8_t)(value>>(i*8));if(b->known)b->known[p.ref.offset+i]=1;}
    e=&r->journal[r->out->writes++];memset(e,0,sizeof *e);e->destination=p.ref;e->pc=pc;e->width=(uint8_t)width;e->word=width==3?value&0xffffffu:value;
    return NBA97_BODY_OK;
}
static int math(Run* r,uint32_t pc,unsigned kind,unsigned index,uint32_t word,uint32_t* out){
    Nba97PlayerMathRequest q;Nba97GamePeriodValue value={0,0};int status;
    r->out->stopped_pc=pc;memset(&r->out->stopped_reference,0,sizeof r->out->stopped_reference);
    q.pc=pc;q.word=word;q.kind=kind;q.index=index;
    status=r->in->math(r->in->math_user,&q,&value);if(status!=NBA97_BODY_OK)return status;
    ++r->out->math_calls;
    if(out){if(value.known>1||(!value.known&&value.word))return NBA97_BODY_ARGUMENT;if(!value.known)return NBA97_BODY_UNKNOWN;*out=value.word;}
    return NBA97_BODY_OK;
}
static int load_rotation(Run* r,Pointer source,uint32_t read_pc,uint32_t load_pc){
    uint32_t w[5];unsigned i;for(i=0;i<5;++i){if(i==4)TRY(low_word(r,add(source,i*4),read_pc+i*4,&w[i]));else TRY(raw(r,add(source,i*4),read_pc+i*4,4,&w[i]));}
    for(i=0;i<5;++i)TRY(math(r,(i==4?0x80055f40:load_pc+i*4),NBA97_PROJECTION_ROTATION,i,w[i],0));
    return NBA97_BODY_OK;
}
static int address(Run* r,Pointer p,uint32_t pc,uint32_t* out){
    const Nba97PlayerProjectionAddress* a;const Nba97GameBodyBuffer* b;
    r->out->stopped_pc=pc;r->out->stopped_reference=p.ref;
    if(p.unresolved)return p.unresolved;
    if(!valid_ref(p.ref))return NBA97_BODY_ARGUMENT;
    if(!p.ref.known)return NBA97_BODY_UNKNOWN;
    if(p.ref.allocation>=r->in->buffer_count)return NBA97_BODY_BOUNDS;
    if(!r->in->addresses||p.ref.allocation>=r->in->address_count)return NBA97_BODY_ADDRESS_REQUIRED;
    a=&r->in->addresses[p.ref.allocation];
    if(a->known>1||(!a->known&&a->word))return NBA97_BODY_ARGUMENT;
    if(!a->known)return NBA97_BODY_ADDRESS_REQUIRED;
    b=&r->in->buffers[p.ref.allocation];
    if(b->address_mod4_known>1||b->address_mod4>3)return NBA97_BODY_ARGUMENT;
    if(!b->address_mod4_known)return NBA97_BODY_ALIGNMENT_UNKNOWN;
    if((a->word&3u)!=b->address_mod4)return NBA97_BODY_ARGUMENT;
    *out=a->word+p.ref.offset;return NBA97_BODY_OK;
}
static int load_vertex(Run* r,Pointer xyz,uint32_t pc){
    uint32_t v;unsigned i;
    for(i=0;i<6;++i){
        if(i&1)TRY(low_word(r,add(xyz,i*4),pc+i*4,&v));
        else TRY(raw(r,add(xyz,i*4),pc+i*4,4,&v));
        TRY(math(r,pc+i*4,NBA97_PROJECTION_VERTEX,i,v,0));
    }
    return NBA97_BODY_OK;
}
static int cache_vertex(Run* r,Pointer xyz,uint32_t pc,uint32_t v[6]){
    unsigned i;for(i=0;i<6;++i){if(i&1)TRY(low_word(r,add(xyz,i*4),pc+i*4,&v[i]));else TRY(raw(r,add(xyz,i*4),pc+i*4,4,&v[i]));}
    return NBA97_BODY_OK;
}
static int screen_store(Run* r,Pointer packet,unsigned i,uint32_t pc){
    uint32_t v;TRY(math(r,pc,NBA97_PROJECTION_SCREEN,i,0,&v));return write(r,add(packet,8+i*8),pc,4,v);
}
static int depth_store(Run* r,Pointer dst,uint32_t read_pc,uint32_t store_pc,uint32_t mask,uint32_t bias,uint32_t* depth){
    uint32_t v;TRY(math(r,read_pc,NBA97_PROJECTION_DEPTH,0,0,&v));*depth=((v-bias)&mask)<<2;return write(r,dst,store_pc,4,*depth);
}
static int link24(Run* r,Pointer packet,Pointer table,uint32_t depth,uint32_t read_pc,uint32_t table_pc,uint32_t packet_pc){
    uint32_t old,encoded;Pointer entry=add(table,depth);
    TRY(raw(r,entry,read_pc,3,&old));
    /* LWL/SWL preserve both high tag bytes. Resolve numeric packet bits only
     * when the accepted winding actually reaches the source table store. */
    TRY(address(r,packet,table_pc,&encoded));
    TRY(write(r,entry,table_pc,3,encoded));return write(r,packet,packet_pc,3,old);
}
static int link32(Run* r,Pointer packet,Pointer table,uint32_t depth,uint32_t read_pc,uint32_t packet_pc,uint32_t table_pc){
    uint32_t old,encoded;Pointer entry=add(table,depth);
    TRY(low24_word(r,entry,read_pc,&old));
    /*5483C/54ADC set the packet tag before observing its own numeric address;
     * retain that earlier store if address provenance is unavailable. */
    TRY(write(r,packet,packet_pc,4,0x07000000u|(old&0xffffffu)));
    TRY(address(r,packet,table_pc,&encoded));return write(r,entry,table_pc,4,encoded&0xffffffu);
}
static int primary(Run* r,Pointer xyz,Pointer packet,Pointer depths,uint32_t count,Pointer ordering,uint32_t mask){
    uint32_t cached[6],left=count-2,clip,depth;unsigned i;
    TRY(load_vertex(r,xyz,0x80054678));TRY(math(r,0x800546b0,NBA97_PROJECTION_THREE,0,0,0));
    xyz=add(xyz,24);TRY(cache_vertex(r,xyz,0x800546b8,cached));
    TRY(math(r,0x800546d4,NBA97_PROJECTION_CLIP,0,0,0));
    for(i=2;i<6;++i)TRY(math(r,0x800546d8+(i-2)*4,NBA97_PROJECTION_VERTEX,i,cached[i],0));
    TRY(math(r,0x800546ec,NBA97_PROJECTION_MAC0,0,0,&clip));
    for(i=0;i<3;++i)TRY(screen_store(r,packet,i,0x800546f0+i*4));
    TRY(math(r,0x800546fc,NBA97_PROJECTION_AVERAGE_THREE,0,0,0));
    for(i=0;i<2;++i)TRY(math(r,0x80054700+i*4,NBA97_PROJECTION_VERTEX,i,cached[i],0));
    TRY(depth_store(r,depths,0x80054708,0x80054718,mask,0,&depth));
    /* Original54670/546D0 decrement before testing zero. Counts0/1 do not
     * mean empty/single geometry; they execute the same three-triangle route
     * as negative remaining counts. Preserve it, with ordinary bounds guards. */
    if(left!=0)do{
        depths=add(depths,4);TRY(math(r,0x80054728,NBA97_PROJECTION_THREE,0,0,0));--left;
        if(signed32(clip)<=0)TRY(link24(r,packet,ordering,depth,0x80054738,0x8005473c,0x80054740));
        xyz=add(xyz,24);packet=add(packet,32);
        TRY(math(r,0x8005474c,NBA97_PROJECTION_CLIP,0,0,0));
        TRY(cache_vertex(r,xyz,0x80054750,cached));
        TRY(screen_store(r,packet,0,0x8005476c));TRY(math(r,0x80054770,NBA97_PROJECTION_MAC0,0,0,&clip));
        TRY(screen_store(r,packet,1,0x80054774));TRY(screen_store(r,packet,2,0x80054778));
        TRY(math(r,0x8005477c,NBA97_PROJECTION_AVERAGE_THREE,0,0,0));
        for(i=0;i<5;++i)TRY(math(r,0x80054780+i*4,NBA97_PROJECTION_VERTEX,i,cached[i],0));
        TRY(math(r,0x80054794,NBA97_PROJECTION_DEPTH,0,0,&depth));
        TRY(math(r,0x80054798,NBA97_PROJECTION_VERTEX,5,cached[5],0));
        depth=(depth&mask)<<2;TRY(write(r,depths,0x800547a8,4,depth));
    }while(signed32(left)>0);
    depths=add(depths,4);TRY(math(r,0x800547b0,NBA97_PROJECTION_THREE,0,0,0));
    if(signed32(clip)<=0)TRY(link24(r,packet,ordering,depth,0x800547bc,0x800547c0,0x800547c4));
    packet=add(packet,32);TRY(math(r,0x800547cc,NBA97_PROJECTION_CLIP,0,0,0));
    TRY(screen_store(r,packet,0,0x800547d0));TRY(math(r,0x800547d4,NBA97_PROJECTION_MAC0,0,0,&clip));
    TRY(screen_store(r,packet,1,0x800547d8));TRY(screen_store(r,packet,2,0x800547dc));
    TRY(math(r,0x800547e4,NBA97_PROJECTION_AVERAGE_THREE,0,0,0));
    TRY(depth_store(r,depths,0x800547e8,0x800547f8,mask,0,&depth));
    if(signed32(clip)<=0)TRY(link24(r,packet,ordering,depth,0x80054808,0x8005480c,0x80054810));
    return NBA97_BODY_OK;
}
/* These two source leaves differ in winding, depth bias and several load/store
 * positions. Explicit PC tables preserve each access, not merely final pixels. */
typedef struct GroupPC {
    uint32_t load0,project0,clip0,readclip0,screen0[3],average0,readdepth0,storedepth0;
    uint32_t load1,project1,link1[3],clip1,screen1[3],readclip1,average1,readdepth1,storedepth1;
    uint32_t load2,project2,link2[3],clip2,screen2[3],readclip2,average2,readdepth2,storedepth2,link3[3];
} GroupPC;
static const GroupPC biased_pc={
    0x80054854,0x80054870,0x8005487c,0x80054884,{0x8005488c,0x80054890,0x80054894},0x8005489c,0x800548a4,0x800548b8,
    0x800548c8,0x800548e4,{0x800548f0,0x80054900,0x80054908},0x80054914,{0x80054918,0x80054928,0x8005492c},0x80054920,0x80054934,0x80054938,0x8005494c,
    0x8005495c,0x80054978,{0x80054988,0x80054998,0x800549a0},0x800549ac,{0x800549b0,0x800549bc,0x800549c0},0x800549b4,0x800549c8,0x800549cc,0x800549e0,{0x800549f0,0x80054a00,0x80054a08}};
static const GroupPC alternate_pc={
    0x80054af0,0x80054b0c,0x80054b18,0x80054b28,{0x80054b1c,0x80054b20,0x80054b24},0x80054b2c,0x80054b30,0x80054b40,
    0x80054b50,0x80054b6c,{0x80054b7c,0x80054b8c,0x80054b94},0x80054ba0,{0x80054ba4,0x80054ba8,0x80054bac},0x80054bb0,0x80054bb4,0x80054bb8,0x80054bc8,
    0x80054bd8,0x80054bf4,{0x80054c04,0x80054c14,0x80054c1c},0x80054c28,{0x80054c2c,0x80054c38,0x80054c3c},0x80054c30,0x80054c44,0x80054c48,0x80054c58,{0x80054c68,0x80054c78,0x80054c80}};
static int screens_clip(Run* r,Pointer packet,const uint32_t pcs[3],uint32_t read_pc,uint32_t* clip){
    unsigned i;int read=0;
    for(i=0;i<3;++i){if(!read&&read_pc<pcs[i]){TRY(math(r,read_pc,NBA97_PROJECTION_MAC0,0,0,clip));read=1;}TRY(screen_store(r,packet,i,pcs[i]));}
    if(!read)TRY(math(r,read_pc,NBA97_PROJECTION_MAC0,0,0,clip));
    return NBA97_BODY_OK;
}
static int other_group(Run* r,Pointer xyz,Pointer packet,Pointer depths,uint32_t count,Pointer ordering,uint32_t mask,uint32_t bias,int alternate){
    const GroupPC* p=alternate?&alternate_pc:&biased_pc;uint32_t left=count-2,clip,depth;
    TRY(load_vertex(r,xyz,p->load0));TRY(math(r,p->project0,NBA97_PROJECTION_THREE,0,0,0));xyz=add(xyz,24);
    TRY(math(r,p->clip0,NBA97_PROJECTION_CLIP,0,0,0));TRY(screens_clip(r,packet,p->screen0,p->readclip0,&clip));
    TRY(math(r,p->average0,NBA97_PROJECTION_AVERAGE_THREE,0,0,0));TRY(depth_store(r,depths,p->readdepth0,p->storedepth0,mask,bias,&depth));depths=add(depths,4);
    if(left!=0)do{
        TRY(load_vertex(r,xyz,p->load1));TRY(math(r,p->project1,NBA97_PROJECTION_THREE,0,0,0));
        if(alternate?signed32(clip)>0:signed32(clip)<=0)TRY(link32(r,packet,ordering,depth,p->link1[0],p->link1[1],p->link1[2]));
        xyz=add(xyz,24);packet=add(packet,32);TRY(math(r,p->clip1,NBA97_PROJECTION_CLIP,0,0,0));
        TRY(screens_clip(r,packet,p->screen1,p->readclip1,&clip));TRY(math(r,p->average1,NBA97_PROJECTION_AVERAGE_THREE,0,0,0));
        TRY(depth_store(r,depths,p->readdepth1,p->storedepth1,mask,bias,&depth));--left;depths=add(depths,4);
    }while(signed32(left)>0);
    TRY(load_vertex(r,xyz,p->load2));TRY(math(r,p->project2,NBA97_PROJECTION_THREE,0,0,0));
    if(alternate?signed32(clip)>0:signed32(clip)<=0)TRY(link32(r,packet,ordering,depth,p->link2[0],p->link2[1],p->link2[2]));
    packet=add(packet,32);TRY(math(r,p->clip2,NBA97_PROJECTION_CLIP,0,0,0));TRY(screens_clip(r,packet,p->screen2,p->readclip2,&clip));
    TRY(math(r,p->average2,NBA97_PROJECTION_AVERAGE_THREE,0,0,0));TRY(depth_store(r,depths,p->readdepth2,p->storedepth2,mask,bias,&depth));
    if(alternate?signed32(clip)>0:signed32(clip)<=0)TRY(link32(r,packet,ordering,depth,p->link3[0],p->link3[1],p->link3[2]));
    return NBA97_BODY_OK;
}
static int assembled(Run* r,Pointer packet,Pointer corners,uint32_t count,Pointer depths,Pointer ordering){
    Pointer corner[3],depth;uint32_t xy[3],clip,d;unsigned i;
    for(i=0;i<3;++i)TRY(pointer(r,add(corners,i*4),0x80054a20+i*4,&corner[i]));
    corners=add(corners,12);
    do{
        for(i=0;i<3;++i)TRY(raw(r,corner[i],0x80054a40+i*4,4,&xy[i]));
        for(i=0;i<3;++i)TRY(math(r,0x80054a4c+i*4,NBA97_PROJECTION_SCREEN_LOAD,i,xy[i],0));
        TRY(pointer(r,depths,0x80054a58,&depth));TRY(math(r,0x80054a60,NBA97_PROJECTION_CLIP,0,0,0));
        /*54A64..6C fetch the NEXT corner references even on the final pass.
         * Contents may be dead/unknown; spans/alignment are still reached. */
        for(i=0;i<3;++i)TRY(pointer(r,add(corners,i*4),0x80054a64+i*4,&corner[i]));
        corners=add(corners,12);--count;
        TRY(raw(r,depth,0x80054a78,4,&d));TRY(math(r,0x80054a7c,NBA97_PROJECTION_MAC0,0,0,&clip));
        TRY(write(r,add(packet,8),0x80054a80,4,xy[0]));depths=add(depths,12);
        if(signed32(clip)<=0){uint32_t old,encoded;Pointer entry=add(ordering,d);
            TRY(raw(r,entry,0x80054a90,3,&old));
            TRY(write(r,add(packet,16),0x80054a94,4,xy[1]));TRY(write(r,add(packet,24),0x80054a98,4,xy[2]));
            TRY(address(r,packet,0x80054a9c,&encoded));TRY(write(r,entry,0x80054a9c,3,encoded));TRY(write(r,packet,0x80054aa0,3,old));
        }
        packet=add(packet,32);
    }while(signed32(count)>0);
    return NBA97_BODY_OK;
}
static int setup(Run* r,const Nba97GamePlayerProjectionInput* in,Nba97GamePlayerGeometryWrite* journal,size_t capacity,Nba97GamePlayerProjectionProgress* out){
    if(!in||!out||!in->math||(!journal&&capacity)||(!in->buffers&&in->buffer_count))return NBA97_BODY_ARGUMENT;
    memset(out,0,sizeof *out);r->in=in;r->journal=journal;r->capacity=capacity;r->out=out;return NBA97_BODY_OK;
}
int nba97_game_player_project_group(const Nba97GamePlayerProjectionInput* in,uint32_t group,Ref xyz,Ref packets,Ref depths,uint32_t count,Ref ordering,uint32_t mask,uint32_t bias,Nba97GamePlayerGeometryWrite* journal,size_t capacity,Nba97GamePlayerProjectionProgress* out){
    Run r;TRY(setup(&r,in,journal,capacity,out));
    if(group==0x80054660)TRY(primary(&r,pof(xyz),pof(packets),pof(depths),count,pof(ordering),mask));
    else if(group==0x8005483c||group==0x80054adc)TRY(other_group(&r,pof(xyz),pof(packets),pof(depths),count,pof(ordering),mask,group==0x8005483c?bias:0,group==0x80054adc));
    else return NBA97_BODY_ARGUMENT;
    out->completed=1;return NBA97_BODY_OK;
}
int nba97_game_player_assemble(const Nba97GamePlayerProjectionInput* in,Ref packets,Ref corners,uint32_t count,Ref depths,Ref ordering,Nba97GamePlayerGeometryWrite* journal,size_t capacity,Nba97GamePlayerProjectionProgress* out){
    Run r;TRY(setup(&r,in,journal,capacity,out));TRY(assembled(&r,pof(packets),pof(corners),count,pof(depths),pof(ordering)));out->completed=1;return NBA97_BODY_OK;
}
static int rotation(Run* r,Pointer source){return load_rotation(r,source,0x80055f18,0x80055f2c);}
static int translation(Run* r,Pointer source){
    uint32_t words[3];unsigned i;
    for(i=0;i<3;++i)TRY(raw(r,add(source,20+i*4),0x80055f44+i*4,4,&words[i]));
    for(i=0;i<3;++i)TRY(math(r,i==2?0x80055f5c:0x80055f50+i*4,NBA97_PROJECTION_TRANSLATION,i,words[i],0));
    return NBA97_BODY_OK;
}
static int suppressed(Run* r,uint32_t index_pc,uint32_t suppress_pc,int* out){
    uint32_t mask,index,flag;TRY(raw(r,pof(r->in->mask_1f80000c),0x80055f0c,4,&mask));
    TRY(raw(r,pof(r->in->index_1029b0),index_pc,4,&index));
    if(mask&(1u<<(index&31))){*out=1;return NBA97_BODY_OK;}
    TRY(raw(r,pof(r->in->suppress_dcf10),suppress_pc,4,&flag));*out=flag!=0;return NBA97_BODY_OK;
}
int nba97_game_player_projection(const Nba97GamePlayerProjectionInput* in,Nba97GamePlayerGeometryWrite* journal,size_t capacity,Nba97GamePlayerProjectionProgress* out){
    Run r;Pointer context,desc,depths,matrix,part,header,xyz,packet,parent,ordering,alternate,corners;uint32_t bank,count;unsigned i;int skip;
    TRY(setup(&r,in,journal,capacity,out));
    TRY(pointer(&r,pof(in->context_f0ed4),0x800525b0,&context));TRY(pointer(&r,add(context,0xbc4),0x800525ec,&desc));
    matrix=add(context,0x24);part=add(context,0xa4);TRY(pointer(&r,add(desc,8),0x800525fc,&depths));
    for(i=0;i<20;++i,matrix=add(matrix,0x94),part=add(part,0x94)){
        uint32_t p=i==9?0x80052690:0x80052638;
        if(i==0||i==4||i==10||i==12||i==16)continue;
        TRY(raw(&r,pof(in->bank_1ede8),p,4,&bank));TRY(pointer(&r,add(part,12),p+4,&header));
        TRY(pointer(&r,add(header,(bank<<2)+8),p+16,&packet));TRY(pointer(&r,add(header,4),p+20,&xyz));TRY(raw(&r,header,p+24,4,&count));
        TRY(rotation(&r,matrix));TRY(pointer(&r,part,i==9?0x800526b4:0x8005265c,&parent));TRY(translation(&r,parent));
        TRY(pointer(&r,pof(in->ordering_102924),i==9?0x800526c8:0x80052670,&ordering));
        if(i==9){
            TRY(other_group(&r,xyz,packet,depths,6,ordering,4095,12,0));
            TRY(pointer(&r,pof(in->ordering_102924),0x800526f4,&ordering));
            TRY(primary(&r,add(xyz,144),add(packet,192),add(depths,24),count-6,ordering,4095));
        }else TRY(primary(&r,xyz,packet,depths,count,ordering,4095));
        depths=add(depths,count<<2);
    }
    TRY(suppressed(&r,0x8005273c,0x80052758,&skip));
    if(!skip){
        TRY(pointer(&r,pof(in->context_f0ed4),0x8005276c,&context));TRY(pointer(&r,add(context,0xbc4),0x80052774,&desc));
        matrix=add(context,0x24);TRY(pointer(&r,add(context,0xbc8),0x8005277c,&alternate));part=add(context,0xa8);TRY(pointer(&r,add(desc,12),0x80052784,&depths));
        for(i=0;i<8;++i,matrix=add(matrix,0x94),part=add(part,0x94)){
            if(i==0||i==4)continue;
            TRY(raw(&r,pof(in->bank_1ede8),0x8005279c,4,&bank));TRY(raw(&r,alternate,0x800527a0,4,&count));TRY(pointer(&r,add(part,8),0x800527a4,&header));
            TRY(pointer(&r,add(alternate,(bank<<2)+8),0x800527b0,&packet));TRY(pointer(&r,add(header,4),0x800527b4,&xyz));
            TRY(rotation(&r,add(matrix,32)));TRY(pointer(&r,part,0x800527c0,&parent));alternate=add(alternate,16);TRY(translation(&r,parent));
            TRY(pointer(&r,pof(in->ordering_102924),0x800527d8,&ordering));TRY(other_group(&r,xyz,packet,depths,count,ordering,4095,0,1));depths=add(depths,count<<2);
        }
    }
    TRY(pointer(&r,pof(in->context_f0ed4),0x80052814,&context));TRY(raw(&r,pof(in->bank_1ede8),0x8005281c,4,&bank));TRY(pointer(&r,add(context,0xbc4),0x80052820,&desc));
    TRY(pointer(&r,add(desc,(bank<<2)+40),0x8005282c,&packet));TRY(pointer(&r,add(desc,(bank<<2)+24),0x80052830,&corners));TRY(raw(&r,desc,0x80052834,4,&count));TRY(pointer(&r,add(desc,16),0x80052838,&depths));
    TRY(pointer(&r,pof(in->ordering_102924),0x80052840,&ordering));TRY(assembled(&r,packet,corners,count,depths,ordering));
    TRY(suppressed(&r,0x8005286c,0x80052888,&skip));
    if(!skip){
        TRY(pointer(&r,pof(in->context_f0ed4),0x8005289c,&context));TRY(raw(&r,pof(in->bank_1ede8),0x800528a4,4,&bank));TRY(pointer(&r,add(context,0xbc4),0x800528a8,&desc));
        TRY(pointer(&r,add(desc,(bank<<2)+48),0x800528b4,&packet));TRY(pointer(&r,add(desc,(bank<<2)+32),0x800528b8,&corners));TRY(raw(&r,add(desc,4),0x800528bc,4,&count));TRY(pointer(&r,add(desc,20),0x800528c0,&depths));
        TRY(pointer(&r,pof(in->ordering_102924),0x800528c8,&ordering));TRY(assembled(&r,packet,corners,count,depths,ordering));
    }
    out->completed=1;return NBA97_BODY_OK;
}
