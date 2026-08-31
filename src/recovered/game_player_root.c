#include "game_player_root.h"
#include <string.h>
typedef Nba97GameBodyReference Ref;
typedef struct Pointer {Ref ref;int unresolved;} Pointer;
typedef struct Run {const Nba97GamePlayerRootInput* in;Nba97GamePlayerGeometryWrite* journal;size_t capacity;Nba97GamePlayerRootProgress* out;} Run;
#define TRY(x) do{int r_=(x);if(r_!=NBA97_BODY_OK)return r_;}while(0)
static int32_t s16(uint32_t v){v&=65535;return v<32768?(int32_t)v:(int32_t)v-65536;}
static uint32_t mul12(uint32_t a,uint32_t b){uint32_t p=a*b;return(p>>12)|((p&0x80000000u)?0xfff00000u:0);}
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
    if(((uint64_t)p.ref.offset+b->address_mod4)&(width-1u))return NBA97_BODY_ALIGNMENT_TRAP;
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
     * and V0Z discard the upperhalf. Unknown padding need not become invented
     * known data. Bounds/alignment/canonical checks above cover the fullspan. */
    if(b->known&&(!b->known[p.ref.offset]||!b->known[p.ref.offset+1]))return NBA97_BODY_UNKNOWN;
    *value=(uint32_t)b->bytes[p.ref.offset]|((uint32_t)b->bytes[p.ref.offset+1]<<8);return NBA97_BODY_OK;
}
static int pointer(Run* r,Pointer p,uint32_t pc,Pointer* out){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;int unknown;Ref zero={0,0,0};
    TRY(access(r,p,pc,4,&b,&c,&unknown));
    /* A loaded pointer may never be used (notably5594C afterpart19). Preserve
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
    e=&r->journal[r->out->writes++];memset(e,0,sizeof *e);e->destination=p.ref;e->pc=pc;e->width=(uint8_t)width;e->word=width==2?value&65535:value;
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
    for(i=0;i<5;++i)TRY(math(r,(load_pc==0x80055f2c&&i==4)?0x80055f40:load_pc+i*4,NBA97_PLAYER_ROTATION,i,w[i],0));
    return NBA97_BODY_OK;
}
typedef struct Scratch {uint8_t bytes[8],known[8];} Scratch;
static uint32_t shift(uint32_t v,unsigned n){return(v>>n)|((v&0x80000000u)?(~0u<<(32-n)):0);}
static int high_word(Run* r,Pointer p,uint32_t pc,uint32_t* value){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;int unknown;
    TRY(access(r,p,pc,4,&b,&c,&unknown));
    if(c->is_reference)return c->reference.known?NBA97_BODY_ADDRESS_REQUIRED:NBA97_BODY_UNKNOWN;
    if(b->known&&(!b->known[p.ref.offset+2]||!b->known[p.ref.offset+3]))return NBA97_BODY_UNKNOWN;
    *value=((uint32_t)b->bytes[p.ref.offset+2]<<16)|((uint32_t)b->bytes[p.ref.offset+3]<<24);return NBA97_BODY_OK;
}
static int copy_template(Run* r,Scratch* out){
    unsigned word,i;Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;int unknown;
    for(word=0;word<2;++word){Pointer p=add(pof(r->in->preset_26384),word*4);
        /* Aligned LWL/LWR pairs snapshot raw bytes; unused yaw/padding need
         * not be known. They are overwritten or discarded by actual source. */
        TRY(access(r,p,0x80052028+word*8,4,&b,&c,&unknown));
        if(c->is_reference)return c->reference.known?NBA97_BODY_ADDRESS_REQUIRED:NBA97_BODY_UNKNOWN;
        for(i=0;i<4;++i){out->bytes[word*4+i]=b->bytes[p.ref.offset+i];out->known[word*4+i]=b->known?b->known[p.ref.offset+i]:1;}
    }
    return NBA97_BODY_OK;
}
static int scratch_half(Run* r,const Scratch* s,unsigned off,uint32_t pc,uint32_t* value){
    r->out->stopped_pc=pc;memset(&r->out->stopped_reference,0,sizeof r->out->stopped_reference);
    if(!s->known[off]||!s->known[off+1])return NBA97_BODY_UNKNOWN;
    *value=(uint32_t)s->bytes[off]|((uint32_t)s->bytes[off+1]<<8);return NBA97_BODY_OK;
}
static void scratch_store(Scratch* s,unsigned off,uint32_t value){s->bytes[off]=(uint8_t)value;s->bytes[off+1]=(uint8_t)(value>>8);s->known[off]=s->known[off+1]=1;}
static int trig(Run* r,uint32_t angle,unsigned axis,uint32_t* sine,uint32_t* cosine){
    static const uint32_t pos[]={0x800560d0,0x80056134,0x800561cc},neg[]={0x800560a8,0x8005610c,0x80056198};
    int32_t a=s16(angle);uint32_t word,index=(a<0?0u-(uint32_t)a:(uint32_t)a)&4095;
    TRY(raw(r,add(pof(r->in->trig_b3254),index*4),a<0?neg[axis]:pos[axis],4,&word));
    *sine=(uint32_t)s16(word);if(a<0)*sine=0u-*sine;*cosine=(uint32_t)s16(word>>16);return NBA97_BODY_OK;
}
static int euler(Run* r,const Scratch* angles,Pointer dst){
    uint32_t a,sx,cx,sy,cy,sz,cz,t;
    TRY(scratch_half(r,angles,0,0x80056080,&a));TRY(trig(r,a,0,&sx,&cx));
    TRY(scratch_half(r,angles,2,0x800560e4,&a));TRY(trig(r,a,1,&sy,&cy));
    TRY(scratch_half(r,angles,4,0x80056150,&a));
    TRY(write(r,add(dst,4),0x80056154,2,sy));
    /*56080 negates the full low32 product BEFORE shift, unlike negating a
     * rounded fixed-point result. Preserve its asymmetric negative rounding. */
    TRY(write(r,add(dst,10),0x80056168,2,shift(0u-cy*sx,12)));
    TRY(write(r,add(dst,16),s16(a)<0?0x8005617c:0x800561bc,2,mul12(cy,cx)));
    TRY(trig(r,a,2,&sz,&cz));
    TRY(write(r,dst,0x800561ec,2,mul12(cz,cy)));
    TRY(write(r,add(dst,2),0x80056204,2,shift(0u-sz*cy,12)));
    t=mul12(cz,0u-sy);
    TRY(write(r,add(dst,6),0x8005623c,2,mul12(sz,cx)-mul12(t,sx)));
    TRY(write(r,add(dst,12),0x80056264,2,mul12(sz,sx)+mul12(t,cx)));
    t=mul12(sz,0u-sy);
    TRY(write(r,add(dst,8),0x8005629c,2,mul12(cz,cx)+mul12(t,sx)));
    return write(r,add(dst,14),0x800562c0,2,mul12(cz,sx)-mul12(t,cx));
}
static int scale(Run* r,Pointer matrix,uint32_t factor){
    uint32_t words[9];unsigned i;
    static const uint32_t read_pc[]={0x80051f18,0x80051f24,0x80051f38,0x80051f4c,0x80051f60,0x80051f74,0x80051f88,0x80051f9c,0x80051fb0};
    static const uint32_t write_pc[]={0x80051fb8,0x80051fc4,0x80051fd0,0x80051fd8,0x80051fe0,0x80051fe8,0x80051ff0,0x80051ff8,0x80052008};
    /*51F18 snapshots all nine halves before its first store. Factors/products
     * wrap32bits; signed shift16 then SH, with no scale clamp or repair. */
    for(i=0;i<9;++i)TRY(raw(r,add(matrix,i*2),read_pc[i],2,&words[i]));
    for(i=0;i<9;++i)TRY(write(r,add(matrix,i*2),write_pc[i],2,shift(factor*(uint32_t)s16(words[i]),16)));
    return NBA97_BODY_OK;
}
static int compose_matrix(Run* r,Pointer a,Pointer b,Pointer dst){
    uint32_t x,y,z,first[3],second[3],v;unsigned i;
    TRY(load_rotation(r,a,0x800562cc,0x800562e0));
    TRY(raw(r,b,0x800562f4,2,&x));TRY(high_word(r,add(b,4),0x800562f8,&y));TRY(low_word(r,add(b,12),0x800562fc,&z));
    TRY(math(r,0x8005630c,NBA97_PLAYER_VERTEX,0,x|y,0));TRY(math(r,0x80056310,NBA97_PLAYER_VERTEX,1,z,0));TRY(math(r,0x80056318,NBA97_PLAYER_ROTATE,0,0,0));
    TRY(raw(r,add(b,2),0x8005631c,2,&x));TRY(low_word(r,add(b,8),0x80056320,&y));TRY(raw(r,add(b,14),0x80056324,2,&z));
    for(i=0;i<3;++i)TRY(math(r,0x80056330+i*4,NBA97_PLAYER_IR,i,0,&first[i]));
    TRY(math(r,0x8005633c,NBA97_PLAYER_VERTEX,0,x|(y<<16),0));TRY(math(r,0x80056340,NBA97_PLAYER_VERTEX,1,z,0));TRY(math(r,0x80056348,NBA97_PLAYER_ROTATE,0,0,0));
    TRY(raw(r,add(b,4),0x8005634c,2,&x));TRY(high_word(r,add(b,8),0x80056350,&y));TRY(low_word(r,add(b,16),0x80056354,&z));
    for(i=0;i<3;++i)TRY(math(r,0x80056364+i*4,NBA97_PLAYER_IR,i,0,&second[i]));
    TRY(math(r,0x80056370,NBA97_PLAYER_VERTEX,0,x|y,0));TRY(math(r,0x80056374,NBA97_PLAYER_VERTEX,1,z,0));TRY(math(r,0x8005637c,NBA97_PLAYER_ROTATE,0,0,0));
    TRY(write(r,dst,0x8005638c,4,(first[0]&65535)|(second[0]<<16)));
    TRY(write(r,add(dst,12),0x8005639c,4,(first[2]&65535)|(second[2]<<16)));
    TRY(math(r,0x800563a0,NBA97_PLAYER_IR,0,0,&x));TRY(math(r,0x800563a4,NBA97_PLAYER_IR,1,0,&y));
    TRY(write(r,add(dst,4),0x800563b4,4,(x&65535)|(first[1]<<16)));
    TRY(write(r,add(dst,8),0x800563c4,4,(second[1]&65535)|(y<<16)));
    TRY(math(r,0x800563c8,NBA97_PLAYER_IR,2,0,&v));return write(r,add(dst,16),0x800563c8,4,v);
}
static int transform_scratch(Run* r,const Scratch* s,Pointer dst){
    uint32_t x,y,z,v;unsigned i;
    TRY(scratch_half(r,s,0,0x80056650,&x));TRY(scratch_half(r,s,2,0x80056650,&y));TRY(math(r,0x80056650,NBA97_PLAYER_VERTEX,0,x|(y<<16),0));
    TRY(scratch_half(r,s,4,0x80056654,&z));TRY(math(r,0x80056654,NBA97_PLAYER_VERTEX,1,z,0));TRY(math(r,0x8005665c,NBA97_PLAYER_TRANSFORM,0,0,0));
    for(i=0;i<3;++i){TRY(math(r,0x80056660+i*4,NBA97_PLAYER_MAC,i,0,&v));TRY(write(r,add(dst,i*4),0x80056660+i*4,4,v));}
    /*56674 writes FLAG to private source stack; no persistent output/alias. */
    return math(r,0x8005666c,NBA97_ROOT_FLAGS,0,0,&v);
}
static int project(Run* r,Pointer vertex,Pointer screen,uint32_t* depth){
    uint32_t v,unused;
    TRY(raw(r,vertex,0x80056624,4,&v));TRY(math(r,0x80056624,NBA97_PLAYER_VERTEX,0,v,0));
    TRY(low_word(r,add(vertex,4),0x80056628,&v));TRY(math(r,0x80056628,NBA97_PLAYER_VERTEX,1,v,0));
    TRY(math(r,0x80056630,NBA97_ROOT_PROJECT,0,0,0));
    TRY(math(r,0x80056634,NBA97_ROOT_SCREEN,0,0,&v));TRY(write(r,screen,0x80056634,4,v));
    TRY(math(r,0x80056638,NBA97_ROOT_IR0,0,0,&unused));TRY(math(r,0x8005663c,NBA97_ROOT_FLAGS,0,0,&unused));
    TRY(math(r,0x80056640,NBA97_ROOT_DEPTH,0,0,&v));*depth=shift(v,2);return NBA97_BODY_OK;
}
int nba97_game_player_root(const Nba97GamePlayerRootInput* in,Nba97GamePlayerGeometryWrite* journal,size_t capacity,Nba97GamePlayerRootProgress* out){
    Run r;Scratch scratch;Pointer context,pose,world,ground;uint32_t index,v,height,factor,y,z,depth,translation[3];unsigned i;
    if(!in||!out||!in->math||(!journal&&capacity)||(!in->buffers&&in->buffer_count))return NBA97_BODY_ARGUMENT;
    memset(out,0,sizeof *out);r.in=in;r.journal=journal;r.capacity=capacity;r.out=out;
    TRY(copy_template(&r,&scratch));
    TRY(pointer(&r,pof(in->context_f0ed4),0x8005204c,&context));TRY(raw(&r,pof(in->index_1029b0),0x80052054,4,&index));
    TRY(raw(&r,add(context,0x16),0x80052060,2,&v));scratch_store(&scratch,2,v<<2);
    TRY(euler(&r,&scratch,add(pof(in->world_fb480),index<<5)));
    TRY(pointer(&r,pof(in->context_f0ed4),0x80052084,&context));TRY(raw(&r,pof(in->index_1029b0),0x8005208c,4,&index));
    TRY(raw(&r,add(context,4),0x80052090,4,&v));world=add(pof(in->world_fb480),index<<5);ground=add(pof(in->ground_102f8c),index<<3);
    TRY(write(&r,add(world,20),0x800520a0,4,v));scratch_store(&scratch,0,v);TRY(write(&r,ground,0x800520b0,2,v));
    TRY(pointer(&r,add(context,0xbc0),0x800520b4,&pose));TRY(raw(&r,add(pose,2),0x800520c4,2,&height));
    TRY(raw(&r,add(pof(in->scales_105f48),index<<2),0x800520cc,4,&factor));
    TRY(raw(&r,add(context,12),0x800520dc,4,&v));
    y=shift(shift(height<<16,20)*factor,16)+v+0x24u;
    TRY(write(&r,add(world,24),0x800520f0,4,y));scratch_store(&scratch,2,y);
    TRY(raw(&r,add(context,8),0x800520f8,4,&z));TRY(write(&r,add(world,28),0x80052100,4,z));scratch_store(&scratch,4,z);
    TRY(write(&r,add(ground,4),0x80052110,2,z));TRY(write(&r,add(ground,2),0x8005211c,2,0));
    /*52120 reloads the scale after the preceding stores, preserving aliases. */
    TRY(raw(&r,add(pof(in->scales_105f48),index<<2),0x80052120,4,&factor));TRY(scale(&r,world,factor));
    TRY(raw(&r,pof(in->index_1029b0),0x80052130,4,&index));
    TRY(compose_matrix(&r,pof(in->camera_f9fd8),add(pof(in->world_fb480),index<<5),add(pof(in->primary_103fd8),index<<5)));
    TRY(load_rotation(&r,pof(in->camera_f9fd8),0x80055f18,0x80055f2c));
    for(i=0;i<3;++i)TRY(raw(&r,add(pof(in->camera_f9fd8),20+i*4),0x80055f44+i*4,4,&translation[i]));
    for(i=0;i<3;++i)TRY(math(&r,i==2?0x80055f5c:0x80055f50+i*4,NBA97_PLAYER_TRANSLATION,i,translation[i],0));
    TRY(raw(&r,pof(in->index_1029b0),0x8005216c,4,&index));TRY(transform_scratch(&r,&scratch,add(pof(in->primary_103fd8),(index<<5)+20)));
    TRY(scratch_half(&r,&scratch,2,0x80051f04,&v));scratch_store(&scratch,2,0u-v);
    TRY(raw(&r,pof(in->index_1029b0),0x80052198,4,&index));TRY(transform_scratch(&r,&scratch,add(pof(in->alternate_10b2b8),(index<<5)+20)));
    TRY(raw(&r,pof(in->index_1029b0),0x800521bc,4,&index));TRY(project(&r,add(pof(in->ground_102f8c),index<<3),add(pof(in->screen_fea94),index<<2),&depth));
    TRY(raw(&r,pof(in->index_1029b0),0x800521f0,4,&index));TRY(write(&r,add(pof(in->depth_106038),index<<2),0x80052204,4,depth));
    out->completed=1;return NBA97_BODY_OK;
}
