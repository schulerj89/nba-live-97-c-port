#include "game_pose_frame.h"
#include "game_pose_sample.h"
#include <string.h>
typedef struct Run {Nba97PlayerFrameContext* in;Nba97PlayerFrameProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_BODY_OK)return status_;}while(0)
static int32_t s32(uint32_t v){return v<0x80000000u?(int32_t)v:-1-(int32_t)~v;}
static int16_t half(uint32_t v){return (int16_t)(v&0x8000u?(int32_t)(v&65535u)-65536:(int32_t)(v&65535u));}
static uint32_t sx16(uint32_t v){return (uint32_t)(int32_t)half(v);}
static uint32_t asr8(uint32_t v){return (v>>8)|((v&0x80000000u)?0xff000000u:0);}
static int access(Run* r,uint32_t pc,uint32_t a,unsigned n,unsigned kind,Nba97PlayerFrameValue* v){
    unsigned i;int status;r->out->stopped_pc=pc;r->out->stopped_address=a;
    if(r->out->operations>=r->in->operation_budget)return NBA97_BODY_JOURNAL_LIMIT;
    ++r->out->operations;if(a&(n-1u))return NBA97_BODY_ALIGNMENT_TRAP;
    status=r->in->access(r->in->user,pc,a,n,kind,v);if(status!=NBA97_BODY_OK)return status;
    if(v->is_reference>1||v->reference.known>1||(!v->reference.known&&(v->reference.allocation||v->reference.offset)))return NBA97_BODY_ARGUMENT;
    if(!v->is_reference&&v->reference.known)return NBA97_BODY_ARGUMENT;
    if(v->is_reference&&(n!=4||(!v->reference.known&&(v->word||v->known_mask))))return NBA97_BODY_ARGUMENT;
    if(v->known_mask&~((1u<<n)-1u)||(n!=4&&(v->word>>(n*8))))return NBA97_BODY_ARGUMENT;
    for(i=0;i<n;++i)if(!(v->known_mask&(1u<<i))&&(v->word&(255u<<(8*i))))return NBA97_BODY_ARGUMENT;
    if(kind==NBA97_FRAME_READ)++r->out->reads;else ++r->out->stores;return NBA97_BODY_OK;
}
static int value(Run* r,uint32_t pc,uint32_t a,unsigned n,Nba97PlayerFrameValue* v){memset(v,0,sizeof *v);return access(r,pc,a,n,NBA97_FRAME_READ,v);}
static int rd(Run* r,uint32_t pc,uint32_t a,unsigned n,uint32_t* word){
    Nba97PlayerFrameValue v;TRY(value(r,pc,a,n,&v));if(v.known_mask!=((1u<<n)-1u))return NBA97_BODY_UNKNOWN;*word=v.word;return NBA97_BODY_OK;
}
static int wr(Run* r,uint32_t pc,uint32_t a,unsigned n,uint32_t word,unsigned kind){
    Nba97PlayerFrameValue v={0};v.word=n==4?word:word&65535u;v.known_mask=(uint8_t)((1u<<n)-1u);return access(r,pc,a,n,kind,&v);
}
#define STORE(pc,a,n,v) TRY(wr(r,pc,a,n,v,NBA97_FRAME_WRITE))
#define POINTER(pc,a,v) TRY(wr(r,pc,a,4,v,NBA97_FRAME_WRITE_POINTER))
static int convert(Run* r,uint32_t src,uint32_t dst,uint32_t map,uint32_t count){
    uint32_t x,y,z,index;
    do{
        TRY(rd(r,0x80054fd0,src,2,&x));TRY(rd(r,0x80054fd4,src+2,2,&y));TRY(rd(r,0x80054fd8,src+4,2,&z));
        TRY(rd(r,0x80054fdc,map,4,&index));index=dst+(index<<3);
        STORE(0x80054ff0,index,2,0x800u-sx16(x));STORE(0x80054ff4,index+2,2,y);STORE(0x80054ff8,index+4,2,0x800u-sx16(z));
        map+=4;src+=8;--count;
    }while(s32(count)>0);
    return NBA97_BODY_OK;
}
static int blend(Run* r,uint32_t a,uint32_t b,uint32_t dst,uint16_t weight){
    uint32_t x,y,z;Nba97GameEuler first,second,result;
    TRY(rd(r,0x80055044,a,2,&x));TRY(rd(r,0x80055048,a+2,2,&y));TRY(rd(r,0x8005504c,a+4,2,&z));
    first.x=half(x);first.y=half(y);first.z=half(z);
    TRY(rd(r,0x80055050,b,2,&x));TRY(rd(r,0x80055054,b+2,2,&y));TRY(rd(r,0x80055058,b+4,2,&z));
    second.x=half(x);second.y=half(y);second.z=half(z);
    /* Existing native55018 arithmetic preserves asymmetric search, strict ties,
     * weight128 midpoint and low32 product. It has no memory side effects. */
    result=nba97_game_euler_blend(first,second,weight);
    STORE(0x80055334,dst,2,(uint32_t)(int32_t)result.x);STORE(0x80055338,dst+2,2,(uint32_t)(int32_t)result.y);STORE(0x8005533c,dst+4,2,(uint32_t)(int32_t)result.z);
    return NBA97_BODY_OK;
}
static int frame(Run* r){
    Nba97PlayerFrameValue initial,flags_value;
    uint32_t index,entity,base,clip,frame_index,primary,secondary,descriptor,flags,context,v,w,weight;
    uint32_t other_primary=0,other_secondary=0,i,p,destination;
    TRY(value(r,0x80053100,0x800f0ed8,4,&initial));
    STORE(0x8005313c,0x801029b0,4,0);
    /* This is an opaque source pointer copy; unknownness is propagated before
     * any later context dereference, not turned into a fabricated known NULL. */
    if(!initial.is_reference&&initial.known_mask!=15){
        /* A source LW creates one unknown register when any input byte is
         * unknown. Its following SW therefore makes the whole destination
         * word unknown; retaining three independently known bytes would claim
         * knowledge the original register does not carry. */
        memset(&initial,0,sizeof initial);
    }
    TRY(access(r,0x80053144,0x800f0ed4,4,
        initial.is_reference||initial.known_mask!=15?NBA97_FRAME_WRITE:NBA97_FRAME_WRITE_POINTER,&initial));
    do{
        TRY(rd(r,0x8005314c,0x801029b0,4,&index));TRY(rd(r,0x80053154,0x800fc654,4,&base));entity=base+index*244u;
        TRY(rd(r,0x80053170,entity+0x84,2,&clip));TRY(rd(r,0x8005317c,entity+0x8c,2,&frame_index));
        TRY(rd(r,0x80053188,0x8001ec98+(clip<<2),4,&descriptor));TRY(rd(r,0x80053190,entity+0x88,2,&clip));
        TRY(rd(r,0x800531a4,descriptor+8,4,&primary));primary+=frame_index*96u;
        TRY(rd(r,0x800531b0,0x800170c8+(clip<<2),4,&descriptor));TRY(rd(r,0x800531b4,entity+0x90,2,&frame_index));
        TRY(value(r,0x800531bc,entity+0x9a,2,&flags_value));
        if(!(flags_value.known_mask&1))return NBA97_BODY_UNKNOWN;
        flags=flags_value.word&15u;
        TRY(rd(r,0x800531c0,descriptor+8,4,&secondary));secondary+=frame_index*68u;
        TRY(rd(r,0x800531d8,entity+0x86,2,&clip));
        if(!(clip&0x8000u)){
            TRY(rd(r,0x800531fc,0x8001ec98+(clip<<2),4,&descriptor));TRY(rd(r,0x80053200,entity+0x8e,2,&frame_index));
            TRY(rd(r,0x80053204,descriptor+8,4,&other_primary));other_primary+=frame_index*96u;
        }
        TRY(rd(r,0x80053218,entity+0x8a,2,&clip));
        if(!(clip&0x8000u)){
            TRY(rd(r,0x80053234,0x800170c8+(clip<<2),4,&descriptor));TRY(rd(r,0x80053238,entity+0x92,2,&frame_index));
            TRY(rd(r,0x8005323c,descriptor+8,4,&other_secondary));other_secondary+=frame_index*68u;
        }
        TRY(rd(r,0x80053254,0x800f0ed4,4,&context));TRY(rd(r,0x80053258,entity+0x8e,2,&v));STORE(0x80053260,context+0x18,2,v);
        TRY(rd(r,0x80053264,entity+0x92,2,&v));STORE(0x8005326c,context+0x1a,2,v);
        if(flags&1u){
            TRY(convert(r,primary,0x800eb690+index*96u,0x800b79b0,12));++r->out->child_calls;
            TRY(rd(r,0x800532a0,0x801029b0,4,&index));primary=0x800eb690+index*96u;
        }
        if(flags&2u){
            TRY(rd(r,0x800532c8,0x801029b0,4,&index));TRY(convert(r,secondary+4,0x800dc804+index*68u,0x800b79e0,8));++r->out->child_calls;
            TRY(rd(r,0x800532f8,0x801029b0,4,&index));TRY(rd(r,0x800532fc,secondary+2,2,&v));secondary=0x800dc800+index*68u;
            STORE(0x80053320,secondary+2,2,v);
        }
        TRY(rd(r,0x80053324,entity+0x86,2,&clip));
        if(!(clip&0x8000u)){
            if(flags&4u){
                TRY(rd(r,0x80053340,0x801029b0,4,&index));TRY(convert(r,other_primary,0x800fac50+index*96u,0x800b79b0,12));++r->out->child_calls;
                TRY(rd(r,0x80053370,0x801029b0,4,&index));other_primary=0x800fac50+index*96u;
            }
            TRY(rd(r,0x80053390,0x801029b0,4,&index));TRY(rd(r,0x800533a4,0x800f0ed4,4,&context));
            POINTER(0x800533b8,context+0xbbc,0x800fa640+index*96u);p=other_primary;
            for(i=0;i<12;++i){
                TRY(rd(r,0x800533c4,entity+0x94,2,&weight));TRY(rd(r,0x800533cc,0x801029b0,4,&index));destination=0x800fa640+i*8u+index*96u;
                TRY(blend(r,primary,p,destination,(uint16_t)weight));++r->out->child_calls;primary+=8;p+=8;
            }
        }else{TRY(rd(r,0x80053408,0x800f0ed4,4,&context));POINTER(0x80053410,context+0xbbc,primary);}
        TRY(rd(r,0x80053414,entity+0x8a,2,&clip));
        if(!(clip&0x8000u)){
            if(flags&8u){
                TRY(rd(r,0x80053430,0x801029b0,4,&index));TRY(convert(r,other_secondary+4,0x800fa388+index*68u,0x800b79e0,8));++r->out->child_calls;
                TRY(rd(r,0x80053460,0x801029b0,4,&index));TRY(rd(r,0x80053464,other_secondary+2,2,&v));other_secondary=0x800fa384+index*68u;
                STORE(0x80053488,other_secondary+2,2,v);
            }
            TRY(rd(r,0x80053494,0x801029b0,4,&index));TRY(rd(r,0x800534ac,0x800f0ed4,4,&context));
            POINTER(0x800534c0,context+0xbc0,0x800f9d14+index*68u);
            for(i=0;i<8;++i){
                TRY(rd(r,0x800534cc,entity+0x96,2,&weight));TRY(rd(r,0x800534d4,0x801029b0,4,&index));
                TRY(blend(r,secondary+4+i*8u,other_secondary+4+i*8u,0x800f9d18+i*8u+index*68u,(uint16_t)weight));++r->out->child_calls;
            }
            TRY(rd(r,0x8005350c,0x801029b0,4,&index));TRY(rd(r,0x80053510,other_secondary+2,2,&v));TRY(rd(r,0x80053514,secondary+2,2,&w));TRY(rd(r,0x80053518,entity+0x96,2,&weight));
            STORE(0x80053544,0x800f9d16+index*68u,2,sx16(w)+asr8((sx16(v)-sx16(w))*weight));
        }else{TRY(rd(r,0x80053554,0x800f0ed4,4,&context));POINTER(0x8005355c,context+0xbc0,secondary);}
        TRY(rd(r,0x80053564,0x801029b0,4,&index));TRY(rd(r,0x8005356c,0x800f0ed4,4,&context));++index;context+=0xbcc;
        STORE(0x8005357c,0x801029b0,4,index);POINTER(0x80053588,0x800f0ed4,context);++r->out->actors;
        /* B pointers are carried across actors exactly as original s7/s6.
         * Aliased context writes may change the later B-present reread. */
    }while(s32(index)<10);
    return NBA97_BODY_OK;
}
static int begin(Run* r,Nba97PlayerFrameContext* in,Nba97PlayerFrameProgress* out){
    if(!out)return NBA97_BODY_ARGUMENT;
    memset(out,0,sizeof *out);
    if(!in||!in->access)return NBA97_BODY_ARGUMENT;
    r->in=in;r->out=out;return NBA97_BODY_OK;
}
static int finish(Run* r,int status){if(status==NBA97_BODY_OK){r->out->completed=1;r->out->stopped_pc=r->out->stopped_address=0;}return status;}
int nba97_game_pose_frame(Nba97PlayerFrameContext* in,Nba97PlayerFrameProgress* out){Run r;TRY(begin(&r,in,out));return finish(&r,frame(&r));}
int nba97_game_pose_convert(Nba97PlayerFrameContext* in,uint32_t src,uint32_t dst,uint32_t map,uint32_t count,Nba97PlayerFrameProgress* out){Run r;TRY(begin(&r,in,out));return finish(&r,convert(&r,src,dst,map,count));}
int nba97_game_pose_blend(Nba97PlayerFrameContext* in,uint32_t a,uint32_t b,uint32_t dst,uint16_t weight,Nba97PlayerFrameProgress* out){Run r;TRY(begin(&r,in,out));return finish(&r,blend(&r,a,b,dst,weight));}
