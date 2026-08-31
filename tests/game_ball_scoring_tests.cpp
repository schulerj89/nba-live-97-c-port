#include "recovered/game_ball_scoring.h"
#include <cstdint>
#include <cstdio>
#include <vector>
namespace {
unsigned checks=0,failures=0;void check(bool v){++checks;if(!v){++failures;std::fprintf(stderr,"failed check %u\n",checks);}}
struct Memory {
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000);
 std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x200000,1);
 struct Event{std::uint32_t pc,a,v;unsigned n,k;};std::vector<Event> events;
 std::vector<Nba97BallScoringCall> calls;
 std::size_t refuse=~std::size_t(0);bool services=false;
 void put(std::uint32_t a,std::uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i){bytes[a-0x80000000+i]=static_cast<std::uint8_t>(v>>(i*8));known[a-0x80000000+i]=1;}}
 std::uint32_t get(std::uint32_t a,unsigned n=4)const{std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint32_t(bytes[a-0x80000000+i])<<(i*8);return v;}
 static int access(void* u,std::uint32_t pc,std::uint32_t a,unsigned n,unsigned k,Nba97PlayerFrameValue* v){auto&m=*static_cast<Memory*>(u);if(a<0x80000000||std::uint64_t(a)+n>0x80200000)return NBA97_BODY_BOUNDS;if((n==4&&(a&3))||(n==2&&(a&1)))return NBA97_BODY_ALIGNMENT_TRAP;for(unsigned i=0;i<n;++i)if(m.known[a-0x80000000+i]>1)return NBA97_BODY_ARGUMENT;if(m.events.size()==m.refuse)return NBA97_BODY_BOUNDS;if(k==NBA97_FRAME_READ){*v={};for(unsigned i=0;i<n;++i)if(m.known[a-0x80000000+i]){v->known_mask|=static_cast<std::uint8_t>(1u<<i);v->word|=std::uint32_t(m.bytes[a-0x80000000+i])<<(i*8);}}else for(unsigned i=0;i<n;++i){m.known[a-0x80000000+i]=static_cast<std::uint8_t>((v->known_mask>>i)&1);if(m.known[a-0x80000000+i])m.bytes[a-0x80000000+i]=static_cast<std::uint8_t>(v->word>>(i*8));}m.events.push_back({pc,a,v->word,n,k});return NBA97_BODY_OK;}
 static int service(void* u,const Nba97BallScoringCall* q,Nba97PlayerFrameValue* v){auto&m=*static_cast<Memory*>(u);if(!m.services)return NBA97_BALL_SCORING_SERVICE_REQUIRED;m.calls.push_back(*q);if(q->entry==0x8006e7ac){v->word=0x80120000;v->known_mask=15;}else if(q->return_bytes){v->word=0;v->known_mask=static_cast<std::uint8_t>((1u<<q->return_bytes)-1);}return NBA97_BODY_OK;}
 bool saw(std::uint32_t pc,std::uint32_t a)const{for(const auto&e:events)if(e.pc==pc&&e.a==a)return true;return false;}
 Nba97BallScoringContext context(){return {access,service,this,100000};}
};
constexpr std::uint32_t BALL=0x80130000;
void setup(Memory&m){m.put(0x800fdc48,BALL);m.put(BALL+0x18,static_cast<unsigned>(-100),2);m.put(BALL+0x10,84u<<8);m.put(BALL+0x2c,76u<<8);m.put(0x800fe8c2,0,2);m.put(0x800fdbe8,0,2);m.put(0x800fdb90,0,2);for(unsigned i=0;i<244;++i)if(i<8||i>=20)m.put(BALL+i,m.get(BALL+i,1),1);}
}
int main(){
 {Memory m;setup(m);m.put(BALL+0x18,1,2);auto c=m.context();Nba97BallScoringProgress p{};check(nba97_game_ball_scoring(&c,BALL,&p)==1);check(p.completed&&p.stores==0&&m.events.size()==1);}
 for(int x:{-100,100}){Memory m;setup(m);m.put(BALL+8,static_cast<unsigned>(x));m.put(BALL+0x10,80u<<8);m.put(BALL+0x2c,80u<<8);m.put(0x800fdb90,0x82,2);m.put(0x800fe884,0,2);auto c=m.context();Nba97BallScoringProgress p{};check(nba97_game_ball_scoring(&c,BALL,&p)==1);check(m.get(BALL+12)==0);check(m.get(BALL+8)==(x<0?0xfffeb200u:0x14e00u));}
 {Memory m;setup(m);m.put(BALL+8,0x14e00);m.put(BALL+0x24,0x14e00);m.put(BALL+0x10,84u<<8);m.put(BALL+0x2c,84u<<8);auto c=m.context();Nba97BallScoringProgress p{};check(nba97_game_ball_scoring(&c,BALL,&p)==NBA97_BODY_OK);check(p.completed&&p.stores==1&&p.services==0);check(!m.saw(0x8006dd30,0x800fdbe8)&&m.saw(0x8006dd9c,0x800fdbe8));}
 {Memory m;setup(m);m.services=true;m.put(BALL+8,0xfffeb200);m.put(BALL+0x24,0xfffeb200);m.put(BALL+0x10,80u<<8);m.put(BALL+0x2c,84u<<8);m.put(0x800fdbd8,0);m.put(0x8001ee04,0);m.put(0x800fdbf4,0x11112222);m.put(0x800fdbf8,0x33334444);m.put(0x8001eec8,0x55556666);m.put(0x800b2048,0x80110000);m.put(0x8001ee22,20,2);m.put(0x8001eee6,0,2);m.put(0x800fdb58,0xe10);auto c=m.context();Nba97BallScoringProgress p{};check(nba97_game_ball_scoring(&c,BALL,&p)==NBA97_BODY_OK);check(p.completed&&m.saw(0x8006e440,0x8001ee04)&&m.saw(0x80058484,0x800fdbf4)&&m.saw(0x8005848c,0x800fdbf8));bool query=false,wrapped=false;for(const auto&q:m.calls){if(q.entry==0x800583fc)query=q.count==3&&q.return_bytes==1&&q.argument[0]==0x11112222&&q.argument[1]==0x33334444&&q.argument[2]==0x55556666;if(q.entry==0x8004c374)wrapped=q.pc==0x8002d364;}check(query);check(wrapped);}
 {Memory m;setup(m);m.put(BALL+8,0x15200);m.put(BALL+0x24,0x15200);m.put(BALL+0x2c,84u<<8);m.put(BALL+12,0);m.put(BALL+0x28,0);m.put(0x800b8ca6,1,1);auto c=m.context();Nba97BallScoringProgress p{};check(nba97_game_ball_scoring(&c,BALL,&p)==NBA97_BALL_SCORING_SERVICE_REQUIRED);check(p.stopped_pc==0x8006e4d4||p.stopped_pc==0x8006e028||p.stopped_pc==0x8002d364);check(p.stores>0);}
 {Memory m;setup(m);m.known[BALL-0x80000000+0x18]=0;auto c=m.context();Nba97BallScoringProgress p{};check(nba97_game_ball_scoring(&c,BALL,&p)==NBA97_BODY_UNKNOWN);check(p.stores==0&&p.stopped_pc==0x8006dc3c);}
 {Memory m;setup(m);m.known[BALL-0x80000000+0x19]=2;auto c=m.context();Nba97BallScoringProgress p{};check(nba97_game_ball_scoring(&c,BALL,&p)==NBA97_BODY_ARGUMENT);check(p.stores==0);}
 {Memory full;setup(full);full.put(BALL+8,0x15200);full.put(BALL+0x24,0x15200);full.put(BALL+0x2c,84u<<8);full.put(BALL+12,0);full.put(BALL+0x28,0);full.put(0x800b8ca6,1,1);auto c=full.context();Nba97BallScoringProgress p{};nba97_game_ball_scoring(&c,BALL,&p);for(std::size_t i=0;i<full.events.size();++i){Memory m;setup(m);m.put(BALL+8,0x15200);m.put(BALL+0x24,0x15200);m.put(BALL+0x2c,84u<<8);m.put(BALL+12,0);m.put(BALL+0x28,0);m.put(0x800b8ca6,1,1);m.refuse=i;c=m.context();check(nba97_game_ball_scoring(&c,BALL,&p)==NBA97_BODY_BOUNDS);check(m.events.size()==i);}}
 std::printf("%u checks, %u failures\n",checks,failures);return failures?1:0;
}
