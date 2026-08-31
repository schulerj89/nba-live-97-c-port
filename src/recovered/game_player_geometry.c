#include "game_player_geometry.h"
#include <string.h>
typedef Nba97GameBodyReference Ref;
typedef struct Pointer {Ref ref;int unresolved;} Pointer;
typedef struct Run {const Nba97GamePlayerGeometryInput* in;Nba97GamePlayerGeometryWrite* journal;size_t capacity;Nba97GamePlayerGeometryProgress* out;} Run;
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
    for(i=0;i<5;++i)TRY(math(r,load_pc+i*4,NBA97_PLAYER_ROTATION,i,w[i],0));
    return NBA97_BODY_OK;
}
static int local_rotation(Run* r,Pointer pose,unsigned part,Pointer root,uint16_t* matrix){
    uint32_t angle[3],packed,extra;uint32_t sine[3],cosine[3],sxsy,cxsy;unsigned axis;
    static const uint32_t positive_pc[]={0x80055494,0x800554f4,0x80055588};
    static const uint32_t negative_pc[]={0x8005546c,0x800554cc,0x80055554};
    static const uint32_t angle_pc[]={0x80055448,0x800554a8,0x80055510};
    static const uint32_t root_read[]={0x800555b8,0x800555d0,0x800555fc,0x80055624,0x80055640};
    static const uint32_t root_load[]={0x800555c4,0x800555e0,0x80055608,0x80055630,0x8005565c};
    if(part==11){
        TRY(raw(r,pose,0x800553f0,2,&angle[0]));TRY(raw(r,add(pose,2),0x800553f4,2,&angle[1]));
        TRY(raw(r,pof(r->in->angle_103edc),0x800553fc,4,&extra));TRY(raw(r,add(pose,4),0x80055400,2,&angle[2]));
        angle[0]=(angle[0]+extra)&65535;
    }
    for(axis=0;axis<3;++axis){int32_t a;uint32_t index;
        if(part!=11)TRY(raw(r,add(pose,axis*2),angle_pc[axis],2,&angle[axis]));
        a=s16(angle[axis]);index=(a<0?0u-(uint32_t)a:(uint32_t)a)&4095;
        TRY(raw(r,add(pof(r->in->trig_b3254),index*4),a<0?negative_pc[axis]:positive_pc[axis],4,&packed));
        sine[axis]=(uint32_t)s16(packed);if(a<0)sine[axis]=0u-sine[axis];cosine[axis]=(uint32_t)s16(packed>>16);
    }
    /* Source MULTU/MFLO then SRA12: truncate to32bits BEFORE shifting. Each SH
     * truncates again. Do not replace this with a floating-point Euler matrix. */
    matrix[6]=(uint16_t)(0u-sine[1]);matrix[7]=(uint16_t)mul12(sine[0],cosine[1]);matrix[8]=(uint16_t)mul12(cosine[0],cosine[1]);
    matrix[0]=(uint16_t)mul12(cosine[1],cosine[2]);matrix[3]=(uint16_t)mul12(sine[2],cosine[1]);
    sxsy=mul12(sine[0],sine[1]);cxsy=mul12(sine[1],cosine[0]);
    matrix[1]=(uint16_t)(mul12(sxsy,cosine[2])-mul12(sine[2],cosine[0]));
    matrix[4]=(uint16_t)(mul12(sxsy,sine[2])+mul12(cosine[0],cosine[2]));
    matrix[2]=(uint16_t)(mul12(cxsy,cosine[2])+mul12(sine[0],sine[2]));
    matrix[5]=(uint16_t)(mul12(cxsy,sine[2])-mul12(sine[0],cosine[2]));
    for(axis=0;axis<5;++axis){if(axis==4)TRY(low_word(r,add(root,axis*4),root_read[axis],&packed));else TRY(raw(r,add(root,axis*4),root_read[axis],4,&packed));TRY(math(r,root_load[axis],NBA97_PLAYER_ROTATION,axis,packed,0));}
    return NBA97_BODY_OK;
}
static int vector(Run* r,const uint16_t* m,unsigned col,uint32_t pc){
    TRY(math(r,pc,NBA97_PLAYER_VERTEX,0,(uint32_t)m[col]|((uint32_t)m[col+3]<<16),0));
    return math(r,pc+4,NBA97_PLAYER_VERTEX,1,(uint32_t)s16(m[col+6]),0);
}
/* Compose three columns. The local scratch words are returned for the second
 * world-chain pass; visible matrices retain source0,C,4,8,10 store order. */
static int compose(Run* r,const uint16_t* m,Pointer dst,unsigned variant,uint32_t* scratch){
    static const uint32_t vp[3][3]={{0x80055698,0x800556c8,0x800556e8},{0x8005578c,0x800557bc,0x800557e8},{0x80055a08,0x80055a38,0x80055a64}};
    static const uint32_t cp[3][3]={{0x800556a4,0x800556d4,0x800556fc},{0x80055798,0x800557c8,0x800557f4},{0x80055a14,0x80055a44,0x80055a70}};
    static const uint32_t rp[3][3]={{0x800556bc,0x800556f0,0x80055720},{0x800557b0,0x800557dc,0x80055818},{0x80055a2c,0x80055a58,0x80055a94}};
    static const uint32_t wp[3][5]={{0x8005570c,0x8005571c,0x80055730,0x80055740,0x80055744},{0x80055804,0x80055814,0x8005582c,0x8005583c,0x80055840},{0x80055a80,0x80055a90,0x80055aa8,0x80055ab8,0x80055abc}};
    uint32_t a[3],b[3],last[3],words[5];unsigned i;
    TRY(vector(r,m,0,vp[variant][0]));TRY(math(r,cp[variant][0],NBA97_PLAYER_ROTATE,0,0,0));
    for(i=0;i<3;++i)TRY(math(r,rp[variant][0]+i*4,NBA97_PLAYER_IR,i,0,&a[i]));
    TRY(vector(r,m,1,vp[variant][1]));TRY(math(r,cp[variant][1],NBA97_PLAYER_ROTATE,0,0,0));
    if(!variant)TRY(vector(r,m,2,vp[variant][2]));
    for(i=0;i<3;++i)TRY(math(r,rp[variant][1]+i*4,NBA97_PLAYER_IR,i,0,&b[i]));
    if(variant)TRY(vector(r,m,2,vp[variant][2]));
    TRY(math(r,cp[variant][2],NBA97_PLAYER_ROTATE,0,0,0));
    words[0]=(a[0]&65535)|(b[0]<<16);words[3]=(a[2]&65535)|(b[2]<<16);
    if(variant!=1){TRY(write(r,dst,wp[variant][0],4,words[0]));TRY(write(r,add(dst,12),wp[variant][1],4,words[3]));}
    TRY(math(r,rp[variant][2],NBA97_PLAYER_IR,0,0,&last[0]));TRY(math(r,rp[variant][2]+4,NBA97_PLAYER_IR,1,0,&last[1]));
    words[1]=(last[0]&65535)|(a[1]<<16);words[2]=(b[1]&65535)|(last[1]<<16);
    if(variant!=1){TRY(write(r,add(dst,4),wp[variant][2],4,words[1]));TRY(write(r,add(dst,8),wp[variant][3],4,words[2]));}
    TRY(math(r,wp[variant][4],NBA97_PLAYER_IR,2,0,&words[4]));
    /* Original SWC2 IR3 stores a full sign-extended word, including matrixpad. */
    if(variant!=1)TRY(write(r,add(dst,16),wp[variant][4],4,words[4]));
    if(scratch)memcpy(scratch,words,sizeof words);
    return NBA97_BODY_OK;
}
static int endpoint(Run* r,Pointer part,Pointer parent,Pointer dst,uint32_t read_pc,uint32_t ptr_pc,uint32_t load_pc,uint32_t store_pc){
    uint32_t t[3],v;Pointer pivot;unsigned i;
    for(i=0;i<3;++i)TRY(raw(r,add(parent,0x14+i*4),read_pc+i*4,4,&t[i]));
    TRY(pointer(r,add(part,0x88),ptr_pc,&pivot));
    TRY(math(r,load_pc,NBA97_PLAYER_TRANSLATION,0,t[0],0));
    TRY(raw(r,pivot,load_pc+4,4,&v));TRY(math(r,load_pc+4,NBA97_PLAYER_VERTEX,0,v,0));
    TRY(low_word(r,add(pivot,4),load_pc+8,&v));TRY(math(r,load_pc+8,NBA97_PLAYER_VERTEX,1,v,0));
    TRY(math(r,load_pc+12,NBA97_PLAYER_TRANSLATION,1,t[1],0));TRY(math(r,load_pc+16,NBA97_PLAYER_TRANSLATION,2,t[2],0));
    TRY(math(r,load_pc+20,NBA97_PLAYER_TRANSFORM,0,0,0));
    for(i=0;i<3;++i){TRY(math(r,store_pc+i*4,NBA97_PLAYER_MAC,i,0,&v));TRY(write(r,add(dst,i*4),store_pc+i*4,4,v));}
    return NBA97_BODY_OK;
}
static int hotpoint(Run* r,Pointer context,Pointer work,Ref slot,uint32_t start){
    uint32_t w[3],base[3];Pointer dst;unsigned i;static const unsigned offsets[]={4,12,8};
    for(i=0;i<3;++i)TRY(raw(r,add(work,0x14+i*4),start+i*4,4,&w[i]));
    for(i=0;i<3;++i)TRY(raw(r,add(context,offsets[i]),start+12+i*4,4,&base[i]));
    TRY(pointer(r,pof(slot),start+36,&dst));
    for(i=0;i<3;++i)TRY(write(r,add(dst,i*2),start+44+i*4+(i==2?4:0),2,w[i]-base[i]));
    return NBA97_BODY_OK;
}
int nba97_game_player_geometry(const Nba97GamePlayerGeometryInput* in,Nba97GamePlayerGeometryWrite* journal,size_t capacity,Nba97GamePlayerGeometryProgress* out){
    Run r;Pointer context,part,pose,parent,root,work[4],other,dummy;uint16_t local[9];uint32_t world[5];unsigned n,i;
    if(!in||!out||!in->math||(!journal&&capacity)||(!in->buffers&&in->buffer_count))return NBA97_BODY_ARGUMENT;
    memset(out,0,sizeof *out);r.in=in;r.journal=journal;r.capacity=capacity;r.out=out;
    TRY(pointer(&r,pof(in->context_f0ed4),0x8005538c,&context));part=add(context,0x24);
    TRY(pointer(&r,add(context,0xbc0),0x80055398,&pose));pose=add(pose,4);
    TRY(pointer(&r,add(part,0x80),0x800553a8,&parent));TRY(pointer(&r,pof(in->root_10292c),0x800553b0,&root));
    TRY(pointer(&r,pof(in->work_f1c4c),0x800553b8,&work[0]));TRY(pointer(&r,pof(in->work_f9cf8),0x800553c0,&work[1]));
    TRY(pointer(&r,pof(in->work_f9c54),0x800553c8,&work[2]));TRY(pointer(&r,pof(in->work_f9d00),0x800553d0,&work[3]));
    for(n=0;n<20;++n){
        if(n==8)TRY(pointer(&r,add(context,0xbbc),0x800553e4,&pose));
        TRY(local_rotation(&r,pose,n,root,local));TRY(compose(&r,local,part,0,0));
        TRY(load_rotation(&r,work[3],0x8005574c,0x80055760));TRY(compose(&r,local,part,1,world));
        TRY(load_rotation(&r,part,0x80055858,0x8005586c));
        TRY(endpoint(&r,part,parent,add(part,0x54),0x80055880,0x8005588c,0x80055890,0x800558a8));
        if(n<8){
            /*55988..559A8 negates exactly these three scratch halfwords. */
            local[3]=(uint16_t)(0u-local[3]);local[4]=(uint16_t)(0u-local[4]);local[8]=(uint16_t)(0u-local[8]);
            TRY(load_rotation(&r,root,0x800559cc,0x800559e0));TRY(compose(&r,local,add(part,0x20),2,0));
            TRY(load_rotation(&r,add(part,0x20),0x80055ad4,0x80055aec));
            TRY(pointer(&r,add(part,0x84),0x80055b00,&other));
            /* The pivot pointer is loaded before the three parent coordinates
             * in this branch, unlike the primary/world endpoint paths. */
            TRY(pointer(&r,add(part,0x88),0x80055b04,&dummy));
            {uint32_t t[3],v;for(i=0;i<3;++i)TRY(raw(&r,add(other,0x14+i*4),0x80055b08+i*4,4,&t[i]));
                TRY(math(&r,0x80055b14,NBA97_PLAYER_TRANSLATION,0,t[0],0));
                TRY(raw(&r,dummy,0x80055b18,4,&v));TRY(math(&r,0x80055b18,NBA97_PLAYER_VERTEX,0,v,0));
                TRY(low_word(&r,add(dummy,4),0x80055b1c,&v));TRY(math(&r,0x80055b1c,NBA97_PLAYER_VERTEX,1,v,0));
                TRY(math(&r,0x80055b20,NBA97_PLAYER_TRANSLATION,1,t[1],0));TRY(math(&r,0x80055b24,NBA97_PLAYER_TRANSLATION,2,t[2],0));
                TRY(math(&r,0x80055b28,NBA97_PLAYER_TRANSFORM,0,0,0));
                for(i=0;i<3;++i){TRY(math(&r,0x80055b2c+i*4,NBA97_PLAYER_MAC,i,0,&v));TRY(write(&r,add(part,0x74+i*4),0x80055b2c+i*4,4,v));}
            }
        }
        for(i=0;i<5;++i)TRY(math(&r,(n<8?0x80055b4c:0x800558d4)+i*4,NBA97_PLAYER_ROTATION,i,world[i],0));
        if(n!=10&&n!=11){
            uint32_t start;unsigned src,dst;
            switch(n){
            case 0:start=0x80055ba4;src=3;dst=0;break;case 4:start=0x80055be0;src=3;dst=1;break;
            case 1:case 2:start=0x80055c1c;src=dst=0;break;case 5:case 6:start=0x80055c54;src=dst=1;break;
            case 3:start=0x80055c8c;src=dst=0;break;case 7:start=0x80055cfc;src=dst=1;break;
            case 9:start=0x80055da4;src=dst=3;break;case 15:start=0x80055e58;src=dst=2;break;
            case 19:start=0x80055de8;src=dst=3;break;case 12:case 13:case 14:start=0x80055ec8;src=dst=2;break;
            default:start=0x80055d6c;src=dst=3;break;
            }
            /* Some third SWC2s are in a jump delay slot (+34, not+30). */
            {uint32_t t[3],v;Pointer pivot;for(i=0;i<3;++i)TRY(raw(&r,add(work[src],0x14+i*4),start+i*4,4,&t[i]));
                TRY(pointer(&r,add(part,0x88),start+12,&pivot));TRY(math(&r,start+16,NBA97_PLAYER_TRANSLATION,0,t[0],0));
                TRY(raw(&r,pivot,start+20,4,&v));TRY(math(&r,start+20,NBA97_PLAYER_VERTEX,0,v,0));
                TRY(low_word(&r,add(pivot,4),start+24,&v));TRY(math(&r,start+24,NBA97_PLAYER_VERTEX,1,v,0));
                TRY(math(&r,start+28,NBA97_PLAYER_TRANSLATION,1,t[1],0));TRY(math(&r,start+32,NBA97_PLAYER_TRANSLATION,2,t[2],0));
                TRY(math(&r,start+36,NBA97_PLAYER_TRANSFORM,0,0,0));
                for(i=0;i<3;++i){uint32_t pc=start+40+i*4;if(i==2&&(n==1||n==2||n==5||n==6||n==8||n==12||n==13||n==14||n==16||n==17||n==18))pc+=4;
                    TRY(math(&r,pc,NBA97_PLAYER_MAC,i,0,&v));TRY(write(&r,add(work[dst],0x14+i*4),pc,4,v));}
                if(n==9)for(i=0;i<3;++i){uint32_t pc=0x80055dd8+i*4+(i==2?4:0);TRY(math(&r,pc,NBA97_PLAYER_MAC,i,0,&v));TRY(write(&r,add(work[2],0x14+i*4),pc,4,v));}
            }
            if(n==3)TRY(hotpoint(&r,context,work[0],in->foot_f9d04,0x80055cc0));
            if(n==7)TRY(hotpoint(&r,context,work[1],in->foot_fea38,0x80055d30));
            if(n==15)TRY(hotpoint(&r,context,work[2],in->hand_f0fb4,0x80055e8c));
            if(n==19)TRY(hotpoint(&r,context,work[3],in->hand_fc62c,0x80055e1c));
        }
        out->parts_completed=(uint8_t)(n+1);pose=add(pose,8);part=add(part,0x94);
        /* Original indexing quirk5594C: the21st parent slot is read even though
         * it is never dereferenced. Do not silently remove this final access. */
        TRY(pointer(&r,add(part,0x80),0x8005594c,&parent));
    }
    out->completed=1;return NBA97_BODY_OK;
}
