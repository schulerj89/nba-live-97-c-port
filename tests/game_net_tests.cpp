#include "game_net.hpp"
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
    nba97::GameNetGeometry geometry;
    unsigned modeReads=0;bool switchMode=false;std::size_t refuseAt=~std::size_t(0);
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
            if(a==0x800c55c0){++m.modeReads;if(m.switchMode&&m.modeReads==1)m.put(a,2,1);}
        }else{
            for(unsigned i=0;i<n;++i){m.known[off+i]=static_cast<std::uint8_t>((v->known_mask>>i)&1);if(m.known[off+i])m.bytes[off+i]=static_cast<std::uint8_t>(v->word>>(i*8));}
        }
        m.events.push_back({pc,a,v->word,n,kind});return NBA97_BODY_OK;
    }
    Nba97PlayerFrameContext context(){return {access,nullptr,nullptr,this,100000};}
    static int math(void* u,const Nba97PlayerMathRequest* q,Nba97GamePeriodValue* v){return static_cast<Memory*>(u)->geometry.apply(*q,*v);}
    Nba97PlayerFrameContext drawContext(){return {access,math,nullptr,this,1000000};}
    void netFixture(){
        // Explicit synthetic resource/camera/geometry fixture, not a loader or cold-state claim.
        put(0x80103f44,0x80120000);put(0x800288b4,0x800a46cc);
        for(unsigned i=0;i<30;++i)put(0x80120000+i*4,120);
        std::vector<unsigned> packed{0x10,0xfb,0,6,0x40};
        for(unsigned i=0;i<400;++i){packed.push_back(0xe0);for(unsigned j=0;j<4;++j)packed.push_back(0);}
        packed.push_back(0xfc);for(unsigned i=0;i<packed.size();++i)put(0x80120078+i,packed[i],1);
        for(auto a:{0x800b7a00u,0x800b7a04u,0x800b7a08u,0x800b7a0cu,0x800dcf10u,0x8001ede8u})put(a,0);
        put(0x800b72dc,1);put(0x8010b60c,1);put(0x800fcc54,3);put(0x800fc660,0x80150000);put(0x80150000,0,2);put(0x80102924,0x80160000);
        for(unsigned i=0;i<4096;++i)put(0x80160000+i*4,0x00ffffff);
        for(unsigned i=0;i<8;++i)put(0x800f9fd8+i*4,(i==0||i==2||i==4)?4096:i==7?5000:0);
        for(unsigned i=0;i<4096;++i)put(0x800b3254+i*4,0x10000000);
        for(unsigned i=0;i<15;++i)for(unsigned j=0;j<3;++j)put(0x800b731c+i*8+j*2,0,2);
        for(auto a:{0x800fa630u,0x800fa632u,0x800fa634u,0x800fa638u,0x800fa63au,0x800fa63cu,0x800fab98u,0x800fab9au,0x800fab9cu,0x801076e4u,0x801076e6u,0x801076e8u,0x80108a08u,0x80108a0au,0x80108a0cu})put(a,0,2);
        put(0x800fa634,5000,2);
        auto& r=geometry.player.root;r.offset_x={256u<<16,1};r.offset_y={120u<<16,1};r.distance={384,1};r.depth_cue_a={0,1};r.depth_cue_b={0,1};geometry.average_scale4={1024,1};
        // Packet links/XY and vertex padding genuinely start unknown.
        for(unsigned side=0;side<2;++side){for(unsigned i=0;i<96+1440+3200;++i)known[0x106444+side*0x1324+i]=0;for(unsigned i=0;i<15;++i)known[0x1063cc+side*0x1324+i*8+6]=known[0x1063cc+side*0x1324+i*8+7]=0;}
    }
    void image(std::uint32_t a=0x80120040){
        put(a,0xffffc041); // signed backward link to palette at120000
        put(a+4,0x0120,2);put(a+6,0x0221,2);put(a+12,0x2df,2);put(a+14,0x01ff,2);
        put(a-64+12,0x200,2);put(a-64+14,0xe3,2);put(0x800c55c0,0,1);
    }
};
}

int main(){
 constexpr std::uint32_t source=0x80120000,dest=0x80130000;
 auto stream=[](Memory& m,const std::vector<unsigned>& b){m.put(0x800288b4,0x800a46cc);for(unsigned i=0;i<b.size();++i)m.put(0x80120000+i,b[i],1);};
 const std::vector<unsigned> overlap{0x10,0xfb,0,0,1,0xe0,'A','B','C','D',0,0,0xfc};
 // A short declaration does NOT bound AA168; overlapping copies observe prior stores.
 {Memory m;stream(m,overlap);auto c=m.context();Nba97GameNetProgress p{};Nba97GamePeriodValue n{};
  check(nba97_game_net_decode(&c,source,dest,&n,&p)==1);check(n.word==1&&n.known&&p.decodes==1&&p.completed);
  for(unsigned i=0;i<7;++i)check(m.get(dest+i,1)==(i<4?'A'+i:'D'));
  check(m.get(dest+7,1)==0xa5);}
 // Opaque unknown literal/backreference bytes remain unknown without clearing backing bytes.
 {Memory m;stream(m,overlap);m.known[source-0x80000000+9]=0;auto c=m.context();Nba97GameNetProgress p{};Nba97GamePeriodValue n{};
  check(nba97_game_net_decode(&c,source,dest,&n,&p)==1);for(unsigned i=3;i<7;++i){check(m.known[dest-0x80000000+i]==0);check(m.get(dest+i,1)==0xa5);}}
 {Memory m;stream(m,overlap);m.known[source-0x80000000+3]=0;auto c=m.context();Nba97GameNetProgress p{};Nba97GamePeriodValue n{};
  check(nba97_game_net_decode(&c,source,dest,&n,&p)==1);check(!n.known&&n.word==0);}
 {Memory m;stream(m,overlap);m.put(source+1,0,1);m.known[0x288b4]=0;auto c=m.context();Nba97GameNetProgress p{};Nba97GamePeriodValue n{};
  check(nba97_game_net_decode(&c,source,dest,&n,&p)==1);check(n.known&&n.word==0&&p.decodes==0&&p.stores==0);}
 {Memory m;stream(m,overlap);m.put(0x800288b4,0x800a46e4);auto c=m.context();Nba97GameNetProgress p{};Nba97GamePeriodValue n{};
  check(nba97_game_net_decode(&c,source,dest,&n,&p)==NBA97_NET_CODEC_REQUIRED);check(p.stopped_pc==0x800a46ec&&p.stores==0);}
 {Memory m;stream(m,overlap);m.known[source-0x80000000+5]=0;auto c=m.context();Nba97GameNetProgress p{};Nba97GamePeriodValue n{};
  check(nba97_game_net_decode(&c,source,dest,&n,&p)==NBA97_BODY_UNKNOWN);check(p.stopped_pc==0x800aa1c0&&p.stores==0);}
 // Every access refusal retains exactly its successful prefix, including knownness.
 {Memory baseline;stream(baseline,overlap);auto c=baseline.context();Nba97GameNetProgress p{};Nba97GamePeriodValue n{};
  check(nba97_game_net_decode(&c,source,dest,&n,&p)==1);
  for(std::size_t stop=0;stop<baseline.events.size();++stop){Memory m;stream(m,overlap);m.refuseAt=stop;auto in=m.context();Nba97GameNetProgress q{};
   check(nba97_game_net_decode(&in,source,dest,&n,&q)==NBA97_BODY_BOUNDS);check(m.events.size()==stop);check(q.stopped_pc==baseline.events[stop].pc);
   auto expected=Memory{};stream(expected,overlap);for(std::size_t j=0;j<stop;++j)if(baseline.events[j].kind)expected.put(baseline.events[j].address,baseline.events[j].value,baseline.events[j].width);
   check(m.bytes==expected.bytes&&m.known==expected.known);}}
 {Memory m;stream(m,overlap);m.known[dest-0x80000000+1]=2;auto c=m.context();Nba97GameNetProgress p{};Nba97GamePeriodValue n{};
  check(nba97_game_net_decode(&c,source,dest,&n,&p)==NBA97_BODY_ARGUMENT);check(p.stores==1&&m.get(dest,1)=='A'&&m.get(dest+1,1)==0xa5);}
 // Original in-place relocation occurs again on a repeated initialization.
 {Memory m;m.put(0x80103f44,source);for(unsigned i=0;i<30;++i)m.put(source+i*4,120+i*8);m.known[0xb7a00]=0;
  auto c=m.context();Nba97GameNetProgress p{};check(nba97_game_net_initialize(&c,&p)==NBA97_BODY_UNKNOWN);check(p.stopped_pc==0x8004b9f0);
  for(unsigned i=0;i<30;++i)check(m.get(source+i*4)==source+120+i*8);
  check(m.get(0x801064a7,1)==3&&m.get(0x801064ab,1)==0x40&&m.get(0x801064a8,1)==181);
  check(nba97_game_net_initialize(&c,&p)==NBA97_BODY_UNKNOWN);for(unsigned i=0;i<30;++i)check(m.get(source+i*4)==source+source+120+i*8);}
 {Memory m;auto c=m.context();Nba97GameNetProgress p{};m.put(0x800b72dc,0);check(nba97_game_net_frame(&c,&p)==NBA97_FRAME_MATH_REQUIRED);check(p.stopped_pc==0x80055f2c);}
 {nba97::GameNetGeometry g;Nba97GamePeriodValue out{};Nba97PlayerMathRequest q{};q.kind=NBA97_NET_AVERAGE_FOUR;
  check(g.apply(q,out)==NBA97_BODY_UNKNOWN);g.average_scale4={1024,1};for(auto& d:g.player.root.depth)d={4000,1};
  check(g.apply(q,out)==1);check(g.player.order_depth.word==4000&&g.player.root.mac0.word==16384000);
  g.average_scale4={0xfffffc00,1};check(g.apply(q,out)==1);check(g.player.order_depth.word==0&&g.player.root.vector.flags.word!=0);
  g.player.root.depth[3]={65536,1};check(g.apply(q,out)==NBA97_BODY_ARGUMENT);}

 {Memory m;m.netFixture();auto c=m.drawContext();Nba97GameNetProgress p{};
  check(nba97_game_net_frame(&c,&p)==1);check(p.initializations==1&&p.links==286&&p.decodes==2);check(m.get(0x800b72dc)==0&&m.get(0x80103fa8)==100);
  for(unsigned side=0;side<2;++side)for(unsigned i=96;i<100;++i){const auto a=0x106a44+side*0x1324+i*16;check(m.known[a]==0&&m.known[a+8]==0&&m.known[a+12]==0);check(m.known[a+3]&&m.known[a+7]);}
  m.put(0x800b7a00,29);m.put(0x800b7a04,29);m.events.clear();check(nba97_game_net_frame(&c,&p)==1);check(p.initializations==0&&p.decodes==2);
  check(m.get(0x800b7a00)==0&&m.get(0x800b7a04)==0);check(m.get(0x801076e6,2)==0x800&&m.get(0x80108a0a,2)==0);
  auto firstDecode=m.events.size();auto lastPacket=std::size_t(0);for(std::size_t i=0;i<m.events.size();++i){auto e=m.events[i];if(e.kind&&e.address==0x800fb998&&firstDecode==m.events.size())firstDecode=i;if(e.kind&&e.pc==0x80055fb8)lastPacket=i;}check(lastPacket<firstDecode);
  m.put(0x80150000,1,2);m.events.clear();check(nba97_game_net_frame(&c,&p)==1);check(p.decodes==2&&m.get(0x800b7a00)==0);
  m.put(0x800b7a08,1);check(nba97_game_net_frame(&c,&p)==1);check(m.get(0x800b7a08)==1&&m.get(0x800fb99a,2)==0xffec);
  m.put(0x80150000,0,2);check(nba97_game_net_frame(&c,&p)==1);check(m.get(0x800b7a08)==0);
 }
 // Noncanonical metadata on a reached write refuses without destroying its prefix.
 {Memory m;m.netFixture();m.known[0x120004]=2;auto c=m.drawContext();Nba97GameNetProgress p{};check(nba97_game_net_frame(&c,&p)==NBA97_BODY_ARGUMENT);check(p.stores==1&&m.get(0x80120000)==0x80120078&&m.get(0x80120004)==120);}
 {Memory m;m.netFixture();nba97::GameNet net;net.memory=m.context();net.geometry=m.geometry;Nba97GameNetProgress p{};
  check(net.frame(1000000,p)==1);check(p.links==286&&net.geometry.player.root.depth[3].known);
  check(net.draw(0,p)==NBA97_BODY_JOURNAL_LIMIT&&p.stopped_pc==0x80055f18);net.memory={};check(net.frame(1,p)==NBA97_BODY_ARGUMENT);}
 std::printf("%u checks, %u failures\n",checks,failures);return failures?1:0;
}
