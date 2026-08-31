#include "game_ball_scoring.h"
#include "game_controller_selection.h"
#include <string.h>
typedef struct Run {Nba97BallScoringContext* in;Nba97BallScoringProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_BODY_OK)return status_;}while(0)
static int32_t s32(uint32_t v){return v<0x80000000u?(int32_t)v:-1-(int32_t)~v;}
static uint32_t sx16(uint32_t v){return v&0x8000u?(v&65535u)|0xffff0000u:v&65535u;}
static uint32_t sx8(uint32_t v){return v&128u?(v&255u)|0xffffff00u:v&255u;}
static uint32_t sar(uint32_t v,unsigned n){return v&0x80000000u?(v>>n)|(~0u<<(32-n)):v>>n;}
static uint32_t mask(unsigned n){return n==4?UINT32_MAX:(1u<<(8*n))-1;}
static unsigned knowledge(unsigned n){return (1u<<n)-1;}
static int reserve(Run* r,uint32_t pc,uint32_t address){
 r->out->stopped_pc=pc;r->out->stopped_address=address;
 if(r->out->operations>=r->in->operation_budget)return NBA97_BODY_JOURNAL_LIMIT;
 ++r->out->operations;return NBA97_BODY_OK;
}
static int access(Run* r,uint32_t pc,uint32_t a,unsigned n,unsigned kind,Nba97PlayerFrameValue* v){
 int rc;unsigned i;TRY(reserve(r,pc,a));if((n==4&&(a&3))||(n==2&&(a&1)))return NBA97_BODY_ALIGNMENT_TRAP;
 rc=r->in->access(r->in->user,pc,a,n,kind,v);if(rc!=NBA97_BODY_OK)return rc;
 if(v->is_reference>1||v->reference.known>1||(!v->reference.known&&(v->reference.allocation||v->reference.offset)))return NBA97_BODY_ARGUMENT;
 if(!v->is_reference&&v->reference.known)return NBA97_BODY_ARGUMENT;
 if(v->is_reference&&(n!=4||(!v->reference.known&&(v->known_mask||v->word))))return NBA97_BODY_ARGUMENT;
 if(v->known_mask&~knowledge(n) || (v->word&~mask(n)))return NBA97_BODY_ARGUMENT;
 for(i=0;i<n;++i)if(!(v->known_mask&(1u<<i))&&(v->word&(255u<<(i*8))))return NBA97_BODY_ARGUMENT;
 if(kind==NBA97_FRAME_READ)++r->out->reads;else ++r->out->stores;return NBA97_BODY_OK;
}
static int rd(Run* r,uint32_t pc,uint32_t a,unsigned n,uint32_t* w){Nba97PlayerFrameValue v={0};TRY(access(r,pc,a,n,NBA97_FRAME_READ,&v));if(v.is_reference||v.known_mask!=knowledge(n))return NBA97_BODY_UNKNOWN;*w=v.word;return NBA97_BODY_OK;}
static int wr(Run* r,uint32_t pc,uint32_t a,unsigned n,uint32_t w){Nba97PlayerFrameValue v={0};v.word=w&mask(n);v.known_mask=(uint8_t)knowledge(n);return access(r,pc,a,n,NBA97_FRAME_WRITE,&v);}
static int service(Run* r,uint32_t pc,uint32_t entry,unsigned count,uint32_t a,uint32_t b,uint32_t c,unsigned ret,uint32_t* word){
 Nba97BallScoringCall q;Nba97PlayerFrameValue v={0};int rc;unsigned i;TRY(reserve(r,pc,entry));if(!r->in->service)return NBA97_BALL_SCORING_SERVICE_REQUIRED;
 q.pc=pc;q.entry=entry;q.argument[0]=a;q.argument[1]=b;q.argument[2]=c;q.count=count;q.return_bytes=ret;
 rc=r->in->service(r->in->user,&q,&v);if(rc!=NBA97_BODY_OK)return rc;++r->out->services;
 if(ret){if(v.is_reference||v.reference.known||v.reference.allocation||v.reference.offset||v.known_mask&~15u)return NBA97_BODY_ARGUMENT;
  for(i=0;i<4;++i)if(!(v.known_mask&(1u<<i))&&(v.word&(255u<<(i*8))))return NBA97_BODY_ARGUMENT;
  if((v.known_mask&knowledge(ret))!=knowledge(ret))return NBA97_BODY_UNKNOWN;
  *word=v.word&mask(ret);}
 return NBA97_BODY_OK;
}
#define CALL(pc,e,n,a,b,c) TRY(service(r,pc,e,n,a,b,c,0,0))
static int divide(Run* r,uint32_t a,uint32_t b,uint32_t zero,uint32_t overflow,uint32_t* q){if(!b||(a==0x80000000u&&b==UINT32_MAX)){r->out->stopped_pc=b?overflow:zero;r->out->stopped_address=0;return NBA97_FRAME_ARITHMETIC_TRAP;}*q=(uint32_t)(s32(a)/s32(b));return NBA97_BODY_OK;}
static uint32_t distance(uint32_t x,uint32_t z){return (uint32_t)nba97_game_selection_distance(s32(x),s32(z));}
static int rng(Run* r,uint32_t* out){uint32_t v;TRY(rd(r,0x8002ab78,0x8001edee,2,&v));if(!v)TRY(wr(r,0x8002ab88,0x8001edee,2,0xa5a5));TRY(rd(r,0x8002ab8c,0x8001edee,2,&v));v=(v<<1)^((v&0x4000)?0x1d87:0);TRY(wr(r,0x8002aba8,0x8001edee,2,v));*out=v&65535;return NBA97_BODY_OK;}

static int grid_position(Run* r,uint32_t* x,uint32_t* z,uint32_t* result){uint32_t ball,v,c;TRY(rd(r,0x8006db54,0x800fdc48,4,&ball));TRY(rd(r,0x8006db68,ball+8,4,&v));c=s32(v)<0?0xfffeb200u:0x14e00u;TRY(wr(r,0x8006db90,0x800fdc2c,4,c));TRY(rd(r,0x8006db9c,ball+8,4,&v));TRY(rd(r,0x8006dba0,0x800fdc2c,4,&c));TRY(rd(r,0x8006dba4,ball+12,4,z));*x=sar(distance(v-c,*z),8);TRY(rd(r,0x8006dbb8,ball+0x24,4,&v));TRY(rd(r,0x8006dbbc,0x800fdc2c,4,&c));TRY(rd(r,0x8006dbc0,ball+0x28,4,z));*z=sar(distance(v-c,*z),8);*result=(s32(*x)>=11&&s32(*z)>=11);return NBA97_BODY_OK;}
static int grid_test(Run* r,uint32_t x,uint32_t z,uint32_t* index,uint32_t* result){uint32_t v;++r->out->grid_tests;if(x>=9||z>=9){*result=0;return NBA97_BODY_OK;}*index=z*9+x;TRY(rd(r,0x8006db30,0x800b8c5c+*index,1,&v));*result=sx8(v);return NBA97_BODY_OK;}

static void reflect_in(uint32_t* x,uint32_t* z,uint32_t nx,uint32_t nz){uint32_t dot=*z*nz+*x*nx,tangent,cross,a,b;if(s32(dot)<=0)return;dot=sar(dot,8);if(s32(dot)>=65){dot-=sar(dot,2);if(s32(dot)>=673)dot=672;}cross=*z*nx-*x*nz;tangent=sar(cross,8);if(tangent+64>=129){tangent-=sar(cross,11);if(s32(tangent)<-671)tangent=0xfffffd60;else if(s32(tangent)>=673)tangent=672;}a=sar((0-dot)*nx-tangent*nz,8);b=sar(tangent*nx-dot*nz,8);*x=a;*z=b;}
static int reflect_out(uint32_t* x,uint32_t* z,uint32_t nx,uint32_t nz){uint32_t dot=*z*nz+*x*nx,tangent,cross,a,b;if(s32(dot)<=0)return 0;dot=sar(dot,8);dot=0-dot;cross=*z*nx-*x*nz;tangent=sar(cross,8);if(tangent+64>=129){tangent-=sar(cross,10);if(s32(tangent)<-671)tangent=0xfffffd60;else if(s32(tangent)>=673)tangent=672;}a=sar(dot*nx+tangent*nz,8);b=sar(tangent*nx-dot*nz,8);*x=0-a;*z=b;return 1;}

static int call_2d358(Run* r,uint32_t pc,uint32_t a){(void)pc;return service(r,0x8002d364,0x8004c374,1,sx16(a),0,0,0,0);}
static int call_5847c(Run* r,uint32_t target,uint32_t* result){uint32_t a,b,c,v;TRY(rd(r,0x80058484,0x800fdbf4,4,&a));TRY(rd(r,0x8005848c,0x800fdbf8,4,&b));TRY(rd(r,0x80058498,target+0x10,4,&c));TRY(service(r,0x8005849c,0x800583fc,3,a,b,c,1,&v));v&=255;if(v)TRY(wr(r,0x800584b8,0x800fdbd8,2,3));*result=v;return NBA97_BODY_OK;}
static int call_35318(Run* r,uint32_t kind){uint32_t style;TRY(rd(r,0x8003531c,0x800b2048,4,&style));TRY(wr(r,0x8003532c,style+0x28,2,0x3c));TRY(wr(r,0x80035338,0x800fea28,2,kind));TRY(wr(r,0x80035340,0x800fea2a,2,UINT32_MAX));TRY(wr(r,0x80035348,0x800fea24,2,0xf0));CALL(0x8003534c,0x80031c8c,0,0,0,0);CALL(0x80035354,0x800345e0,0,0,0,0);TRY(rd(r,0x80035360,0x800b2048,4,&style));return wr(r,0x8003536c,style+0x28,2,0x7fff);}
static int close_clock(Run* r,uint32_t* result){uint32_t a,b;TRY(rd(r,0x80031fe8,0x8001ee22,2,&a));TRY(rd(r,0x80031ff0,0x8001eee6,2,&b));a=sx16(a-b);if(s32(a)<0)a=0-a;a=sx16(a);*result=s32(a)<4;return NBA97_BODY_OK;}
static int randomize_ball(Run* r,uint32_t ball){uint32_t v,x;TRY(rd(r,0x8006e16c,ball+0x18,2,&v));if(!v)return NBA97_BODY_OK;TRY(rng(r,&x));TRY(rd(r,0x8006e19c,ball+0x14,2,&v));x&=0x17;if(s32(sx16(v))<0)x=0-x;TRY(wr(r,0x8006e1b4,ball+0x14,2,x));TRY(rng(r,&x));TRY(rd(r,0x8006e1b8,ball+0x16,2,&v));x&=0x17;if(s32(sx16(v))<0)x=0-x;return wr(r,0x8006e1d0,ball+0x16,2,x);}

static int simulate_score(Run* r,uint32_t ball){
 uint32_t v,phase,height,oldheight,gx,gz,index,hit,s5=1,s4,s2,s0,s1,a,b,c,d,nx,nz,qx,qz,actor,target,cellx,small_path;
 TRY(rd(r,0x8006dc3c,ball+0x18,2,&v));if(s32(sx16(v))>=0)return NBA97_BODY_OK;TRY(rd(r,0x8006dc4c,ball+0x10,4,&v));height=sar(v,8);if(s32(height)>=85)return NBA97_BODY_OK;TRY(rd(r,0x8006dc64,ball+0x2c,4,&v));oldheight=sar(v,8);if(s32(oldheight)<76)return NBA97_BODY_OK;TRY(rd(r,0x8006dc80,0x800fe8c2,2,&v));if(v==7||v==8)return NBA97_BODY_OK;
 small_path=s32(height)<81;if(small_path&&s32(oldheight)<81){TRY(rd(r,0x8006dcac,0x800fdb90,2,&phase));if(phase==0x82){TRY(rd(r,0x8006dcc0,0x800fe884,2,&v));if(!v){TRY(rd(r,0x8006dcd0,ball+8,4,&v));TRY(wr(r,0x8006dcdc,ball+12,4,0));return wr(r,s32(v)<0?0x8006dcec:0x8006dcfc,ball+8,4,s32(v)<0?0xfffeb200u:0x14e00u);}small_path=0;}}
 TRY(grid_position(r,&gx,&gz,&hit));if(hit)return NBA97_BODY_OK;
 /* The DD18 small-X shortcut is reached only through the current-height<81
  * arm.  The source's height>=81 branch enters at DD80 and deliberately
  * skips it even though both paths call the same DB48 helper. */
 if(small_path&&s32(gx)<3){TRY(rd(r,0x8006dd30,0x800fdbe8,2,&v));if(v!=1){TRY(rd(r,0x8006dd40,ball+0xb4,2,&v));if(v){TRY(rd(r,0x8006dcd0,ball+8,4,&v));TRY(wr(r,0x8006dcdc,ball+12,4,0));return wr(r,s32(v)<0?0x8006dcec:0x8006dcfc,ball+8,4,s32(v)<0?0xfffeb200u:0x14e00u);}TRY(rd(r,0x8006dd54,0x800fdb90,2,&phase));if(phase==0x82){TRY(rd(r,0x8006dd68,0x800fe884,2,&v));if(!v)return NBA97_BODY_OK;}goto scoring;}}
 TRY(rd(r,0x8006dd9c,0x800fdbe8,2,&v));if(s32(sx16(v))<0)return NBA97_BODY_OK;if(s32(gx)<3&&s32(gz)<3)return NBA97_BODY_OK;
 s2=gx-gz;s5=1;if(s32(s2)<0){s2=0-s2;s5=UINT32_MAX;}s4=oldheight-height;a=gz-2;b=oldheight-76;
 if(s32(s4)<s32(s2)){s0=s2;if(s32(s0)<=0)return NBA97_BODY_OK;do{a+=s5;if(s32(s4)>=0)--b;TRY(grid_test(r,a,b,&index,&hit));--s0;if(hit)goto grid_hit;}while(s32(s0)>0);return NBA97_BODY_OK;}
 if(!s4){TRY(grid_test(r,a,b,&index,&hit));if(!hit)return NBA97_BODY_OK;goto grid_hit;}
 {uint32_t swap=s2;s2=s4;s4=swap;s1=0-sar(s2,1);s0=s2;if(s32(s0)<=0)return NBA97_BODY_OK;do{--b;s1+=s4;if(s32(s1)>=0){s1-=s2;a+=s5;}TRY(grid_test(r,a,b,&index,&hit));--s0;if(hit)goto grid_hit;}while(s32(s0)>0);return NBA97_BODY_OK;}
 grid_hit:
 cellx=a;
 TRY(wr(r,0x8006dedc,0x800fdbe8,2,0));TRY(rd(r,0x8006dee8,0x800b8a64+index,1,&v));TRY(wr(r,0x8006def4,ball+0x10,4,sx8(v)<<8));TRY(rd(r,0x8006df00,0x800b8ab8+index,1,&v));TRY(wr(r,0x8006df14,ball+0xba,2,sx8(v)));TRY(rd(r,0x8006df04,ball+0xa0,2,&s1));s1=sx16(s1);if(s5==UINT32_MAX)s1=0-s1;
 TRY(rd(r,0x8006df1c,ball+0x18,2,&b));TRY(rd(r,0x8006df2c,0x800b8b14+index*2,2,&nx));TRY(rd(r,0x8006df38,0x800b8bb8+index*2,2,&nz));a=s1;b=sx16(b);reflect_in(&a,&b,0-sx16(nx),0-sx16(nz));
 TRY(wr(r,0x8006df64,0x800fdba4,4,0x5a0));TRY(wr(r,0x8006df6c,0x800fdbd6,2,0));TRY(wr(r,0x8006df74,0x800fdbd4,2,0));
 if(s32(b)>=0){b+=sar(b,1);if(s32(b)>=449)b=448;else if(s32(b)<192)b=192;}else{TRY(rd(r,0x8006dfb4,ball+0x10,4,&v));if(s32(sar(v,8))<81){TRY(rd(r,0x8006dfcc,ball+0x18,2,&v));if(s32(sx16(v))<s32(b))goto after_vertical;}}TRY(wr(r,0x8006dfe0,ball+0x18,2,b));
 after_vertical: TRY(wr(r,0x8006dff4,ball+0xa0,2,a));
 /* DF44 reuses stack+1C for the reflected horizontal component, overwriting
  * the earlier grid coordinate.  E324 later tests that live stack word. */
 cellx=a;if(s32(b)>=128){TRY(rd(r,0x8006dff8,ball+8,4,&v));if(v+0x14e00u<0x29c00u)TRY(call_2d358(r,0x8006e014,3));TRY(service(r,0x8006e028,0x80029258,1,2,0,0,0,0));}else{TRY(service(r,0x8006e028,0x80029258,1,3,0,0,0,0));}
 TRY(wr(r,0x8006e034,0x800fdbb2,2,0));TRY(rd(r,0x8006e038,ball+8,4,&a));TRY(rd(r,0x8006e040,0x800fdc2c,4,&v));TRY(rd(r,0x8006e044,ball+12,4,&b));a=sar(a-v,4);b=sar(b,4);d=distance(a,b);if(!d){TRY(rd(r,0x8006e06c,ball+0x24,4,&a));TRY(rd(r,0x8006e074,0x800fdc2c,4,&v));TRY(rd(r,0x8006e078,ball+0x28,4,&b));a=sar(a-v,4);b=sar(b,4);d=distance(a,b);}
 /* E0BC/E100 leave a1/a2 holding the pre-division deltas.  Only the
  * quotients in v1/v0 are published to RAM; E1E8/E294 intentionally reuse
  * those stale argument registers.  Preserve that original register quirk. */
 TRY(rd(r,0x8006e098,ball+0xba,2,&v));if(v!=gx){TRY(divide(r,a*v,d,0x8006e0c8,0x8006e0e0,&qx));TRY(rd(r,0x8006e0e8,ball+0xba,2,&v));TRY(divide(r,b*v,d,0x8006e10c,0x8006e124,&qz));TRY(rd(r,0x8006e130,0x800fdc2c,4,&c));TRY(wr(r,0x8006e140,ball+8,4,(qx<<8)+c));TRY(wr(r,0x8006e14c,ball+12,4,qz<<8));}
 TRY(rd(r,0x8006e150,ball+0xa0,2,&v));if(((v+32u)&0xffffu)<65u)return randomize_ball(r,ball);
 if(((s1+32u)&0xffffu)<65u){TRY(divide(r,a*(0-sx16(v)),d,0x8006e204,0x8006e21c,&a));TRY(rd(r,0x8006e224,ball+0xa0,2,&v));TRY(divide(r,b*(0-sx16(v)),d,0x8006e24c,0x8006e264,&b));TRY(wr(r,0x8006e26c,ball+0x14,2,a));TRY(rd(r,0x8006e270,ball+0x14,2,&v));TRY(wr(r,0x8006e288,ball+0x16,2,b));if(((v+31)&0xffffu)>=63)return NBA97_BODY_OK;if(((b+31)&0xffffu)>=63)return NBA97_BODY_OK;goto randomize;}
 nx=a;nz=b;TRY(divide(r,nx<<8,d,0x8006e2a4,0x8006e2bc,&c));TRY(rd(r,0x8006e2c4,ball+0x14,2,&a));TRY(rd(r,0x8006e2c8,ball+0xa0,2,&v));TRY(rd(r,0x8006e2cc,ball+0x16,2,&b));if(s32(sx16(v))<0)v=0-sx16(v);TRY(wr(r,0x8006e2e4,ball+0xa0,2,v));TRY(divide(r,nz<<8,d,0x8006e2f8,0x8006e310,&nz));a=sx16(a);b=sx16(b);s1=s32(s1)<0?0-s1:s1;
 if(s32(s5^cellx)<0){reflect_in(&a,&b,0-c,0-nz);if(s32(s5)>=0)s1=0-s1;}else{reflect_out(&a,&b,c,nz);}
 TRY(rd(r,0x8006e36c,ball+0xa0,2,&v));TRY(divide(r,a*sx16(v),s1,0x8006e394,0x8006e3ac,&a));TRY(rd(r,0x8006e3b4,ball+0xa0,2,&v));TRY(divide(r,b*sx16(v),s1,0x8006e3dc,0x8006e3f4,&b));TRY(wr(r,0x8006e3fc,ball+0x14,2,a));TRY(rd(r,0x8006e400,ball+0x14,2,&v));TRY(wr(r,0x8006e418,ball+0x16,2,b));if(((v+31)&0xffffu)>=63||((b+31)&0xffffu)>=63)return NBA97_BODY_OK;
 randomize: return randomize_ball(r,ball); /* Source E430 jumps back to E16C. */
 scoring:
 TRY(rd(r,0x8006e440,0x8001ee04,4,&target));TRY(rd(r,0x8006e444,ball+8,4,&v));target=s32(target^v)<0?0x8001eeb8:0x8001edf4;TRY(wr(r,0x8006e464,ball+0xac,4,0));TRY(rd(r,0x8006e468,0x800fdbd8,4,&v));if(!v){TRY(rd(r,0x8006e47c,0x800fdb90,2,&phase));if(s32(sx16(phase))<128){TRY(wr(r,0x8006e490,0x800fdbd8,2,2));TRY(call_5847c(r,target,&v));}}
 TRY(rd(r,0x8006e4a0,0x800fe8cc,2,&v));if(v==1){TRY(rd(r,0x8006e4b4,0x800fe8e4,2,&phase));if(s32(sx16(phase))<0){TRY(wr(r,0x8006e4c8,0x800fe8ce,2,1));TRY(wr(r,0x8006e4d0,0x800fe8f2,2,0));}}
 TRY(service(r,0x8006e4d4,0x8006e7ac,0,0,0,0,4,&actor));TRY(rd(r,0x8006e4e0,actor+0x14,2,&v));s0=(v^5)&65535;TRY(service(r,0x8006e4e8,0x80029258,1,4,0,0,0,0));TRY(rd(r,0x8006e4f4,0x800fdbd8,2,&phase));if(phase==3){TRY(rd(r,0x8006e504,actor+0x14,2,&v));a=v?7:13;}else if(phase==1){TRY(rd(r,0x8006e524,actor+0x14,2,&v));a=v?3:10;}else{TRY(rd(r,0x8006e53c,actor+0x14,2,&v));a=v?3:8;}TRY(service(r,0x8006e550,0x80029590,1,a,0,0,0,0));
 TRY(rd(r,0x8006e55c,0x800fe8c4,2,&v));if(!v){TRY(rd(r,0x8006e570,0x800fe8cc,2,&v));TRY(wr(r,0x8006e578,0x800fe880,2,s0));TRY(wr(r,0x8006e580,0x800fe882,2,0));if(v)TRY(wr(r,0x8006e590,0x800fe882,2,7));TRY(wr(r,0x8006e59c,0x800fdb90,2,0x82));TRY(wr(r,0x8006e5a8,0x800fe884,2,0));TRY(wr(r,0x8006e5b0,0x800fe866,2,0));TRY(wr(r,0x8006e5b8,0x800fe86e,2,3));}
 TRY(rd(r,0x8006e5bc,ball+0xa0,2,&v));if(s32(sx16(v))>=241)a=0;else{TRY(rd(r,0x8006e5d8,ball+0x18,2,&v));a=s32(sx16(v))<240?1:2;}TRY(call_2d358(r,0x8006e5f0,a));TRY(rd(r,0x8006e5fc,0x8001ee22,2,&a));TRY(rd(r,0x8006e604,0x8001eee6,2,&b));
 a=a+b;if(s32(a)>=10){TRY(rd(r,0x8006e620,0x800fdb58,4,&v));if(s32(v)>=0xe10){TRY(close_clock(r,&v));if(!v){TRY(rd(r,0x8006e64c,0x800fdbd8,2,&phase));if(phase==2){TRY(rng(r,&v));v&=0x3ff;if(v<256)a=v<123?11:10;else{TRY(rd(r,0x8006e688,0x800fdbd8,2,&phase));if(phase==3){TRY(rng(r,&v));v&=0x3ff;a=v<256?(v<123?20:15):2;}else a=2;}}else if(phase==3){TRY(rng(r,&v));v&=0x3ff;a=v<256?(v<123?20:15):2;}else a=2;goto display;}}}a=2;
 display: TRY(call_35318(r,a));TRY(wr(r,0x8006e6d0,0x800fdbda,2,0));TRY(wr(r,0x8006e6d8,0x800fdbd8,2,0));TRY(rd(r,0x8006e6dc,ball+0x14,2,&a));TRY(rd(r,0x8006e6e0,ball+0x16,2,&b));TRY(rd(r,0x8006e6e4,ball+0x18,2,&c));TRY(wr(r,0x8006e700,ball+0x14,2,sar(a<<16,18)));TRY(wr(r,0x8006e704,ball+0x16,2,sar(b<<16,18)));return wr(r,0x8006e708,ball+0x18,2,sar(c<<16,17));
}
int nba97_game_ball_scoring(Nba97BallScoringContext* c,uint32_t ball,Nba97BallScoringProgress* p){Run r;int rc;if(!p)return NBA97_BODY_ARGUMENT;memset(p,0,sizeof *p);if(!c||!c->access)return NBA97_BODY_ARGUMENT;r.in=c;r.out=p;rc=simulate_score(&r,ball);if(rc==NBA97_BODY_OK){p->completed=1;p->stopped_pc=0;p->stopped_address=0;}return rc;}
