#include "recovered/game_court_interactive.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks=0;void require(bool v,const char* w){++checks;if(!v)throw std::runtime_error(w);}
constexpr std::uint32_t Resource=0x80130000,Images=0x80131000,Roots=0x80140000,Records=0x80141000,Pad=0x1f800000;
constexpr std::uint32_t PlayerData=Records+0x800;
struct Fixture {
 std::array<std::vector<std::uint8_t>,9> bytes,known;std::array<Nba97GameTextRegion,9> regions{};
 std::vector<Nba97CourtInteractiveEvent> journal=std::vector<Nba97CourtInteractiveEvent>(2000);Nba97CourtInteractiveProgress progress{};Nba97CourtInteractiveContext context{};
 std::vector<std::uint32_t> input;std::size_t cursor=0;bool refuse=false;std::vector<std::array<std::uint32_t,5>> uploads;
 Fixture(bool interactive=true){
  const std::uint32_t base[]={Resource,Roots,Records,Pad,0x8001ede8,0x800dcf10,0x800faba4,0x800260e4,0x800fc650};const std::size_t size[]={0x9000,64,0x1000,1024,4,4,32,80,4};
  for(unsigned i=0;i<9;++i){bytes[i].assign(size[i],0xcd);known[i].assign(size[i],0);regions[i]={base[i],bytes[i].data(),known[i].data(),size[i]};}context={{regions.data(),regions.size()},100000,io,this};
  put(Pad+24,interactive?0x20:0);put(Pad+4,0);put(Pad+12,0x55);put(Pad+20,0);put(0x8001ede8,0);put(0x800dcf10,0);put(0x800fc650,Roots);
  put(Resource+8,24);for(unsigned i=0;i<24;++i){put(Resource+20+i*8,0x1000+i*0x100);const auto image=Images+i*0x100;put(image,0x23);put(image+4,5+i,2);}
  const std::uint32_t t0[]={0x80048044,0x80048050,0x8004805c,0x80048068,0x80048074,0x80048080,0x8004808c,0x80048098,0x800480a4,0x800480b0};
  const std::uint32_t t1[]={0x8004810c,0x80048118,0x80048124,0x80048130,0x8004813c,0x80048148,0x80048154,0x80048160,0x8004816c,0x80048178};
  for(unsigned i=0;i<10;++i){put(0x800260e4+i*4,t0[i]);put(0x8002610c+i*4,t1[i]);}
  for(unsigned i=0;i<8;++i){put(Roots+i*4,Records+i*64);put(Records+i*64+32,PlayerData+i*64);put(PlayerData+i*64+9,(i==0?200:i==1?255:i==2?0:i==3?17:i==4?145:50),1);}
 }
 std::pair<std::uint8_t*,std::uint8_t*> at(std::uint32_t a){for(auto&r:regions)if(a>=r.base&&a-r.base<r.size)return{r.data+a-r.base,r.known+a-r.base};throw std::runtime_error("fixture address");}
 void put(std::uint32_t a,std::uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i){auto p=at(a+i);*p.first=std::uint8_t(v>>(8*i));*p.second=1;}}
 std::uint32_t get(std::uint32_t a,unsigned n=4){std::uint32_t v=0;for(unsigned i=0;i<n;++i){auto p=at(a+i);require(*p.second==1,"known fixture read");v|=std::uint32_t(*p.first)<<(i*8);}return v;}
 static int io(void* u,const Nba97GameTextMemory*,const Nba97CourtInteractiveEvent* e,Nba97CourtInteractiveValue* v){auto&f=*static_cast<Fixture*>(u);if(f.refuse)return 0;
  if(e->entry==0x8008f224){if(f.cursor>=f.input.size()){std::cerr<<"input exhausted at "<<std::hex<<e->pc<<std::dec<<'\n';return 0;}*v={f.input[f.cursor++],1};}
  else if(e->entry==0x80029bfc)*v={Resource,1};
  else if(e->entry==0x800946b8){f.uploads.push_back({e->argument[0],e->argument[1],e->argument[2],e->argument[3],e->argument[4]});f.put(e->argument[0]+4,7,2);}
  return 1;}
 void frame(std::array<std::uint32_t,8> buttons){input.push_back(0);input.insert(input.end(),buttons.begin(),buttons.end());}
 int run(std::size_t cap=2000){return nba97_game_court_interactive(&context,journal.data(),cap,&progress);}
};
void ordinary(){Fixture f(false);require(f.run()==1&&f.progress.completed&&!f.progress.interactive_entered,"ordinary reset path");require(f.progress.stores==5&&f.progress.services_completed==0,"exact ordinary events");require(f.get(Pad+4)==0xfffffdffu&&f.get(Pad+16)==0&&f.get(Pad+20)==0&&f.get(Pad+24)==0,"all source scratch resets");
 Fixture wrong;wrong.input={0};require(wrong.run()==1&&wrong.cursor==1&&!wrong.progress.interactive_entered,"real initial input selects ordinary path");}
void interactive(){Fixture f;f.input={0xe75};f.frame({0x80,1,2,0x200,0x1f20,0x1000,0x3e1a,0});f.frame({0x100,0,2,0,0x3b20,0,0,0});f.frame({0x10,0,0,0,0,0,0,0});f.frame({0x40,0,0,0,0,0,0,0});f.input.push_back(0x820);
 const auto status=f.run();if(status!=1)std::cerr<<"interactive status "<<status<<" pc "<<std::hex<<f.progress.stopped_pc<<" at "<<f.progress.stopped_address<<std::dec<<" events "<<f.progress.events<<" input "<<f.cursor<<'/'<<f.input.size()<<'\n';require(status==1&&f.progress.completed&&f.progress.interactive_entered&&f.cursor==f.input.size(),"explicit four-frame interactive run");
 require(f.progress.frames_completed==4&&f.progress.players_completed==32&&f.progress.loaded_resource==Resource,"source loop counts");
 require(f.get(0x800dcf10)==0&&f.get(Pad+20)==0&&!(f.get(Pad+4)&0x800),"special selectors set then cleared");
 require(f.get(Pad+12)&2&&!(f.get(Pad+12)&4),"set and conditional-clear player bits");
 require(f.get(PlayerData+3*64+9,1)==18&&f.get(PlayerData+5*64+9,1)==49,"increment wrap/clamp and decrement");
 require(f.get(PlayerData+9,1)==144&&f.get(PlayerData+64+9,1)==144&&f.get(PlayerData+2*64+9,1)==18,"post-render clamping");
 require(f.get(0x800faba4+6*4)==2,"source3E1A player marker");require(f.uploads.size()>100,"all portraits/digits/highlights rendered");
 require(f.uploads[0][0]==Images+9*0x100&&f.uploads[0][1]==16&&f.uploads[0][2]==68,"first portrait placement");
 bool saw_fixed_hundred=false;for(auto&u:f.uploads)if(u[0]==Images&&u[1]==196)saw_fixed_hundred=true;require(saw_fixed_hundred,"raw200 still draws fixed hundreds glyph1");
 for(unsigned i=0;i<24;++i)require(f.get(Images+i*0x100)==0x23,"header high bytes cleared, low byte retained");
 Fixture control;control.input={0xe75,0,0};control.put(0x800260e4,0x8004810c);require(control.run()==NBA97_COURT_INTERACTIVE_CONTROL_TARGET&&control.progress.stopped_pc==0x8004803c,"cross-table control target refuses explicitly");
}
void refusal(){Fixture base;base.input={0xe75,0x820};require(base.run()==1,"event baseline");for(std::size_t cap=0;cap<base.progress.events;++cap){Fixture f;f.input={0xe75,0x820};require(f.run(cap)==NBA97_TEXT_LIMIT&&f.progress.events==cap,"every event cutoff");for(std::size_t i=0;i<cap;++i)require(f.journal[i].pc==base.journal[i].pc&&f.journal[i].kind==base.journal[i].kind&&f.journal[i].completed==base.journal[i].completed,"event prefix order");}
 Fixture missing;missing.context.io=nullptr;require(missing.run()==NBA97_TEXT_IO_REFUSED,"missing actual input refuses");Fixture refused;refused.refuse=true;require(refused.run()==NBA97_TEXT_IO_REFUSED,"service refusal propagates");Fixture unknown;*unknown.at(Pad+24).second=0;require(unknown.run()==NBA97_TEXT_UNKNOWN&&unknown.progress.events==0,"unknown route not guessed");Fixture budget;budget.context.access_budget=0;require(budget.run()==NBA97_TEXT_LIMIT,"access budget");}
}
int main(){try{ordinary();interactive();refusal();std::cout<<checks<<" court interactive checks passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
