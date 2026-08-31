#include "recovered/game_pose_frame.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using U=std::uint32_t;
unsigned checks=0;void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
struct Fixture{
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000,0xa5),known=std::vector<std::uint8_t>(0x200000,0);
 struct Event{U pc,a,value;unsigned n,kind;};std::vector<Event> events;
 Nba97PlayerFrameProgress progress{};
 void put(U a,U v,unsigned n=4){a-=0x80000000;for(unsigned i=0;i<n;++i){bytes[a+i]=std::uint8_t(v>>(i*8));known[a+i]=1;}}
 U get(U a,unsigned n=4)const{a-=0x80000000;U v=0;for(unsigned i=0;i<n;++i)v|=U(bytes[a+i])<<(i*8);return v;}
 static int access(void* user,U pc,U a,unsigned n,unsigned kind,Nba97PlayerFrameValue* v){
  auto& f=*static_cast<Fixture*>(user);if(a<0x80000000||std::uint64_t(a)+n>0x80200000)return NBA97_BODY_BOUNDS;
  auto off=a-0x80000000;for(unsigned i=0;i<n;++i)if(f.known[off+i]>1)return NBA97_BODY_ARGUMENT;
  if(kind){for(unsigned i=0;i<n;++i){f.known[off+i]=std::uint8_t((v->known_mask>>i)&1);if(f.known[off+i])f.bytes[off+i]=std::uint8_t(v->word>>(i*8));}}
  else{*v={};for(unsigned i=0;i<n;++i)if(f.known[off+i]){v->known_mask|=std::uint8_t(1u<<i);v->word|=U(f.bytes[off+i])<<(i*8);}}
  f.events.push_back({pc,a,v->word,n,kind});return NBA97_BODY_OK;
 }
 Nba97PlayerFrameContext context(std::size_t cap=100000){return {access,nullptr,nullptr,this,cap};}
 void seed(unsigned flags,bool other){
  put(0x800f0ed8,0x80110000);put(0x800fc654,0x801c0000);
  for(unsigned i=0;i<2;++i){put(0x8001ec98+i*4,0x801d0000+i*16);put(0x800170c8+i*4,0x801d0040+i*16);
   put(0x801d0008+i*16,0x80140000+i*0x1000);put(0x801d0048+i*16,0x80150000+i*0x1000);
   for(unsigned j=0;j<12;++j){put(0x80140000+i*0x1000+j*8,100+i*200+j,2);put(0x80140002+i*0x1000+j*8,200+i*100+j,2);put(0x80140004+i*0x1000+j*8,300+i*200+j,2);}
   put(0x80150002+i*0x1000,40+i*80,2);
   for(unsigned j=0;j<8;++j){put(0x80150004+i*0x1000+j*8,10+i*20+j,2);put(0x80150006+i*0x1000+j*8,20+i*10+j,2);put(0x80150008+i*0x1000+j*8,30+i*20+j,2);}}
  const std::array<U,12> map{0,1,2,3,8,9,10,11,4,5,6,7};
  for(unsigned i=0;i<12;++i)put(0x800b79b0+i*4,map[i]);
  for(unsigned i=0;i<8;++i)put(0x800b79e0+i*4,(i+4)%8);
  for(unsigned i=0;i<10;++i){const U e=0x801c0000+i*244;
   for(U off:{0x84u,0x88u,0x8cu,0x8eu,0x90u,0x92u})put(e+off,0,2);
   put(e+0x86,other?1:65535,2);put(e+0x8a,other?1:65535,2);put(e+0x94,128,2);put(e+0x96,128,2);put(e+0x9a,flags,2);
  }
 }
 int run(std::size_t cap=100000){auto c=context(cap);return nba97_game_pose_frame(&c,&progress);}
};
void frame(){
 for(unsigned flags=0;flags<16;++flags)for(bool other:{false,true}){Fixture f;f.seed(flags,other);
  check(f.run()==NBA97_BODY_OK&&f.progress.completed&&f.progress.actors==10,"all actor pose paths finish");
  check(f.get(0x801029b0)==10&&f.get(0x800f0ed4)==0x80110000+10*0xbcc,"source final live cursors");
  for(unsigned i=0;i<10;++i){const U c=0x80110000+i*0xbcc;
   check(f.get(c+0xbbc)==(other?0x800fa640+i*96:(flags&1)?0x800eb690+i*96:0x80140000),"primary source/converted/blended pointer");
   check(f.get(c+0xbc0)==(other?0x800f9d14+i*68:(flags&2)?0x800dc800+i*68:0x80150000),"secondary source/converted/blended pointer");
   check(f.get(c+0x18,2)==0&&f.get(c+0x1a,2)==0,"B frame fields copied even when B absent");
   if(other){check(f.get(0x800f9d16+i*68,2)==80,"source root-height interpolation");check(!f.known[0xf9d14+i*68]&&!f.known[0xfa640+i*96+6],"unwritten root and joint marker bytes stay unknown");}
  }
 }
 Fixture noB;noB.seed(0,false);noB.put(0x801c008e,0x1234,2);noB.put(0x801c0092,0x5678,2);
 check(noB.run()==1&&noB.get(0x80110018,2)==0x1234&&noB.get(0x8011001a,2)==0x5678,"absent B does not suppress context frame copies");
 Fixture flags;flags.seed(15,true);for(unsigned i=0;i<10;++i)flags.known[0x1c009b+i*244]=0;
 check(flags.run()==1,"discarded conversion flag high bytes need not be known");
 Fixture missing;missing.seed(0,false);for(unsigned i=0;i<4;++i)missing.known[0xf0ed8+i]=0;
 check(missing.run()==NBA97_BODY_UNKNOWN&&missing.progress.stores==2&&!missing.known[0xf0ed4],"unknown original context is copied after index store before dereference refusal");
 Fixture base;base.seed(15,true);check(base.run()==1,"prefix baseline");
 for(std::size_t cap:{0u,1u,2u,30u,60u,100u,200u,500u}){Fixture f;f.seed(15,true);check(f.run(cap)==NBA97_BODY_JOURNAL_LIMIT&&!f.progress.completed&&f.progress.operations==cap,"operation cutoff retains exact prefix");
  Fixture expected;expected.seed(15,true);for(std::size_t i=0;i<cap;++i){const auto e=base.events[i];if(e.kind)expected.put(e.a,e.value,e.n);}
  check(f.bytes==expected.bytes&&f.known==expected.known,"prefix memory and knowledge match only completed stores");}
}
void leaves(){
 for(U count:{0u,0xffffffffu,1u}){Fixture f;f.put(0x80120000,100,2);f.put(0x80120002,65535,2);f.put(0x80120004,300,2);f.put(0x80130000,0);
  auto c=f.context();check(nba97_game_pose_convert(&c,0x80120000,0x80140000,0x80130000,count,&f.progress)==1&&f.progress.stores==3,"zero and negative conversion count still run once");
  check(f.get(0x80140000,2)==1948&&f.get(0x80140002,2)==65535&&f.get(0x80140004,2)==1748&&!f.known[0x140006],"conversion retains signedY and untouched marker");}
 for(U dst:{0x80120000u,0x80120002u,0x80120008u,0x80130000u}){Fixture f;
  for(unsigned i=0;i<3;++i){f.put(0x80120000+i*2,100+i*10,2);f.put(0x80120008+i*2,300+i*10,2);}
  auto c=f.context();check(nba97_game_pose_blend(&c,0x80120000,0x80120008,dst,128,&f.progress)==1,"blender source/destination aliases supported");
  check(f.events.size()==9&&f.events[5].kind==0&&f.events[6].kind!=0,"all six halfword reads precede three writes");
  check(f.get(dst,2)==200&&f.get(dst+2,2)==210&&f.get(dst+4,2)==220,"blend uses captured source halves before alias writes");}
}
}
int main(){try{frame();leaves();std::cout<<checks<<" stateful pose checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
