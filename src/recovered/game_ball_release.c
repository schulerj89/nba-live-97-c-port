#include "game_ball_release.h"
#include <string.h>
#define TRY(x) do { int result_=(x);if(result_!=1)return result_; } while(0)
static int32_t s32(uint32_t v){return v<0x80000000u?(int32_t)v:-1-(int32_t)(~v);}
static int32_t s16(uint32_t v){v&=65535;return v<32768?(int32_t)v:(int32_t)v-65536;}
static uint32_t sar(uint32_t v,unsigned n){return (v>>n)|((v&0x80000000u)?(UINT32_MAX<<(32-n)):0);}
static uint32_t mask(unsigned w){return w==4?UINT32_MAX:(1u<<(w*8))-1;}
static int access(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,
                  uint32_t a,unsigned w,int write,Nba97GamePeriodValue* v){
    int result;r->stopped_pc=pc;
    if(a&(w-1))return NBA97_TIPOFF_ALIGNMENT;
    result=c->access(c->user,pc,a,w,write,v);if(result!=1)return result;
    if(v->known>1||(v->known?(v->word&~mask(w)):(v->word!=0)))return NBA97_TIPOFF_ARGUMENT;
    if(write)++r->stores;
    return 1;
}
static int value(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,
                 uint32_t a,unsigned w,Nba97GamePeriodValue* v){v->word=0;v->known=0;return access(c,r,pc,a,w,0,v);}
static int require(Nba97GameTipoffReceipt* r,uint32_t pc,Nba97GamePeriodValue v,uint32_t* out){
    r->stopped_pc=pc;if(!v.known)return NBA97_TIPOFF_UNKNOWN;*out=v.word;return 1;
}
static int read(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,uint32_t a,unsigned w,uint32_t* out){
    Nba97GamePeriodValue v;TRY(value(c,r,pc,a,w,&v));return require(r,pc,v,out);
}
static int copy(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,uint32_t a,unsigned w,Nba97GamePeriodValue v){
    if(v.known)v.word&=mask(w);
    return access(c,r,pc,a,w,1,&v);
}
static int write(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,uint32_t a,unsigned w,uint32_t n){
    Nba97GamePeriodValue v={n&mask(w),1};return copy(c,r,pc,a,w,v);
}
static int divide(Nba97GameTipoffReceipt* r,uint32_t n,int32_t d,uint32_t zero_pc,uint32_t overflow_pc,uint32_t* out){
    if(!d){r->stopped_pc=zero_pc;return NBA97_BALL_RELEASE_DIVZERO;}
    if(n==0x80000000u&&d==-1){r->stopped_pc=overflow_pc;return NBA97_BALL_RELEASE_DIVOVERFLOW;}
    *out=(uint32_t)(s32(n)/d);return 1;
}
#define R(pc,a,w,v) TRY(read(c,r,pc,a,w,&(v)))
#define W(pc,a,w,v) TRY(write(c,r,pc,a,w,(uint32_t)(v)))
static int rng(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t* out){
    uint32_t v;R(0x8002ab78,0x8001edee,2,v);
    if(!v)W(0x8002ab88,0x8001edee,2,0xa5a5);
    R(0x8002ab8c,0x8001edee,2,v);
    /* Original bit14 test, shared seed and zero-seed two-store sequence. */
    v=((v<<1)^((v&0x4000)?0x1d87u:0))&65535;
    W(0x8002aba8,0x8001edee,2,v);*out=v;return 1;
}
int nba97_game_ball_release(Nba97GameTipoffContext* c,uint32_t thrower,uint32_t receiver,
                           Nba97GameTipoffReceipt* r){
    Nba97GamePeriodValue receiver_id,loose_mode,ball_reference,vertical;
    uint32_t mode,ball,table,row,kind,duration,vx,vz,x,z,px,pz,v,n,q,boost=0,option,accuracy,ptr,draw;
    int32_t ticks;
    if(!c||!c->access||!r)return NBA97_TIPOFF_ARGUMENT;
    memset(r,0,sizeof(*r));
    W(0x8005863c,0x800fdbd4,2,0);
    TRY(value(c,r,0x80058640,receiver,4,&receiver_id));
    TRY(value(c,r,0x80058648,0x800fe8cc,2,&loose_mode));
    TRY(value(c,r,0x80058650,0x800fdc48,4,&ball_reference));
    W(0x8005865c,0x800fdbd6,2,1);W(0x80058668,0x800fdbcc,2,65535);
    TRY(copy(c,r,0x80058670,0x800fdbd2,2,receiver_id));
    TRY(require(r,0x80058674,loose_mode,&mode));
    if(mode){row=3;table=0x800b8198;}
    else {
        W(0x80058688,thrower+0xb4,2,30);
        R(0x80058690,0x800fdc02,2,kind);R(0x80058698,0x800fdc00,2,row);
        if(!kind){table=0x800b81c8;boost=1;}
        else table=s16(kind)>0?0x800b81b0:0x800b8198;
    }
    table+=row*4u;
    TRY(copy(c,r,0x800586e8,0x800fdc34,4,ball_reference));
    R(0x800586ec,table,2,duration);ticks=s16(duration);
    R(0x800586f0,receiver+0x14,2,vx);R(0x800586fc,receiver+0x16,2,vz);
    R(0x80058714,receiver+8,4,x);R(0x80058718,receiver+0xc,4,z);
    /* MULT/MFLO and ADDU wrap BEFORE the signed court-limit comparisons. */
    px=x+(uint32_t)s16(vx)*(uint32_t)ticks;pz=z+(uint32_t)s16(vz)*(uint32_t)ticks;
    if(s32(px)>0x15800||s32(px)<-0x15800){
        px=s32(px)>0x15800?0x15800u:(uint32_t)-0x15800;
        R(0x80058760,receiver+8,4,v);
        TRY(divide(r,px-v,ticks,0x80058778,0x80058790,&q));W(0x8005879c,receiver+0x14,2,q);
    }
    if(s32(pz)>0xa800||s32(pz)<-0xa800){
        pz=s32(pz)>0xa800?0xa800u:(uint32_t)-0xa800;
        R(0x800587d0,receiver+0xc,4,v);
        TRY(divide(r,pz-v,ticks,0x800587e8,0x80058800,&q));W(0x8005880c,receiver+0x16,2,q);
    }
    /* The captured ball pointer is opaque until this first dereference, after
     * FDC34 and any receiver-velocity stores. Unknown is not a zero pointer. */
    TRY(require(r,0x80058810,ball_reference,&ball));R(0x80058810,ball+8,4,v);
    TRY(divide(r,px-v,ticks,0x80058828,0x80058840,&vx));
    R(0x80058848,ball+0xc,4,v);TRY(divide(r,pz-v,ticks,0x80058860,0x80058878,&vz));
    if(boost){vx+=sar(vx,2);vz+=sar(vz,2);}
    R(0x80058894,receiver+0x1a,1,v);if(v!=15)W(0x800588a4,receiver+0xb6,2,(uint32_t)ticks+4u);
    R(0x800588ac,0x800fe8cc,2,v);
    if(!v){
        R(0x800588c0,0x80021d72,1,option);
        if(option){
            R(0x800588d4,0x800fdc00,2,kind);
            if(kind>=2){
                if(option>=2&&kind>=5){TRY(rng(c,r,&draw));accuracy=draw&0x18;}
                else {
                    R(0x8005890c,0x80021d93,1,v);n=32;
                    if(v){R(0x8005891c,thrower+0x1c,4,ptr);R(0x80058924,ptr+0x20,2,v);n=sar(v<<16,26);}
                    R(0x80058934,thrower+0x20,4,ptr);R(0x8005893c,ptr+0x1a,1,v);
                    n=v+n-((uint32_t)ticks-90u);TRY(rng(c,r,&draw));accuracy=(uint32_t)((int32_t)(draw&255)<s32(n));
                }
                if(!accuracy){
                    R(0x80058964,0x800fdb90,2,v);
                    /* Original skips the error only for phase82. A phase81
                     * tip can still consume RNG and receive this perturbation. */
                    if(v!=0x82){
                        R(0x80058978,0x80021d72,1,option);n=1;
                        if(option>=2){R(0x8005898c,thrower+4,2,v);if(s16(v)<0){TRY(rng(c,r,&draw));n=(draw&3)==0;}}
                        if(n){TRY(rng(c,r,&draw));vx=vx-64u+(draw&127);TRY(rng(c,r,&draw));vz=vz-64u+(draw&127);}
                    }
                }
            }
        }
    }
    W(0x800589d4,ball+0x14,2,vx);W(0x800589d8,ball+0x16,2,vz);
    /* Read vertical-table value AFTER both ball stores; a genuine alias may
     * change it. Nonnegative entries produce a SECOND vertical store. */
    TRY(value(c,r,0x800589dc,table+2,2,&vertical));TRY(copy(c,r,0x800589e4,ball+0x18,2,vertical));
    TRY(require(r,0x800589e8,vertical,&v));
    if(s16(v)>=0){
        R(0x800589f8,0x800fdc28,4,n);R(0x800589fc,ball+0x10,4,v);
        TRY(divide(r,(n<<8)-v,ticks,0x80058a14,0x80058a2c,&q));
        W(0x80058a44,ball+0x18,2,q+(uint32_t)ticks*12u);
    }
    R(0x80058a4c,0x800fe8cc,2,v);
    if(!v){R(0x80058a60,0x800fdb90,2,v);if(s16(v)<128)W(0x80058a78,0x800fdb90,2,0);}
    r->completed=1;r->stopped_pc=0;return 1;
}
