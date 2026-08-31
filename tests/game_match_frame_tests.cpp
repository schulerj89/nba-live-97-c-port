#include "recovered/game_match_frame.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks=0;void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
using U=std::uint32_t;
struct Fixture {
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200040),known=std::vector<std::uint8_t>(0x200040);
 std::vector<Nba97MatchFrameCall> calls;Nba97MatchFrameProgress progress{};
 unsigned actor_mode=0,repeat=0;U fail_pc=0,status=0xabcdef01;bool unknown_status=false;
 static size_t offset(U a,unsigned n){if(a>=0x80000000&&std::uint64_t(a)+n<=0x80200000)return a-0x80000000;
  if(a>=0x1f800000&&std::uint64_t(a)+n<=0x1f800040)return 0x200000+a-0x1f800000;
  throw std::out_of_range("unowned memory");}
 void put(U a,U v,unsigned n=4){auto o=offset(a,n);for(unsigned i=0;i<n;++i){bytes[o+i]=std::uint8_t(v>>(i*8));known[o+i]=1;}}
 U get(U a,unsigned n=4)const{auto o=offset(a,n);U v=0;for(unsigned i=0;i<n;++i)v|=U(bytes[o+i])<<(i*8);return v;}
 static int access(void* u,U,U a,unsigned n,unsigned kind,Nba97PlayerFrameValue* value){auto& f=*static_cast<Fixture*>(u);
  try{auto o=offset(a,n);if(kind)f.put(a,value->word,n);else{*value={};for(unsigned i=0;i<n;++i)if(f.known[o+i]){value->word|=U(f.bytes[o+i])<<(8*i);value->known_mask|=std::uint8_t(1u<<i);}}return NBA97_BODY_OK;}
  catch(const std::out_of_range&){return NBA97_BODY_BOUNDS;}}
 static int io(void* u,const Nba97MatchFrameCall* q,Nba97GamePeriodValue* out){auto& f=*static_cast<Fixture*>(u);f.calls.push_back(*q);
  if(q->pc==f.fail_pc)return NBA97_BODY_BOUNDS;
  if(q->entry==0x80048ff4){*out={f.status,std::uint8_t(!f.unknown_status)};if(f.unknown_status)out->word=0;f.status&=~1u;}
  if(q->entry==0x8004900c)f.status=q->args[0];
  if(q->entry==0x8004a044){if(f.actor_mode==1)f.put(0x801029b0,9);if(f.actor_mode==2&&f.progress.actor_updates==0)f.put(0x801029b0,0xffffffffu);}
  if(q->entry==0x800530fc&&f.repeat&&f.progress.frames==f.repeat)f.put(0x1f800004,0xff0000ff);
  return NBA97_BODY_OK;
 }
 Fixture(){put(0x8001ede8,0);put(0x800b729c,384);put(0x800fc660,0x80140000);put(0x80140000,0,2);put(0x800fc630,0x80140004);put(0x80140004,1,2);put(0x800b2048,0x80141000);put(0x80141053,0xfe,1);put(0x1f800030,0);}
 int run(size_t budget=10000,bool service=true){Nba97MatchFrameContext c{access,service?io:nullptr,this,budget};return nba97_game_match_frame(&c,&progress);}
 unsigned count(U entry)const{unsigned n=0;for(const auto& q:calls)n+=q.entry==entry;return n;}
 const Nba97MatchFrameCall& at(U pc)const{for(const auto& q:calls)if(q.pc==pc)return q;throw std::runtime_error("missing call");}
};
void sequencing(){
 Fixture f;check(f.run()==NBA97_BODY_OK&&f.progress.completed,"complete sequencing with explicit service fixtures");
 check(f.progress.frames==1&&f.progress.actor_updates==10&&f.progress.submissions==2,"one frame, nominal actor loop and both submissions");
 check(f.get(0x8001ede8)==1&&f.get(0x80102924)==0x800f5c50&&f.get(0x801046d8)==0x800fccf0,"source alternate bank tables");
 check(f.at(0x80049084).args[0]==0x800fccf0&&f.at(0x80049084).args[1]==32,"small table clear first");
 check(f.at(0x80049094).args[0]==0x800f5c50&&f.at(0x80049094).args[1]==4096,"large table clear second");
 check(f.at(0x800490ac).args[0]==384&&f.at(0x800490c0).args[0]==256&&f.at(0x800490c0).args[1]==120,"source geometry controls");
 std::vector<U> render;for(const auto& q:f.calls)if(q.pc>=0x800490c8&&q.pc<=0x80049198)render.push_back(q.entry);
 check(render==std::vector<U>{0x80075d40,0x8004b1a4,0x80052914,0x8004ac68,0x80057f5c,0x80049300,0x80049d34,0x80035bec},"actual pass order and attachment before ball");
 check(f.get(0x80141053,1)==0xff&&f.get(0x801029b0)==10,"style byte53 XOR1 and live index increments");
 check(f.at(0x80049234).args[0]==0x80022070&&f.at(0x80049264).args[0]==0x80021f48,"different display and draw bank strides");
 check(f.at(0x80049290).args[0]==0x800fcd6c&&f.at(0x800492a0).args[0]==0x800f9c4c,"small then main reverse-table submission");
 check(f.at(0x800492a8).args[0]==0&&f.at(0x800492b0).args[0]==1&&f.at(0x800492b8).args[0]==2&&f.status==0xabcdef01,"text groups and captured interrupt status restored");
 for(U bank:std::array<U,4>{1,2,0x80000000,0xffffffff}){Fixture b;b.put(0x8001ede8,bank);check(b.run()==NBA97_BODY_OK&&b.get(0x8001ede8)==0,"nonzero raw bank toggles to zero");}
 for(U mode:std::array<U,6>{0,1,2,3,4,65535}){Fixture a;a.put(0x80140004,mode,2);check(a.run()==NBA97_BODY_OK,"raw unsigned attachment selector");
  check(a.count(0x80057f5c)==unsigned(mode==1)&&a.count(0x80058120)==unsigned(mode==2)&&a.count(0x800581c0)==unsigned(mode==3),"only exact attachment modes dispatch");}
 Fixture pause;pause.put(0x80140000,65535,2);pause.known[Fixture::offset(0x800fc630,4)]=0;check(pause.run()==NBA97_BODY_OK&&!pause.count(0x80057f5c),"paused frame never consumes selector pointer");
 Fixture live;live.actor_mode=1;check(live.run()==NBA97_BODY_OK&&live.progress.actor_updates==1,"actor callback live index change suppresses remaining loop");
 Fixture wrap;wrap.actor_mode=2;check(wrap.run()==NBA97_BODY_OK&&wrap.progress.actor_updates==11,"wrapped negative index survives callback");
 Fixture repeat;repeat.repeat=2;for(unsigned i=0;i<4;++i)repeat.put(0x1f800030+i*4,0x1f800030+i*4);repeat.put(0x1f800004,0xff0002ff);
 check(repeat.run()==NBA97_BODY_OK&&repeat.progress.frames==2&&repeat.count(0x800530fc)==2&&repeat.progress.submissions==4,"source scratch redraw repeats render poses and submission");
 check(repeat.get(0x8001ede8)==0&&repeat.get(0x80141053,1)==0xfe,"redraw toggles bank and style again");
}
void refusal(){
 Fixture base;check(base.run()==NBA97_BODY_OK,"prefix baseline");
 for(size_t limit=0;limit<base.progress.operations;++limit){Fixture f;check(f.run(limit)==NBA97_BODY_JOURNAL_LIMIT&&!f.progress.completed&&f.progress.operations==limit,"bounded original operation prefix");}
 Fixture missing;check(missing.run(10000,false)==NBA97_MATCH_FRAME_IO_REQUIRED&&missing.progress.stopped_entry==0x800530fc&&missing.progress.stores==0,"no successful pose service default");
 Fixture unknown;unknown.unknown_status=true;check(unknown.run()==NBA97_BODY_UNKNOWN&&unknown.progress.stores==3&&unknown.status==0xabcdef00,"unknown returned status retains earlier bank stores and service effect");
 Fixture failed;failed.fail_pc=0x800491b8;check(failed.run()==NBA97_BODY_BOUNDS&&failed.get(0x80141053,1)==0xff&&!failed.count(0x8004a044),"delay-slot style write precedes failed sync");
 Fixture bad;bad.put(0x800fc660,0x80140001);check(bad.run()==NBA97_BODY_ALIGNMENT_TRAP&&bad.progress.stopped_pc==0x80049114,"unaligned source halfword stops after preceding render services");
 for(unsigned i=0;i<4;++i){Fixture f;for(unsigned j=0;j<i;++j)f.put(0x1f800030+j*4,0x1f800030+j*4);f.put(0x1f800030+i*4,0);check(f.run()==NBA97_BODY_OK,"sentinel mismatch skips unknown later scratchwords");}
 Fixture scratch;for(unsigned i=0;i<4;++i)scratch.put(0x1f800030+i*4,0x1f800030+i*4);check(scratch.run()==NBA97_BODY_UNKNOWN&&scratch.progress.stopped_address==0x1f800004&&scratch.progress.submissions==2,"required final scratch guard can refuse after both submissions");
 Nba97MatchFrameProgress p{};check(nba97_game_match_frame(nullptr,&p)==NBA97_BODY_ARGUMENT&&!p.completed,"invalid context");
}
}
int main(){try{sequencing();refusal();std::cout<<checks<<" match frame checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
