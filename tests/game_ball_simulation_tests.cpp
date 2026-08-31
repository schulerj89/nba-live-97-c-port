#include "recovered/game_ball_simulation.h"
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
unsigned checks=0,failures=0;
void check(bool v){++checks;if(!v){++failures;std::fprintf(stderr,"failed check %u\n",checks);}}
struct Memory {
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000,0xa5);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x200000,1);
    struct Event {std::uint32_t pc,address,value;unsigned width,kind;};
    std::vector<Event> events;
    std::size_t refuseAt=~std::size_t(0);
    std::vector<Nba97BallSimulationCall> calls;bool poison=false;
    void put(std::uint32_t a,std::uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i){bytes[a-0x80000000+i]=static_cast<std::uint8_t>(v>>(i*8));known[a-0x80000000+i]=1;}}
    std::uint32_t get(std::uint32_t a,unsigned n=4)const{std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint32_t(bytes[a-0x80000000+i])<<(i*8);return v;}
    static int access(void* u,std::uint32_t pc,std::uint32_t a,unsigned n,unsigned kind,Nba97PlayerFrameValue* v){
        auto& m=*static_cast<Memory*>(u);
        if(a<0x80000000||std::uint64_t(a)+n>0x80200000)return NBA97_BODY_BOUNDS;
        auto off=a-0x80000000;
        for(unsigned i=0;i<n;++i)if(m.known[off+i]>1)return NBA97_BODY_ARGUMENT;
        if(m.events.size()==m.refuseAt)return NBA97_BODY_BOUNDS;
        if(kind==NBA97_FRAME_READ){
            *v={};for(unsigned i=0;i<n;++i)if(m.known[off+i]){v->word|=std::uint32_t(m.bytes[off+i])<<(i*8);v->known_mask|=static_cast<std::uint8_t>(1u<<i);}
        }else{
            for(unsigned i=0;i<n;++i){m.known[off+i]=static_cast<std::uint8_t>((v->known_mask>>i)&1);if(m.known[off+i])m.bytes[off+i]=static_cast<std::uint8_t>(v->word>>(i*8));}
        }
        m.events.push_back({pc,a,v->word,n,kind});return NBA97_BODY_OK;
    }
    Nba97BallSimulationContext context(){return {access,nullptr,this,100000};}
    static int service(void* u,const Nba97BallSimulationCall* call,Nba97PlayerFrameValue*){
        auto& m=*static_cast<Memory*>(u);m.calls.push_back(*call);
        // An explicit no-effect caller fixture, not an implementation of sound.
        if(call->entry!=0x80029258)return NBA97_BALL_SIMULATION_SERVICE_REQUIRED;
        if(m.poison)m.known[0x130014]=2;
        return NBA97_BODY_OK;
    }
};
}
int main(){
 constexpr std::uint32_t ball=0x80130000;
 auto setup=[](Memory& m,int tick){
  for(unsigned i=0;i<244;++i)m.put(0x80130000+i,0,1);
  for(auto a:{0x800fe8c0u,0x800fe8c4u,0x800fe8ccu,0x800fdbd6u,0x800fdbd4u,0x800fdbb2u})m.put(a,0,2);
  m.put(0x800fdb90,0x81,2);m.put(0x800fdbcc,65535,2);m.put(0x800fdb6c,static_cast<unsigned>(tick),2);m.put(0x80021d8a,1,1);
  m.put(0x80130010,0x5c00);m.put(0x80130018,600,2);m.put(0x800fdc48,0x80130000);
  for(unsigned i=0;i<16;++i)m.put(0x800b8a54+i,i,1);
 };
 for(int tick:{-1,0,1,2,3}){Memory m;setup(m,tick);auto c=m.context();Nba97BallSimulationProgress p{};check(nba97_game_ball_simulate(&c,ball,&p)==1);
  const auto steps=tick>0?static_cast<unsigned>(tick):1u;check(p.substeps==steps);check(m.get(ball+0x18,2)==600-24*steps);check(m.get(ball+0x10)==0x5c00+600*steps-12*steps*(steps+1));check(m.get(0x800fdbc0)==0&&m.get(0x800fdbc4)==0);}
 {Memory m;setup(m,2);m.put(ball+0x10,0x410);m.put(ball+0x18,static_cast<unsigned>(-1000),2);auto c=m.context();Nba97BallSimulationProgress p{};
  check(nba97_game_ball_simulate(&c,ball,&p)==NBA97_BALL_SIMULATION_SERVICE_REQUIRED);check(p.stopped_pc==0x8006f2fc);check(m.get(ball+0x10)==0x410&&m.get(ball+0x2c)==0x410);}
 {Memory m;setup(m,1);m.put(ball+8,0x15500);m.put(ball+0x10,0x5300);m.put(ball+0x2c,0x5300);m.put(ball+0x18,10,2);auto c=m.context();Nba97BallSimulationProgress p{};Nba97GamePeriodValue v{};
  check(nba97_game_ball_backboard(&c,ball,&v,&p)==1);check(v.known&&v.word==0&&m.get(ball+0x10)==0x5400);}
 {Memory m;setup(m,1);m.known[ball-0x80000000]=0;m.known[ball-0x80000000+8]=0;auto c=m.context();Nba97BallSimulationProgress p{};
  check(nba97_game_ball_simulate(&c,ball,&p)==NBA97_BODY_UNKNOWN);check(p.stores>=3&&m.known[ball-0x80000000+0x24]==0);}
 {Memory m;setup(m,1);m.known[ball-0x80000000+0x25]=2;auto c=m.context();Nba97BallSimulationProgress p{};
  check(nba97_game_ball_simulate(&c,ball,&p)==NBA97_BODY_ARGUMENT);check(p.stores==0);}
 for(int vx:{-65,-4,-1,0,1,4,65}){Memory m;setup(m,7);m.put(ball+0x10,0x400);m.put(ball+0x18,static_cast<unsigned>(-1000),2);m.put(ball+0x14,static_cast<unsigned>(vx),2);m.put(ball+0x16,static_cast<unsigned>(vx),2);auto c=m.context();c.service=Memory::service;Nba97BallSimulationProgress p{};
  check(nba97_game_ball_simulate(&c,ball,&p)==1);check(p.substeps==1&&m.calls.size()==1);check(m.calls[0].pc==0x8006f2fc&&m.calls[0].argument[0]==0);check(m.get(ball+0x10)==0x400&&m.get(ball+0x18,2)==768);
  const auto q=vx<0?-((-vx+3)/4):vx/4;check(m.get(ball+0x14,2)==(static_cast<unsigned>(vx-(q==-1?0:q))&65535));}
 {Memory m;setup(m,1);m.put(ball+0x10,0x400);m.put(ball+0x18,0,2);m.put(ball+0x14,1,2);m.put(ball+0x16,65535,2);auto c=m.context();Nba97BallSimulationProgress p{};
  check(nba97_game_ball_simulate(&c,ball,&p)==1);check(m.get(ball+0x14,2)==0&&m.get(ball+0x16,2)==0&&m.get(ball+0x18,2)==0);}
 for(unsigned axis:{0u,1u}){Memory m;setup(m,1);m.put(ball+0x10,0x80005400);m.put(ball+0x2c,0x800053ff);m.put(ball+8,0x15501-axis);m.put(ball+0x24,0x15500);m.put(ball+12,axis);m.put(ball+0x28,0);auto c=m.context();Nba97BallSimulationProgress p{};Nba97GamePeriodValue v{};
  check(nba97_game_ball_backboard(&c,ball,&v,&p)==NBA97_FRAME_ARITHMETIC_TRAP);check(p.stopped_pc==(axis?0x8006d948u:0x8006d8fcu)&&p.stores==0&&!v.known);}
 {Memory m;setup(m,1);m.put(ball+0x10,0x400);m.put(ball+0x18,static_cast<unsigned>(-1000),2);m.poison=true;auto c=m.context();c.service=Memory::service;Nba97BallSimulationProgress p{};
  check(nba97_game_ball_simulate(&c,ball,&p)==NBA97_BODY_ARGUMENT);check(p.stopped_pc==0x8006f304&&p.stores==3&&p.services==1);check(m.get(ball+0x18,2)==static_cast<unsigned>(-1000+65536));}
 {Memory m;setup(m,1);auto c=m.context();c.operation_budget=0;Nba97BallSimulationProgress p{};check(nba97_game_ball_simulate(&c,ball,&p)==NBA97_BODY_JOURNAL_LIMIT);check(p.stores==0&&p.stopped_pc==0x8006ef64);}
 {Memory m;setup(m,1);auto c=m.context();Nba97BallSimulationProgress p{};check(nba97_game_ball_simulate(&c,ball+1,&p)==NBA97_BODY_ALIGNMENT_TRAP);check(p.stores==0&&p.stopped_pc==0x8006f03c);}
 {Memory m;setup(m,1);m.known[0x130008]=0;m.known[0x13000a]=2;auto c=m.context();Nba97BallSimulationProgress p{};check(nba97_game_ball_simulate(&c,ball,&p)==NBA97_BODY_ARGUMENT);check(p.stores==0&&p.stopped_pc==0x8006f03c);}
 // Every reached access is a refusal boundary, including stores and the final
 // projected-position publication. No later access or invented rollback occurs.
 {Memory full;setup(full,3);auto c=full.context();Nba97BallSimulationProgress p{};check(nba97_game_ball_simulate(&c,ball,&p)==1);
  for(std::size_t cap=0;cap<full.events.size();++cap){Memory m;setup(m,3);m.refuseAt=cap;c=m.context();check(nba97_game_ball_simulate(&c,ball,&p)==NBA97_BODY_BOUNDS);check(m.events.size()==cap&&p.stopped_pc==full.events[cap].pc);}
 }
 std::printf("%u checks, %u failures\n",checks,failures);return failures?1:0;
}
