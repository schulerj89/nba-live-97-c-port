#include "game_ball_simulation.h"
#include "game_ball_attachment.h"
#include "game_controller_selection.h"
#include <string.h>
typedef struct Run {Nba97BallSimulationContext* in;Nba97BallSimulationProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_BODY_OK)return status_;}while(0)
static int32_t s32(uint32_t v){return v<0x80000000u?(int32_t)v:-1-(int32_t)~v;}
static uint32_t sx16(uint32_t v){return v&0x8000u?(v&65535u)|0xffff0000u:v&65535u;}
static uint32_t shr(uint32_t v,unsigned n){return v&0x80000000u?(v>>n)|(~0u<<(32-n)):v>>n;}
static uint32_t bits(unsigned n){return n==4?UINT32_MAX:(1u<<(8*n))-1;}
static unsigned knowledge(unsigned n){return (1u<<n)-1;}
static int reserve(Run* r,uint32_t pc,uint32_t address){
    r->out->stopped_pc=pc;r->out->stopped_address=address;
    if(r->out->operations>=r->in->operation_budget)return NBA97_BODY_JOURNAL_LIMIT;
    ++r->out->operations;return NBA97_BODY_OK;
}
static int access(Run* r,uint32_t pc,uint32_t address,unsigned width,unsigned kind,Nba97PlayerFrameValue* v){
    int status;unsigned i;
    TRY(reserve(r,pc,address));

    if((width==4&&(address&3))||(width==2&&(address&1)))return NBA97_BODY_ALIGNMENT_TRAP;
    status=r->in->access(r->in->user,pc,address,width,kind,v);if(status!=NBA97_BODY_OK)return status;
    if(v->is_reference>1||v->reference.known>1||(!v->reference.known&&(v->reference.allocation||v->reference.offset)))return NBA97_BODY_ARGUMENT;
    if(!v->is_reference&&v->reference.known)return NBA97_BODY_ARGUMENT;
    if(v->is_reference&&(width!=4||(!v->reference.known&&(v->known_mask||v->word))))return NBA97_BODY_ARGUMENT;
    if(v->known_mask&~knowledge(width))return NBA97_BODY_ARGUMENT;
    if(v->word&~bits(width))return NBA97_BODY_ARGUMENT;
    for(i=0;i<width;++i)if(!(v->known_mask&(1u<<i))&&(v->word&(255u<<(8*i))))return NBA97_BODY_ARGUMENT;
    if(kind==NBA97_FRAME_READ)++r->out->reads;else ++r->out->stores;
    return NBA97_BODY_OK;
}
static int rd(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t* word){
    Nba97PlayerFrameValue v={0};TRY(access(r,pc,address,width,NBA97_FRAME_READ,&v));
    if(v.known_mask!=knowledge(width))return NBA97_BODY_UNKNOWN;
    *word=v.word;return NBA97_BODY_OK;
}
static int wr(Run* r,uint32_t pc,uint32_t address,unsigned width,uint32_t word){
    Nba97PlayerFrameValue v={0};v.word=word&bits(width);v.known_mask=(uint8_t)knowledge(width);
    return access(r,pc,address,width,NBA97_FRAME_WRITE,&v);
}

static int value(Run* r,uint32_t pc,uint32_t a,unsigned n,Nba97PlayerFrameValue* v){memset(v,0,sizeof *v);return access(r,pc,a,n,NBA97_FRAME_READ,v);}
static int copy(Run* r,uint32_t readpc,uint32_t writepc,uint32_t src,uint32_t dst,unsigned n){Nba97PlayerFrameValue v;TRY(value(r,readpc,src,n,&v));return access(r,writepc,dst,n,NBA97_FRAME_WRITE,&v);}
static int service(Run* r,uint32_t pc,uint32_t entry,unsigned count,uint32_t a,uint32_t b,unsigned return_bytes,uint32_t* word){
 Nba97BallSimulationCall call;Nba97PlayerFrameValue v={0};int rc;unsigned i;
 TRY(reserve(r,pc,entry));if(!r->in->service)return NBA97_BALL_SIMULATION_SERVICE_REQUIRED;
 call.pc=pc;call.entry=entry;call.count=count;call.argument[0]=a;call.argument[1]=b;
 rc=r->in->service(r->in->user,&call,&v);if(rc!=NBA97_BODY_OK)return rc;++r->out->services;
 if(return_bytes){if(v.is_reference||v.reference.known||v.reference.allocation||v.reference.offset||v.known_mask>15)return NBA97_BODY_ARGUMENT;
  for(i=0;i<4;++i)if(!(v.known_mask&(1u<<i))&&(v.word&(255u<<(8*i))))return NBA97_BODY_ARGUMENT;
  if((v.known_mask&knowledge(return_bytes))!=knowledge(return_bytes))return NBA97_BODY_UNKNOWN;
  *word=v.word&bits(return_bytes);}
 return NBA97_BODY_OK;
}
#define CALL(pc,entry,n,a,b) TRY(service(r,pc,entry,n,a,b,0,0))
static int child_access(void* u,uint32_t pc,uint32_t a,unsigned n,unsigned kind,Nba97PlayerFrameValue* v){return access((Run*)u,pc,a,n,kind,v);}
static int attachment(Run* r,uint32_t pc,uint32_t entry,uint32_t* word){
 Nba97PlayerFrameContext c;Nba97PlayerFrameProgress p;Nba97GamePeriodValue v;int rc;
 TRY(reserve(r,pc,entry));memset(&c,0,sizeof c);c.access=child_access;c.user=r;c.operation_budget=SIZE_MAX;
 rc=nba97_game_ball_attachment(&c,entry,&v,&p);if(rc!=NBA97_BODY_OK){r->out->stopped_pc=p.stopped_pc;r->out->stopped_address=p.stopped_address;return rc;}
 ++r->out->attachments;if(word){if(v.known>1||(!v.known&&v.word))return NBA97_BODY_ARGUMENT;if(!v.known)return NBA97_BODY_UNKNOWN;*word=v.word;}return NBA97_BODY_OK;
}
static uint32_t distance(uint32_t x,uint32_t z){return (uint32_t)nba97_game_selection_distance(s32(x),s32(z));}
static int divide(Run* r,uint32_t a,uint32_t b,uint32_t zero_pc,uint32_t overflow_pc,uint32_t* out){
 if(!b||(a==0x80000000u&&b==UINT32_MAX)){r->out->stopped_pc=b?overflow_pc:zero_pc;r->out->stopped_address=0;return NBA97_FRAME_ARITHMETIC_TRAP;}
 *out=(uint32_t)(s32(a)/s32(b));return NBA97_BODY_OK;
}
static int rng(Run* r,uint32_t* out){uint32_t v;TRY(rd(r,0x8002ab78,0x8001edee,2,&v));if(!v)TRY(wr(r,0x8002ab88,0x8001edee,2,0xa5a5));TRY(rd(r,0x8002ab8c,0x8001edee,2,&v));v=(v<<1)^((v&0x4000)?0x1d87:0);TRY(wr(r,0x8002aba8,0x8001edee,2,v));*out=v&65535;return NBA97_BODY_OK;}
static int score(Run* r,uint32_t kind,uint32_t player){uint32_t v,t;TRY(rd(r,0x800622a4,0x800fe8bc,2,&v));if(v)return NBA97_BODY_OK;
 TRY(rd(r,0x800622b8,0x800fdb94,2,&t));TRY(wr(r,0x800622bc,0x800fe8bc,2,kind));TRY(wr(r,0x800622c4,0x800fe8c8,2,player));
 if(t){TRY(rd(r,0x800622d8,0x8001eec4+(player<<1),2,&v));v+=12;}else TRY(rd(r,0x800622ec,0x8001ee0a+(player<<1),2,&v));return wr(r,0x800622f4,0x800fe8f6,2,v);
}
static int select_score(Run* r,uint32_t kind,int other){uint32_t p,v;uint32_t d=other?0:0x58;
 TRY(rd(r,0x80062304+d,0x800fdbcc,2,&p));p=sx16(p);if(s32(p)<0){TRY(rd(r,0x80062318+d,other?0x800fdb94:0x800fdb96,2,&v));if(v)TRY(rd(r,0x8006233c+d,0x8001ef0a,2,&p));else TRY(rd(r,0x8006232c+d,0x8001ee46,2,&p));p=sx16(p);}return score(r,kind,p);
}
static int reset_bounds(Run* r){uint32_t p,v;CALL(0x8006f82c,0x800626a0,0,0,0);TRY(rd(r,0x8006f838,0x800fdbcc,2,&v));if(s32(sx16(v))<0)CALL(0x8006f848,0x800623b0,1,1,0);
 TRY(rd(r,0x8006f854,0x800fdc48,4,&p));TRY(rd(r,0x8006f85c,p+0xc2,2,&v));TRY(wr(r,0x8006f878,0x800fe882,2,(v&2)?1:3));return wr(r,0x8006f880,0x800fe884,2,0);
}
static int rule_gate(Run* r,uint32_t* out){uint32_t v,p,random;*out=0;TRY(rd(r,0x80062d88,0x800fdbcc,2,&v));if(s32(sx16(v))<0)return NBA97_BODY_OK;
 TRY(rd(r,0x80062da0,0x800fdc34,4,&p));TRY(rd(r,0x80062da8,p+0xe6,2,&v));if(!v)return NBA97_BODY_OK;TRY(rd(r,0x80062db8,p+0xdc,1,&v));if(v&128)return NBA97_BODY_OK;
 TRY(rng(r,&random));TRY(rd(r,0x80062dd4,0x80021d87,1,&v));if(((random&0x78)>>3)>=v)return NBA97_BODY_OK;TRY(rng(r,&random));if((random&4095)>=0xb33)return NBA97_BODY_OK;
 TRY(rd(r,0x80062e04,p+0xdc,1,&v));v=v&128?v|0xffffff00u:v;TRY(rd(r,0x80062e18,0x80020bec+(v<<2),4,&v));TRY(wr(r,0x80062e24,0x800fe8bc,2,1));TRY(wr(r,0x80062e30,0x800fe882,2,3));TRY(rd(r,0x80062e34,v,4,&v));CALL(0x80062e38,0x800628fc,2,v,p);*out=1;return NBA97_BODY_OK;
}
static int boundary(Run* r){uint32_t v;TRY(rd(r,0x8006ecdc,0x80021d8a,1,&v));if(!v)return NBA97_BODY_OK;TRY(rd(r,0x8006ecf0,0x800fdb90,2,&v));if(s32(sx16(v))>=128)return NBA97_BODY_OK;
 TRY(rd(r,0x8006ed08,0x800fdbcc,2,&v));if(s32(sx16(v))>=0)return NBA97_BODY_OK;TRY(rd(r,0x8006ed1c,0x800fe8c4,2,&v));if(v)return NBA97_BODY_OK;TRY(rd(r,0x8006ed30,0x800fe8cc,2,&v));if(v)return NBA97_BODY_OK;TRY(rd(r,0x8006ed44,0x800fdb58,4,&v));if(!v)return NBA97_BODY_OK;
 TRY(rd(r,0x8006ed58,0x800fdb96,2,&v));if(v){CALL(0x8006ed78,0x80029590,1,12,0);v=20000;}else{CALL(0x8006ed68,0x80029590,1,11,0);v=5000;}CALL(0x8006ed84,0x800295c8,1,v,0);TRY(select_score(r,4,0));return reset_bounds(r);
}
static int held_boundary(Run* r){uint32_t v,phase,p,ball;
 TRY(rd(r,0x8006edb0,0x800fe8cc,2,&v));if(v)return NBA97_BODY_OK;TRY(rd(r,0x8006edc8,0x800fe8c4,2,&v));if(v)return NBA97_BODY_OK;TRY(rd(r,0x8006eddc,0x800fdb90,2,&phase));phase=sx16(phase);if(s32(phase)>=128)return NBA97_BODY_OK;
 TRY(rd(r,0x8006edf4,0x80021d8a,1,&v));if(!v)return NBA97_BODY_OK;TRY(rd(r,0x8006ee08,0x800fdc34,4,&p));TRY(rd(r,0x8006ee10,p+0x10,4,&v));if(v)return NBA97_BODY_OK;TRY(rd(r,0x8006ee20,p+0xc2,2,&v));if(!v)return NBA97_BODY_OK;
 /*6EDE8's earlier signed phase<128 gate makes the later phase82 branch unreachable. Retain the source gate, do not admit82 here. */
 TRY(rd(r,0x8006ee88,0x800fe8c4,2,&v));if(v)return NBA97_BODY_OK;TRY(rd(r,0x8006ee9c,0x800fe8cc,2,&v));if(v)return NBA97_BODY_OK;TRY(rule_gate(r,&v));if(v)return NBA97_BODY_OK;
 TRY(rd(r,0x8006eec0,0x800fdc48,4,&ball));TRY(copy(r,0x8006eec4,0x8006eecc,p+0xc2,ball+0xc2,2));TRY(rd(r,0x8006eed4,0x800fdb94,2,&v));if(v){CALL(0x8006eef4,0x80029590,1,12,0);v=20000;}else{CALL(0x8006eee4,0x80029590,1,11,0);v=5000;}CALL(0x8006ef00,0x800295c8,1,v,0);
 TRY(rd(r,0x8006ef08,p,4,&v));TRY(score(r,4,v));TRY(rd(r,0x8006ef14,p+0xd9,1,&v));TRY(wr(r,0x8006ef1c,0x800fdb96,2,v));TRY(reset_bounds(r));TRY(rd(r,0x8006ef28,p+0x46,2,&v));if(v==0x32){TRY(wr(r,0x8006ef38,p+0x4e,2,0));CALL(0x8006ef3c,0x80056ffc,2,p,1);CALL(0x8006ef44,0x8005703c,1,p,0);}return NBA97_BODY_OK;
}
static int backboard(Run*,uint32_t,uint32_t*);
static int rim(Run*,uint32_t,uint32_t*);

static int simulate(Run* r,uint32_t ball){
 uint32_t v,p,x,z,phase,held,vertical,height,remaining,zone,flag,q,previous;
 Nba97PlayerFrameValue old[3];unsigned i;
 TRY(rd(r,0x8006ef64,0x800fe8c0,2,&v));if(v){TRY(rd(r,0x8006ef90,0x800fdb90,2,&v));if(v==1){TRY(rd(r,0x8006efa4,0x800fe8c4,2,&v));TRY(wr(r,0x8006efb4,0x800fe8c4,2,v|2));}}
 TRY(rd(r,0x8006efc0,0x800fdbd6,2,&v));if(v){v=sx16(v);TRY(rd(r,0x8006efd4,0x800fdb6c,2,&q));TRY(wr(r,0x8006efe0,0x800fdbd6,2,v+sx16(q)));}
 TRY(rd(r,0x8006efe8,0x800fdb90,2,&phase));TRY(rd(r,0x8006eff0,0x800fdb6c,2,&remaining));remaining=sx16(remaining)-1;
 if(phase==2){TRY(rd(r,0x8006f004,0x800fdc34,4,&p));TRY(copy(r,0x8006f00c,0x8006f014,p+0x18,ball+0x18,2));TRY(copy(r,0x8006f018,0x8006f020,p+0x14,ball+0x14,2));TRY(copy(r,0x8006f024,0x8006f030,p+0x16,ball+0x16,2));remaining=0;TRY(held_boundary(r));goto speed;}
 /* The original executes at least one substep even for zero/negative FDB6C.
  * Bounce/attachment can set remaining=0, discarding further admitted ticks. */
 again:
 ++r->out->substeps;
 for(i=0;i<3;++i)TRY(value(r,0x8006f03c+i*4,ball+8+i*4,4,&old[i]));
 for(i=0;i<3;++i)TRY(access(r,0x8006f048+i*4,ball+0x24+i*4,4,NBA97_FRAME_WRITE,&old[i]));
 TRY(rd(r,0x8006f058,0x800fdbcc,2,&held));held=sx16(held);TRY(rd(r,0x8006f05c,ball+0x18,2,&vertical));vertical=sx16(vertical);
 if(s32(held)>=0){
  TRY(rd(r,0x8006f06c,0x800fdc34,4,&p));TRY(copy(r,0x8006f074,0x8006f07c,p+0x14,ball+0x14,2));TRY(copy(r,0x8006f080,0x8006f088,p+0x16,ball+0x16,2));TRY(rd(r,0x8006f08c,p+0x10,4,&v));
  if(v){TRY(copy(r,0x8006f0b8,0x8006f0c0,p+0x18,ball+0x18,2));goto held_attach;}
  TRY(rd(r,0x8006f09c,p+0x50,2,&v));if(v<16){TRY(wr(r,0x8006f0b4,ball+0x18,2,0xfffffe00));goto held_attach;}
 }
 TRY(rd(r,0x8006f0dc,ball+0x10,4,&height));TRY(rd(r,0x8006f0e8,0x800fdbd4,2,&v));vertical-=24;height+=vertical;
 /* Height uses the full wrapped ADD before vertical velocity is narrowed back
  * into the halfword. Do not clamp velocity or replace this with float physics. */
 if(!v){TRY(rd(r,0x8006f0fc,0x800fdb90,2,&v));if(v!=1)goto falling_done;}
 if(s32(height)<0x4800&&s32(vertical)<0){TRY(rd(r,0x8006f124,0x800fdb90,2,&v));TRY(wr(r,0x8006f128,0x800fdbd4,2,0));TRY(wr(r,0x8006f130,0x800fdbd6,2,0));if(s32(sx16(v))<128)TRY(wr(r,0x8006f144,0x800fdb90,2,0));}
 falling_done:
 TRY(rd(r,0x8006f14c,0x800fe8c4,2,&v));if(v)goto ground;TRY(rd(r,0x8006f160,0x800fe8cc,2,&v));if(v)goto ground;TRY(rd(r,0x8006f174,0x80021d8a,1,&v));if(!v)goto ground;
 if(s32(height)<0x401){TRY(rd(r,0x8006f190,0x800fdc48,4,&p));TRY(rd(r,0x8006f198,p+0xc2,2,&v));if(v)goto boundary_phase;}
 TRY(rd(r,0x8006f1ac,0x800fdbcc,2,&v));if(s32(sx16(v))<0)goto ground;
 TRY(rd(r,0x8006f1c0,0x800fdc34,4,&p));TRY(rd(r,0x8006f1c8,p+0xc2,2,&v));if(!v)goto ground;v=sx16(v);TRY(rd(r,0x8006f1d8,p+0x10,4,&q));if(q)goto ground;TRY(wr(r,0x8006f1e8,ball+0xc2,2,v));
 boundary_phase:
 TRY(rd(r,0x8006f1f0,0x800fdb90,2,&phase));phase=sx16(phase);
 if(phase==0x82){TRY(rd(r,0x8006f214,0x800fe884,2,&v));if(v!=3)goto ground;TRY(rd(r,0x8006f228,0x800fdbcc,2,&held));held=sx16(held);if(s32(held)>=0){TRY(rd(r,0x8006f23c,0x800fe8ca,2,&v));if(held==sx16(v))goto ground;}}
 else if(s32(phase)>=128)goto ground;
 TRY(rd(r,0x8006f250,0x800fe8c4,2,&v));if(v)goto ground;TRY(rd(r,0x8006f264,0x800fe8cc,2,&v));if(v)goto ground;TRY(rd(r,0x8006f278,0x800fdb58,4,&v));if(!v)goto ground;TRY(rule_gate(r,&v));if(v)goto ground;
 TRY(rd(r,0x8006f29c,0x800fdb96,2,&v));if(v){CALL(0x8006f2bc,0x80029590,1,12,0);v=20000;}else{CALL(0x8006f2ac,0x80029590,1,11,0);v=5000;}
 CALL(0x8006f2c8,0x800295c8,1,v,0);TRY(select_score(r,4,0));TRY(reset_bounds(r));
 ground:
 if(s32(height)<0x400){
  vertical=shr(vertical,2)-vertical;height=0x400;
  if(s32(vertical)>=72){
   CALL(0x8006f2fc,0x80029258,1,0,0);remaining=0;TRY(rd(r,0x8006f304,ball+0x14,2,&x));TRY(rd(r,0x8006f30c,ball+0x16,2,&z));x=sx16(x);q=shr(x,2);if(q==UINT32_MAX)q=0;TRY(wr(r,0x8006f334,ball+0x14,2,x-q));z=sx16(z);q=shr(z,2);if(q==UINT32_MAX)q=0;TRY(wr(r,0x8006f3bc,ball+0x16,2,z-q));
  }else{
   TRY(rd(r,0x8006f354,ball+0x14,2,&x));x=sx16(x);vertical=0;
   if(x){q=shr(x,6);if(!q)q=s32(x)<0?UINT32_MAX:1;TRY(wr(r,0x8006f388,ball+0x14,2,x-q));}
   TRY(rd(r,0x8006f38c,ball+0x16,2,&z));z=sx16(z);if(z){q=shr(z,6);if(!q)q=s32(z)<0?UINT32_MAX:1;TRY(wr(r,0x8006f3bc,ball+0x16,2,z-q));}
  }
 }
 TRY(wr(r,0x8006f3c0,ball+0x18,2,vertical));TRY(wr(r,0x8006f3c4,ball+0x10,4,height));TRY(rd(r,0x8006f3cc,0x800fdbcc,2,&held));
 if(s32(sx16(held))>=0){TRY(attachment(r,0x8006f3dc,0x80057f5c,&v));TRY(rd(r,0x8006f3e4,ball+0x10,4,&q));if(s32(q)>=s32(v)){TRY(wr(r,0x8006f3f8,ball+0x10,4,v));TRY(attachment(r,0x8006f3fc,0x80058120,0));}}
 else{TRY(rd(r,0x8006f40c,ball+0x14,2,&x));TRY(rd(r,0x8006f410,ball+8,4,&v));TRY(rd(r,0x8006f414,ball+0x16,2,&z));TRY(rd(r,0x8006f418,ball+12,4,&q));TRY(wr(r,0x8006f424,ball+8,4,v+sx16(x)));TRY(wr(r,0x8006f428,ball+12,4,q+sx16(z)));}
 goto speed;
 held_attach:
 TRY(attachment(r,0x8006f0c4,0x80058120,0));remaining=0;TRY(held_boundary(r));
 speed:
 TRY(rd(r,0x8006f42c,ball+0x14,2,&x));x=sx16(x);if(s32(x)<0)x=0-x;TRY(rd(r,0x8006f440,ball+0x16,2,&z));z=sx16(z);if(s32(z)<0)z=0-z;
 v=distance(x,z);TRY(wr(r,0x8006f45c,ball+0xa0,2,v));TRY(rd(r,0x8006f464,0x800fdbcc,2,&held));if(s32(sx16(held))>=0)goto substep_end;
 TRY(rd(r,0x8006f474,ball+0x10,4,&v));height=shr(v,8);flag=0;
 if(s32(height)>=72){flag=1;if(s32(height)>=109)goto previous_rim;
  TRY(rd(r,0x8006f498,ball+8,4,&x));x=shr(x,8);if(s32(x)<0)x=0-x;if(s32(x)>=348||s32(x)<324)goto previous_rim;
  TRY(rd(r,0x8006f4c4,ball+12,4,&z));z=shr(z,8);if(s32(z)<29&&s32(z)>=-28)goto rim_collision;
 }else{
  TRY(rd(r,0x8006f4f0,0x800fdbb2,2,&v));if(!v)goto previous_rim;TRY(rd(r,0x8006f500,ball+0x18,2,&v));if(s32(sx16(v))>=0)goto previous_rim;
  TRY(rd(r,0x8006f514,0x800fdb90,2,&v));if(s32(sx16(v))<128){TRY(rd(r,0x8006f528,ball+8,4,&v));TRY(rd(r,0x8006f530,0x8001ee04,4,&q));if(s32(v^q)<0)TRY(wr(r,0x8006f548,0x800fdbb0,2,1));}TRY(wr(r,0x8006f550,0x800fdbb2,2,0));
 }
 previous_rim:
 /*6F554 compares the saved full-coordinate words against the small integer
  * thresholds directly, unlike the preceding current-coordinate >>8 tests.
  * Preserve this original scale mismatch rather than silently correcting it. */
 TRY(rd(r,0x8006f554,ball+0x2c,4,&previous));if(s32(previous)<72)goto no_rim;++flag;if(s32(previous)>=109)goto no_rim;
 TRY(rd(r,0x8006f570,ball+0x24,4,&x));if(s32(x)<0)x=0-x;if(x-324>=25)goto no_rim;TRY(rd(r,0x8006f594,ball+0x28,4,&z));if(z+28>=57)goto no_rim;
 rim_collision:
 TRY(rim(r,ball,&v));v&=255;TRY(backboard(r,ball,&q));if(!(v|(q&255))) {CALL(0x8006f5d0,0x8006dc18,1,ball,0);TRY(backboard(r,ball,&q));}
 goto substep_end;
 no_rim:
 TRY(rd(r,0x8006f5f0,0x800fdb90,2,&v));if(v==1&&!flag){TRY(rd(r,0x8006f608,ball+0x18,2,&v));if(s32(sx16(v))<0){TRY(wr(r,0x8006f61c,0x800fdbd6,2,0));TRY(wr(r,0x8006f624,0x800fdbd4,2,0));TRY(wr(r,0x8006f628,0x800fdb90,2,0));}}
 substep_end:
 --remaining;if(s32(remaining)>=0)goto again;
 TRY(rd(r,0x8006f638,ball+8,4,&x));x=shr(x,8);zone=0;
 if(s32(x)>=376){zone=7;if(s32(x)>=417){TRY(rd(r,0x8006f660,0x800fdbcc,2,&v));if(s32(sx16(v))<0){TRY(wr(r,0x8006f678,ball+8,4,0x1a000));TRY(boundary(r));TRY(rd(r,0x8006f67c,ball+0x14,2,&v));if(s32(sx16(v))>0)TRY(wr(r,0x8006f690,ball+0x14,2,0));}}}
 else if(s32(x)<-375){zone=3;if(s32(x)<-416){TRY(rd(r,0x8006f6ac,0x800fdbcc,2,&v));if(s32(sx16(v))<0){TRY(wr(r,0x8006f6c4,ball+8,4,0xfffe6000));TRY(boundary(r));TRY(rd(r,0x8006f6c8,ball+0x14,2,&v));if(s32(sx16(v))<0)TRY(wr(r,0x8006f6d8,ball+0x14,2,0));}}}
 TRY(rd(r,0x8006f6dc,ball+12,4,&z));z=shr(z,8);
 if(s32(z)>=200){TRY(rd(r,0x8006f6fc,0x800b8a54+zone,1,&zone));zone=zone&128?zone|0xffffff00u:zone;if(s32(z)>=241){TRY(rd(r,0x8006f710,0x800fdbcc,2,&v));if(s32(sx16(v))<0){TRY(wr(r,0x8006f728,ball+12,4,0xf000));TRY(boundary(r));TRY(rd(r,0x8006f72c,ball+0x16,2,&v));if(s32(sx16(v))>0)TRY(wr(r,0x8006f740,ball+0x16,2,0));}}}
 else if(s32(z)<-199){TRY(rd(r,0x8006f754,0x800b8a5c+zone,1,&zone));zone=zone&128?zone|0xffffff00u:zone;if(s32(z)<-240){TRY(rd(r,0x8006f768,0x800fdbcc,2,&v));if(s32(sx16(v))<0){TRY(wr(r,0x8006f784,ball+12,4,0xffff1000));TRY(boundary(r));TRY(rd(r,0x8006f788,ball+0x16,2,&v));if(s32(sx16(v))<0)TRY(wr(r,0x8006f798,ball+0x16,2,0));}}}
 TRY(rd(r,0x8006f79c,ball+0x14,2,&x));TRY(wr(r,0x8006f7a0,ball+0xc2,2,zone));x=sx16(x);if(s32(x)<0)x=0-x;TRY(rd(r,0x8006f7b0,ball+0x16,2,&z));z=sx16(z);if(s32(z)<0)z=0-z;v=distance(x,z);
 TRY(rd(r,0x8006f7cc,ball+0x14,2,&x));TRY(rd(r,0x8006f7d0,ball+8,4,&q));TRY(wr(r,0x8006f7d4,ball+0xa0,2,v));TRY(wr(r,0x8006f7e4,0x800fdbc0,4,q+(sx16(x)<<4)));
 TRY(rd(r,0x8006f7e8,ball+0x16,2,&z));TRY(rd(r,0x8006f7ec,ball+12,4,&q));return wr(r,0x8006f7fc,0x800fdbc4,4,q+(sx16(z)<<4));
}

static int backboard(Run* r,uint32_t ball,uint32_t* result){
 uint32_t h,oldh,d,old_d,den,x,z,v,q,vx,vz;*result=0;
 TRY(rd(r,0x8006d890,ball+0x10,4,&h));TRY(rd(r,0x8006d894,ball+0x2c,4,&oldh));d=h-0x5400;if(s32(d)>0)return NBA97_BODY_OK;old_d=oldh-0x5400;
 /*6D8CC/6D918 use wrapped32-bit products and signed DIV, including the
  * original BREAK6 overflow: oldh=800053FF,h=80005400 can make den=-1.
  * Keep this source trap; do not widen the interpolation to avoid it. */
 if(s32(old_d)>0&&s32(d)<-0x400){den=oldh-h;TRY(rd(r,0x8006d8b8,ball+8,4,&v));TRY(rd(r,0x8006d8bc,ball+0x24,4,&x));TRY(divide(r,(v-x)*d,den,0x8006d8e4,0x8006d8fc,&q));
  TRY(rd(r,0x8006d904,ball+0x28,4,&z));TRY(rd(r,0x8006d908,ball+12,4,&v));TRY(divide(r,(v-z)*d,den,0x8006d930,0x8006d948,&v));x+=q;z+=v;
 }else{if(s32(old_d)<-0x400)return NBA97_BODY_OK;TRY(rd(r,0x8006d970,ball+8,4,&x));TRY(rd(r,0x8006d974,ball+12,4,&z));}
 if(s32(z)>=513||s32(z)<-512)return NBA97_BODY_OK;
 if(s32(x)<0)x=0-x;
 if(s32(x)>0x157ff||s32(x)<=0x153ff)return NBA97_BODY_OK;
 TRY(rd(r,0x8006d9c0,ball+0x18,2,&v));v=0-sx16(v);
 /*6D9D0 is a branch-delay STORE: even an upward ball that returns0 has its
  * height snapped to5400. Do not defer this store until collision success. */
 TRY(wr(r,0x8006d9d0,ball+0x10,4,0x5400));if(s32(v)<0)return NBA97_BODY_OK;if(s32(v)>=449)v=448;TRY(wr(r,0x8006d9e4,ball+0x18,2,v));
 TRY(rd(r,0x8006d9e8,ball+0x14,2,&vx));vx=sx16(vx);if(!vx){TRY(rng(r,&v));TRY(wr(r,0x8006da30,ball+0x14,2,(v&32)-16));}
 else if(vx+16<33)TRY(wr(r,s32(vx)<0?0x8006da14:0x8006da1c,ball+0x14,2,s32(vx)<0?0xfffffff0u:16));
 TRY(rd(r,0x8006da34,ball+0x16,2,&vz));vz=sx16(vz);if(!vz){TRY(rng(r,&v));TRY(wr(r,0x8006da7c,ball+0x16,2,(v&32)-16));}
 else if(vz+16<33)TRY(wr(r,s32(vz)<0?0x8006da60:0x8006da68,ball+0x16,2,s32(vz)<0?0xfffffff0u:16));
 TRY(rd(r,0x8006da84,0x800fdbe8,2,&v));TRY(wr(r,0x8006da90,0x800fdba4,4,0x5a0));TRY(wr(r,0x8006da98,0x800fdbb2,2,0));TRY(wr(r,0x8006daa0,0x800fdbd4,2,0));if(s32(sx16(v))>0)TRY(wr(r,0x8006dab0,0x800fdbe8,2,0));
 TRY(rd(r,0x8006dab4,ball+0x18,2,&v));v=sx16(v);if(s32(v)<0)v=0-v;if(s32(v)>=65)CALL(0x8006dae8,0x80029258,1,2,0);*result=1;return NBA97_BODY_OK;
}
static int reflect_plane(Run* r,uint32_t ball,uint32_t plane,uint32_t direction){
 uint32_t x,v,q;TRY(rd(r,0x8006d4b0,ball+8,4,&x));if(s32(x)<0){plane=0-plane;direction=0-direction;}TRY(rd(r,0x8006d4c8,ball+0x14,2,&v));v=sx16(v);TRY(wr(r,0x8006d4d8,ball+8,4,plane));if(s32(v^direction)<0)return NBA97_BODY_OK;
 q=shr(0-v,1);if(q==UINT32_MAX)q=0;if(s32(q)<-511)q=0xfffffe00;else if(s32(q)>=513)q=512;TRY(wr(r,0x8006d514,ball+0x14,2,q));
 TRY(rd(r,0x8006d51c,0x800fdbdc,2,&v));if(!v){TRY(rd(r,0x8006d52c,ball+0x16,2,&v));v=sx16(v);q=shr(v,2);if(q==UINT32_MAX)q=0;q=v-q;if(s32(q)<-511)q=0xfffffe00;else if(s32(q)>=513)q=512;TRY(wr(r,0x8006d574,ball+0x16,2,q));}return wr(r,0x8006d57c,0x800fdbdc,2,0);
}
static int reflect_velocity(Run* r,uint32_t dx,uint32_t dy,uint32_t* vx,uint32_t* vy,uint32_t* result){
 uint32_t nx=0-dx,ny=0-dy,d=distance(nx,ny),dot,tangent,cross,a,b;*result=0;
 if(s32(d)>=1025||!d)return NBA97_BODY_OK;
 TRY(divide(r,nx<<8,d,0x8005dd4c,0x8005dd64,&nx));TRY(divide(r,ny<<8,d,0x8005dd80,0x8005dd98,&ny));
 dot=*vy*ny+*vx*nx;if(s32(dot)<=0)return NBA97_BODY_OK;dot=shr(dot,8);if(s32(dot)>=65){dot-=shr(dot,2);if(s32(dot)>=673)dot=672;}
 cross=*vy*nx-*vx*ny;tangent=shr(cross,8);if(tangent+64>=129){tangent-=shr(cross,11);if(s32(tangent)<-671)tangent=0xfffffd60;else if(s32(tangent)>=673)tangent=672;}
 a=shr((0-dot)*nx-tangent*ny,8);b=shr(tangent*nx-dot*ny,8);*vx=a;*vy=b;*result=1;return NBA97_BODY_OK;
}
static int rim(Run* r,uint32_t ball,uint32_t* result){
 uint32_t previous,x,z,v,phase,q,vx,vy,argument;int vertical_plane=0;*result=0;
 TRY(rd(r,0x8006d598,ball+0x24,4,&previous));TRY(rd(r,0x8006d59c,ball+8,4,&x));if(s32(previous)<0){x=0-x;previous=0-previous;}previous-=0x15800;x-=0x15800;
 if(s32(x^previous)<0)goto plane;
 /*6D71C retains the already-subtracted previousX in this comparison. */
 if(s32(previous)<=0x15400)return NBA97_BODY_OK;
 TRY(rd(r,0x8006d728,ball+12,4,&argument));
 if(s32(argument)>=0x1801)argument-=0x1800;
 else if(s32(argument)<-0x1800)argument+=0x1800;
 else{TRY(rd(r,0x8006d74c,ball+0x10,4,&argument));vertical_plane=1;if(s32(argument)<0x4c00)argument-=0x4c00;else if(s32(argument)<0x6801){argument-=0x6800;goto plane;}else argument-=0x6800;}
 TRY(rd(r,vertical_plane?0x8006d7d4:0x8006d778,ball+8,4,&x));x+=s32(x)<0?0x15800:0xfffea800u;x=shr(x,1);argument=shr(argument,1);
 TRY(rd(r,vertical_plane?0x8006d7fc:0x8006d7a0,ball+0x14,2,&vx));TRY(rd(r,vertical_plane?0x8006d800:0x8006d7a4,ball+0x16,2,&vy));vx=sx16(vx);vy=sx16(vy);TRY(reflect_velocity(r,x,argument,&vx,&vy,&v));if(!v)return NBA97_BODY_OK;
 if(vertical_plane){TRY(wr(r,0x8006d838,ball+0x14,2,vx));if(s32(vy)<0){TRY(rd(r,0x8006d83c,ball+0x18,2,&v));vy+=sx16(v);}TRY(rd(r,0x8006d850,ball+0x16,2,&z));TRY(wr(r,0x8006d854,ball+0x18,2,vy));TRY(wr(r,0x8006d868,ball+0x16,2,z-shr(sx16(z),2)));}
 /* The other5DCEC call modifies only private temporary velocities; source
  *6D7C4 discards them and still takes the collision effects path on success. */
 goto collision;
 plane:
 if(s32(previous)<0){TRY(reflect_plane(r,ball,0x15400,1));goto collision;}
 TRY(reflect_plane(r,ball,0x15c00,UINT32_MAX));TRY(rd(r,0x8006d5e4,0x80021d8a,1,&v));if(!v)goto collision;
 TRY(rd(r,0x8006d5f8,0x800fe8cc,2,&v));if(v)goto collision;TRY(rd(r,0x8006d60c,0x800fe8bc,2,&v));if(v==4)goto collision;TRY(rd(r,0x8006d620,0x800fe8c2,2,&v));if(v==4)goto collision;
 TRY(rd(r,0x8006d634,0x800fdb90,2,&phase));phase=sx16(phase);if(s32(phase)>=128){if(phase!=0x82)goto collision;TRY(rd(r,0x8006d654,0x800fe884,2,&v));if(v!=3)goto collision;}
 TRY(rd(r,0x8006d668,0x800fe8c4,2,&v));if(!v){TRY(select_score(r,4,1));TRY(rd(r,0x8006d684,0x800fdb94,2,&v));if(v){CALL(0x8006d6a4,0x80029590,1,12,0);v=20000;}else{CALL(0x8006d694,0x80029590,1,11,0);v=5000;}CALL(0x8006d6b0,0x800295c8,1,v,0);}
 TRY(reset_bounds(r));TRY(wr(r,0x8006d6c8,0x800fe882,2,1));
 collision:
 TRY(rd(r,0x8006d6ec,0x800fdbe8,2,&q));if(s32(sx16(q))>0)TRY(wr(r,0x8006d6fc,0x800fdbe8,2,0));TRY(wr(r,0x8006d704,0x800fdbb2,2,0));CALL(0x8006d708,0x80029258,1,1,0);*result=1;return NBA97_BODY_OK;
}
static int begin(Nba97BallSimulationContext* c,Nba97BallSimulationProgress* p,Run* r){
 if(!p)return NBA97_BODY_ARGUMENT;
 memset(p,0,sizeof *p);if(!c||!c->access)return NBA97_BODY_ARGUMENT;r->in=c;r->out=p;return NBA97_BODY_OK;
}
static int finish(Run* r,int status){if(status==NBA97_BODY_OK){r->out->completed=1;r->out->stopped_pc=0;r->out->stopped_address=0;}return status;}
int nba97_game_ball_simulate(Nba97BallSimulationContext* c,uint32_t ball,Nba97BallSimulationProgress* p){Run r;TRY(begin(c,p,&r));return finish(&r,simulate(&r,ball));}
int nba97_game_ball_backboard(Nba97BallSimulationContext* c,uint32_t ball,Nba97GamePeriodValue* v,Nba97BallSimulationProgress* p){Run r;uint32_t word=0;int rc;TRY(begin(c,p,&r));if(!v)return NBA97_BODY_ARGUMENT;v->word=0;v->known=0;rc=backboard(&r,ball,&word);if(rc==NBA97_BODY_OK){v->word=word;v->known=1;}return finish(&r,rc);}
int nba97_game_ball_rim(Nba97BallSimulationContext* c,uint32_t ball,Nba97GamePeriodValue* v,Nba97BallSimulationProgress* p){Run r;uint32_t word=0;int rc;TRY(begin(c,p,&r));if(!v)return NBA97_BODY_ARGUMENT;v->word=0;v->known=0;rc=rim(&r,ball,&word);if(rc==NBA97_BODY_OK){v->word=word;v->known=1;}return finish(&r,rc);}
