#include "recovered/game_player_marker_resources.h"
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
    Nba97PlayerMarkerContext context(){return {access,nullptr,this,100000};}
    void image(std::uint32_t a=0x80120040){
        put(a,0xffffc041); // signed backward link to palette at120000
        put(a+4,0x0120,2);put(a+6,0x0221,2);put(a+12,0x2df,2);put(a+14,0x01ff,2);
        put(a-64+12,0x200,2);put(a-64+14,0xe3,2);put(0x800c55c0,0,1);
    }
};
}
int main(){
    constexpr std::uint32_t image=0x80120040,packet=0x80130000;
    for(unsigned reflected=0;reflected<2;++reflected){
        Memory m;m.image();for(unsigned i=0;i<40;++i)m.known[packet-0x80000000+i]=0;
        auto c=m.context();Nba97PlayerMarkerProgress p{};
        check(nba97_game_player_marker_packet(&c,packet,image,reflected,&p)==NBA97_BODY_OK);
        check(p.completed&&p.packets==1&&m.modeReads==2);check(m.get(packet+3,1)==9);check(m.get(packet+7,1)==(reflected?0x2eu:0x2cu));
        check(m.get(packet+12,1)==62);check(m.get(packet+20,1)==93);check(m.get(packet+13,1)==(reflected?31u:255u));
        check(m.get(packet+29,1)==(reflected?255u:31u));check(m.get(packet+14,2)==0x38e0);
        check(m.get(packet+22,2)==(reflected?0xfbu:0x9bu));
        for(unsigned i: {0u,1u,2u,8u,9u,10u,11u,16u,17u,18u,19u,24u,25u,26u,27u,32u,33u,34u,35u,38u,39u})check(m.known[packet-0x80000000+i]==0);
    }
    {Memory m;m.image();m.switchMode=true;auto c=m.context();Nba97PlayerMarkerProgress p{};
     check(nba97_game_player_marker_packet(&c,packet,image,0,&p)==1);check(m.modeReads==2);check(m.get(packet+22,2)==0x22b);}
    {Memory m;m.image();m.put(0x800c55c0,1,1);auto c=m.context();Nba97PlayerMarkerProgress p{};
     check(nba97_game_player_marker_packet(&c,packet,image,0,&p)==1);check(m.modeReads==1);}
    {Memory m;m.image();m.put(image,0x41);auto c=m.context();Nba97PlayerMarkerProgress p{};
     check(nba97_game_player_marker_packet(&c,packet,image,0,&p)==NBA97_BODY_BOUNDS);check(p.stopped_pc==0x80050f5c&&p.stopped_address==12);
     check(m.get(packet+7,1)==0x2c&&m.get(packet+4,1)==128);check(m.get(packet+14,2)==0xa5a5);}
    {Memory m;m.image();m.known[image-0x80000000+4]=0;auto c=m.context();Nba97PlayerMarkerProgress p{};
     check(nba97_game_player_marker_packet(&c,packet,image,0,&p)==NBA97_BODY_UNKNOWN);check(p.stopped_pc==0x80050eac);
     check(m.get(packet+12,1)==62&&m.get(packet+13,1)==255);check(m.get(packet+21,1)==0xa5);}
    {Memory m;m.image();m.known[packet-0x80000000+3]=2;auto c=m.context();Nba97PlayerMarkerProgress p{};
     check(nba97_game_player_marker_packet(&c,packet,image,0,&p)==NBA97_BODY_ARGUMENT);check(p.stores==0);}
    for(unsigned n:{16u,32u,528u})for(int delta:{-12,0,4,16,32,600}){
        Memory m;auto src=0x80120000u,dst=src+delta;for(unsigned i=0;i<1200;++i)m.put(src-32+i,i*37,1);
        std::vector<std::uint8_t> expected(n);for(unsigned i=0;i<n;++i)expected[i]=static_cast<std::uint8_t>(m.get(src+i,1));
        m.known[src-0x80000000+7]=0;auto c=m.context();Nba97PlayerMarkerProgress p{};
        check(nba97_game_player_marker_copy(&c,src,dst,n,&p)==1);check(p.copies==1);
        for(unsigned i=0;i<n;++i){check(m.known[dst-0x80000000+i]==(i==7?0:1));if(i!=7)check(m.get(dst+i,1)==expected[i]);}
    }
    {Memory m;auto c=m.context();Nba97PlayerMarkerProgress p{};
     check(nba97_game_player_marker_copy(&c,0x7ffffff8,0x7ffffffc,16,&p)==NBA97_FRAME_ARITHMETIC_TRAP);check(p.stopped_pc==0x800aa65c);
     check(nba97_game_player_marker_copy(&c,image+1,packet,16,&p)==NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT);
     check(nba97_game_player_marker_copy(&c,image,packet,40,&p)==NBA97_BODY_ARGUMENT);
     check(nba97_game_player_marker_resources(&c,&p)==NBA97_MARKER_IO_REQUIRED);check(p.stopped_pc==0x8004d4c0&&p.stores==0);}
    {Memory m;auto c=m.context();Nba97PlayerMarkerProgress p{};
     // Explicit loader boundary fixture: it returns a mapped test resource,
     // without claiming allocator/loader coverage. The second load refuses.
     c.io=[](void*,const Nba97PlayerMarkerCall* q,Nba97GamePeriodValue* v){
         if(q->pc!=0x8004d4c0)return int(NBA97_BODY_BOUNDS);
         v->word=0x80120000;v->known=1;return int(NBA97_BODY_OK);};
     m.known[0xdce04]=m.known[0xdce05]=m.known[0xdce06]=m.known[0xdce07]=0;
     check(nba97_game_player_marker_resources(&c,&p)==NBA97_BODY_BOUNDS);
     check(p.stopped_pc==0x8004d4d8&&p.stores==1&&p.calls==1);check(m.get(0x800dce04)==0x80120000);
     for(unsigned i=0;i<4;++i)check(m.known[0xdce04+i]==1);}
    {Memory m;auto c=m.context();Nba97PlayerMarkerProgress p{};
     m.put(0x800dce04,0x80120000);m.put(0x80120008,0);m.known[0x26134]=0;
     check(nba97_game_player_marker_arrows(&c,&p)==NBA97_BODY_UNKNOWN);
     check(p.stopped_pc==0x800a547c&&p.stopped_address==0x80026134&&p.calls==0);}
    {Memory m;auto c=m.context();Nba97PlayerMarkerProgress p{};
     m.put(0x800dce04,0x80120000);m.put(0x80120008,1);m.put(0x80120010,0);m.put(0x80026134,1);
     m.known[0x120018]=0;check(nba97_game_player_marker_arrows(&c,&p)==NBA97_BODY_UNKNOWN);
     check(p.stopped_pc==0x800a548c&&p.stopped_address==0x80120018&&p.calls==0);}
    {Memory m;auto c=m.context();Nba97PlayerMarkerProgress p{};
     m.known[0x120000]=0;m.known[0x120003]=2;
     check(nba97_game_player_marker_copy(&c,0x80120000,0x80130000,16,&p)==NBA97_BODY_ARGUMENT);check(p.stores==0);
     m.known[0x120003]=1;m.known[0x130007]=2;
     check(nba97_game_player_marker_copy(&c,0x80120000,0x80130000,16,&p)==NBA97_BODY_ARGUMENT);
     check(p.stores==1&&p.stopped_address==0x80130004);check(m.known[0x130000]==0&&m.known[0x130001]==1);}
    // Check refusal at EVERY reached leaf access retains exactly its prior writes.
    {Memory base;base.image();auto c=base.context();Nba97PlayerMarkerProgress p{};check(nba97_game_player_marker_packet(&c,packet,image,0,&p)==1);
     for(std::size_t cut=0;cut<base.events.size();++cut){Memory m;m.image();m.refuseAt=cut;auto x=m.context();Nba97PlayerMarkerProgress q{};
       check(nba97_game_player_marker_packet(&x,packet,image,0,&q)==NBA97_BODY_BOUNDS);check(m.events.size()==cut);
       for(std::size_t i=0;i<cut;++i)check(m.events[i].pc==base.events[i].pc&&m.events[i].address==base.events[i].address&&m.events[i].value==base.events[i].value);}}
    std::printf("%u player marker checks, %u failures\n",checks,failures);return failures?1:0;
}
