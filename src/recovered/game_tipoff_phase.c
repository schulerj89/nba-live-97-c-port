#include "game_tipoff_phase.h"
#include <string.h>

#define TRY(x) do { int result_=(x); if(result_!=NBA97_TIPOFF_OK)return result_; } while(0)
static int32_t signed32(uint32_t v){return v<0x80000000u?(int32_t)v:-1-(int32_t)(~v);}
static int32_t signed16(uint32_t v){v&=65535;return v<32768?(int32_t)v:(int32_t)v-65536;}
static uint32_t mask(unsigned width){return width==4?UINT32_MAX:(1u<<(width*8))-1;}
static int valid(Nba97GamePeriodValue v,unsigned width){
    return v.known<=1 && (v.known?!(v.word&~mask(width)):v.word==0);
}
static int access_value(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,
                        uint32_t pc,uint32_t a,unsigned width,int write,Nba97GamePeriodValue* v){
    int status;r->stopped_pc=pc;
    if(a&(width-1))return NBA97_TIPOFF_ALIGNMENT;
    if(write&&!valid(*v,width))return NBA97_TIPOFF_ARGUMENT;
    status=c->access(c->user,pc,a,width,write,v);
    if(status!=NBA97_TIPOFF_OK)return status;
    if(!valid(*v,width))return NBA97_TIPOFF_ARGUMENT;
    if(write)++r->stores;
    return NBA97_TIPOFF_OK;
}
static int read_value(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,
                      uint32_t a,unsigned width,Nba97GamePeriodValue* v){
    v->word=0;v->known=0;return access_value(c,r,pc,a,width,0,v);
}
static int read_word(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,
                     uint32_t a,unsigned width,uint32_t* out){
    Nba97GamePeriodValue v;TRY(read_value(c,r,pc,a,width,&v));
    if(!v.known)return NBA97_TIPOFF_UNKNOWN;
    *out=v.word;return NBA97_TIPOFF_OK;
}
static int require_value(Nba97GameTipoffReceipt* r,uint32_t pc,Nba97GamePeriodValue v,uint32_t* out){
    r->stopped_pc=pc;if(!v.known)return NBA97_TIPOFF_UNKNOWN;
    *out=v.word;return NBA97_TIPOFF_OK;
}
static int write_value(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,
                       uint32_t a,unsigned width,Nba97GamePeriodValue v){
    if(v.known)v.word&=mask(width);
    return access_value(c,r,pc,a,width,1,&v);
}
static int write_word(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,
                      uint32_t a,unsigned width,uint32_t word){
    Nba97GamePeriodValue v={word&mask(width),1};return write_value(c,r,pc,a,width,v);
}
static int invoke(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r,uint32_t pc,
                  uint32_t owner,unsigned count,uint32_t a0,uint32_t a1,int consumes,uint32_t* out){
    Nba97GameTipoffCall call;Nba97GamePeriodValue v={0,0};int status;
    call.pc=pc;call.owner=owner;call.count=count;call.argument[0]=a0;call.argument[1]=a1;
    r->stopped_pc=pc;++r->calls;
    if(!c->call)return NBA97_TIPOFF_PENDING;
    status=c->call(c->user,&call,&v);
    if(status!=1)return status==0?NBA97_TIPOFF_PENDING:NBA97_TIPOFF_CALLBACK_FAILED;
    if(consumes){if(!valid(v,4))return NBA97_TIPOFF_ARGUMENT;if(!v.known)return NBA97_TIPOFF_UNKNOWN;*out=v.word;}
    return NBA97_TIPOFF_OK;
}
static int begin(Nba97GameTipoffContext* c,Nba97GameTipoffReceipt* r){
    if(!c||!c->access||!r)return NBA97_TIPOFF_ARGUMENT;
    memset(r,0,sizeof(*r));return NBA97_TIPOFF_OK;
}
static int finish(Nba97GameTipoffReceipt* r,uint32_t value,int known){
    r->completed=1;r->stopped_pc=0;r->return_v0=value;r->return_known=(uint8_t)known;return NBA97_TIPOFF_OK;
}
#define R(pc,a,w,v) TRY(read_word(c,r,pc,a,w,&(v)))
#define W(pc,a,w,v) TRY(write_word(c,r,pc,a,w,(uint32_t)(v)))
static int hand(Nba97GameTipoffContext* c,uint32_t entity,uint32_t which,
                Nba97GamePeriodValue out[3],Nba97GameTipoffReceipt* r){
    uint32_t flags,id,p,v,height;
    R(0x8002d37c,entity+0x9a,2,flags);
    R(which==(flags&1)?0x8002d3a4:0x8002d390,entity,4,id);
    p=(which==(flags&1)?0x800fed20u:0x800faa04u)+(id<<3);
    R(0x8002d3b8,p,2,v);out[0].word=(uint32_t)signed16(v)<<5;out[0].known=1;
    R(0x8002d3c8,p+4,2,v);out[1].word=(uint32_t)signed16(v)<<5;out[1].known=1;
    R(0x8002d3d8,p+2,2,v);R(0x8002d3dc,entity+0x10,4,height);
    out[2].word=((uint32_t)signed16(v)<<5)+height;out[2].known=1;
    return NBA97_TIPOFF_OK;
}
int nba97_game_tipoff_hand(Nba97GameTipoffContext* c,uint32_t entity,uint32_t which,
                          Nba97GamePeriodValue out[3],Nba97GameTipoffReceipt* r){
    if(!out)return NBA97_TIPOFF_ARGUMENT;
    TRY(begin(c,r));TRY(hand(c,entity,which,out,r));return finish(r,0,0);
}
static int hand_contact(Nba97GameTipoffContext* c,uint32_t ball,uint32_t entity,
                        const uint32_t xyz[3],uint32_t mode,Nba97GameTipoffReceipt* r){
    uint32_t bh,bx,ez,bz,ex,x,z,h,result=0;int32_t limit=mode?0x8c0:0x700;
    R(0x80060100,ball+0x10,4,bh);R(0x80060108,ball+8,4,bx);
    R(0x8006010c,entity+0xc,4,ez);R(0x80060110,ball+0xc,4,bz);R(0x80060118,entity+8,4,ex);
    x=xyz[0]+(ex-bx);z=xyz[1]+(ez-bz);h=xyz[2]-bh;
    /*600F0's signed, asymmetric height window is intentional; SUBU/ADDU wrap. */
    if(signed32(x)<=limit&&signed32(z)<=limit&&signed32(h)<0xa81&&
       signed32(x)>=-limit&&signed32(z)>=-limit&&signed32(h)>=-limit){
        if(mode)result=3;
        else {TRY(invoke(c,r,0x80060188,0x8005fc88,1,entity,0,1,&result));result=(uint32_t)signed16(result);}
    }
    r->return_v0=result;r->return_known=1;return NBA97_TIPOFF_OK;
}
int nba97_game_tipoff_hand_contact(Nba97GameTipoffContext* c,uint32_t ball,uint32_t entity,
                                  const uint32_t xyz[3],uint32_t mode,Nba97GameTipoffReceipt* r){
    if(!xyz)return NBA97_TIPOFF_ARGUMENT;
    TRY(begin(c,r));TRY(hand_contact(c,ball,entity,xyz,mode,r));return finish(r,r->return_v0,1);
}
int nba97_game_tipoff_contact(Nba97GameTipoffContext* c,uint32_t ball,uint32_t entity,
                             unsigned which,uint32_t mode,Nba97GameTipoffReceipt* r){
    Nba97GamePeriodValue point[3];uint32_t xyz[3];unsigned i;
    if(which>1)return NBA97_TIPOFF_ARGUMENT;
    TRY(begin(c,r));
    W(which?0x80060274:0x800601f0,0x800fdc30,2,which);
    TRY(hand(c,entity,which,point,r));for(i=0;i<3;++i)xyz[i]=point[i].word;
    TRY(hand_contact(c,ball,entity,xyz,mode,r));return finish(r,r->return_v0,1);
}
int nba97_game_tipoff_body_contact(Nba97GameTipoffContext* c,uint32_t ball,uint32_t entity,
                                  uint32_t height,uint32_t distance,uint32_t mode,
                                  Nba97GameTipoffReceipt* r){
    uint32_t v,id,target,result=0;int32_t h=signed16(height),d=signed16(distance),m=signed16(mode);
    (void)ball;TRY(begin(c,r));R(0x80060014,0x800fe8aa,2,v);
    if(v){R(0x8006002c,0x800fdbd4,2,v);if(!v){R(0x8006003c,entity+0x10,4,v);if(!v){m=2;d=d>=0?d/2:-1-((-1-d)/2);}}}
    R(0x80060058,0x800fdbd2,2,target);R(0x8006005c,entity,4,id);
    if(h<(id==(uint32_t)signed16(target)?0x41:0x39)){
        W(0x8006008c,0x800fdc30,2,0);
        if(!m){if(d<8&&h>=0x1c){TRY(invoke(c,r,0x800600d0,0x8005fc88,1,entity,0,1,&result));result=(uint32_t)signed16(result);}}
        else if(d<9)result=2;
    }
    return finish(r,result,1);
}
static int release(Nba97GameTipoffContext* c,uint32_t entity,Nba97GameTipoffReceipt* r){
    uint32_t rng,side,receiver;Nba97GamePeriodValue receiver_value,id;
    R(0x8002ab78,0x8001edee,2,rng);
    if(!rng)W(0x8002ab88,0x8001edee,2,0xa5a5);
    R(0x8002ab8c,0x8001edee,2,rng);
    /*2AB70 tests bit14, and zero seed produces TWO stores to shared1EDEE. */
    rng=((rng<<1)^((rng&0x4000)?0x1d87u:0))&65535;
    W(0x8002aba8,0x8001edee,2,rng);
    R(0x8005bc48,entity+0xd9,1,side);
    /* Original5BC54 adds unsignedside+3, NOT side*5+3. Preserve this indexing
     * quirk even when the selected record belongs to the opposing lineup. */
    TRY(read_value(c,r,0x8005bc68,0x80020becu+(((rng&8)?1u:0u)+side+3u)*4u,4,&receiver_value));
    W(0x8005bc74,0x800fdc02,2,65535);W(0x8005bc80,0x800fdc00,2,2);
    TRY(read_value(c,r,0x8005bc84,entity,4,&id));TRY(write_value(c,r,0x8005bc8c,0x800fdbce,2,id));
    TRY(require_value(r,0x8005bc94,receiver_value,&receiver));
    W(0x8005bc94,receiver+0x1a,1,7);W(0x8005bca0,0x800fdc28,4,32);
    return invoke(c,r,0x8005bca4,0x80058610,2,entity,receiver,0,0);
}
int nba97_game_tipoff_release(Nba97GameTipoffContext* c,uint32_t entity,Nba97GameTipoffReceipt* r){
    TRY(begin(c,r));TRY(release(c,entity,r));return finish(r,0,0);
}
int nba97_game_tipoff_after_acquire(Nba97GameTipoffContext* c,uint32_t entity,uint32_t ball,
                                   uint32_t previous_team,uint32_t acquired_v0,
                                   Nba97GameTipoffReceipt* r){
    uint32_t target,phase,v,first,second;Nba97GamePeriodValue saved={previous_team,1},claim,loaded_target,first_value,second_value;
    TRY(begin(c,r));TRY(read_value(c,r,0x800608a8,0x800fdbd2,2,&loaded_target));W(0x800608b8,0x800fdb88,2,1);
    TRY(require_value(r,0x800608bc,loaded_target,&target));
    if(signed16(target)<0){
        R(0x800608c8,0x800fdb90,2,phase);
        if(phase==0x81){
            TRY(read_value(c,r,0x8006092c,0x80020bec,4,&first_value));
            TRY(read_value(c,r,0x80060934,0x80020c00,4,&second_value));
            W(0x80060940,0x800fe880,2,acquired_v0);W(0x80060948,0x800fdb96,2,acquired_v0);
            W(0x80060950,0x800fdb72,2,acquired_v0);
            /*60958..60964 deliberately repeat both stores; aliases and prefix
             * observations must see all FOUR original writes. */
            TRY(require_value(r,0x80060958,second_value,&second));
            W(0x80060958,second+0xb4,2,30);
            TRY(require_value(r,0x8006095c,first_value,&first));W(0x8006095c,first+0xb4,2,30);
            W(0x80060960,second+0xb4,2,30);W(0x80060964,first+0xb4,2,30);
            TRY(read_value(c,r,0x80060968,entity+4,2,&claim));
            TRY(write_value(c,r,0x80060970,0x800fdbd0,2,claim));
            TRY(release(c,entity,r));
            TRY(invoke(c,r,0x80060988,0x80029590,1,acquired_v0?10:12,0,0,0));
            R(0x80060994,0x80020bec,4,first);R(0x8006099c,first+0x46,2,v);
            if(v==0x27){R(0x800609b0,0x80020bec,4,first);TRY(invoke(c,r,0x800609b4,0x800582dc,2,first,1,0,0));}
            R(0x800609c0,0x80020c00,4,second);R(0x800609c8,second+0x46,2,v);
            if(v==0x27){R(0x800609dc,0x80020c00,4,second);TRY(invoke(c,r,0x800609e0,0x800582dc,2,second,1,0,0));}
            /*609EC reloads the current team, overriding incomingS3. The final
             * store is retained; tipoff does NOT restore the earlier team. */
            TRY(read_value(c,r,0x800609ec,0x800fdc40,4,&saved));
            W(0x800609f8,0x800fdb90,2,0x82);W(0x80060a04,0x800fe884,2,3);
            r->tip_transition=1;goto team_store;
        }
        if(phase!=0x82)goto clear_phase;
        R(0x800608e4,0x800fe884,2,v);if(!v)goto reset;
    }
    R(0x800608f8,0x800fdb90,2,phase);
    if(phase==0x82)W(0x8006090c,0x800fe86e,2,3);
clear_phase:
    W(0x80060914,0x800fdb90,2,0);
reset:
    TRY(invoke(c,r,0x80060918,0x80058260,0,0,0,0,0));
    W(0x80060924,ball+0x18,2,0);
team_store:
    TRY(write_value(c,r,0x80060cbc,0x800fdc40,4,saved));return finish(r,0,0);
}
