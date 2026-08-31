#include "recovered/game_scoring_actor_ai.h"
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
unsigned checks=0,failures=0;
void check(bool value){++checks;if(!value){++failures;std::fprintf(stderr,"failed check %u\n",checks);}}

struct Memory {
    struct Event {std::uint32_t pc,address,value;unsigned width,kind;};
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x200000);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x200000,1);
    std::vector<Event> events;
    std::vector<Nba97ScoringActorAiCall> calls;
    std::size_t refuse=~std::size_t(0);bool services=true;
    void put(std::uint32_t address,std::uint32_t value,unsigned width=4){
        for(unsigned i=0;i<width;++i){bytes[address-0x80000000u+i]=
            static_cast<std::uint8_t>(value>>(i*8));known[address-0x80000000u+i]=1;}}
    std::uint32_t get(std::uint32_t address,unsigned width=4)const{
        std::uint32_t value=0;for(unsigned i=0;i<width;++i)
            value|=std::uint32_t(bytes[address-0x80000000u+i])<<(i*8);
        return value;}
    static int access(void* user,std::uint32_t pc,std::uint32_t address,unsigned width,
                      unsigned kind,Nba97PlayerFrameValue* value){
        auto& m=*static_cast<Memory*>(user);
        if(address<0x80000000u||std::uint64_t(address)+width>0x80200000ull)
            return NBA97_BODY_BOUNDS;
        if((width==4&&(address&3u))||(width==2&&(address&1u)))
            return NBA97_BODY_ALIGNMENT_TRAP;
        if(m.events.size()==m.refuse)return NBA97_BODY_BOUNDS;
        if(kind==NBA97_FRAME_READ){*value={};for(unsigned i=0;i<width;++i)
            if(m.known[address-0x80000000u+i]){value->known_mask|=static_cast<std::uint8_t>(1u<<i);
                value->word|=std::uint32_t(m.bytes[address-0x80000000u+i])<<(i*8);}}
        else for(unsigned i=0;i<width;++i){m.known[address-0x80000000u+i]=
            static_cast<std::uint8_t>((value->known_mask>>i)&1u);
            if(m.known[address-0x80000000u+i])m.bytes[address-0x80000000u+i]=
                static_cast<std::uint8_t>(value->word>>(i*8));}
        m.events.push_back({pc,address,value->word,width,kind});return NBA97_BODY_OK;
    }
    static int service(void* user,const Nba97ScoringActorAiCall* call){
        auto& m=*static_cast<Memory*>(user);if(!m.services)return NBA97_SCORING_ACTOR_AI_SERVICE_REQUIRED;
        m.calls.push_back(*call);return NBA97_BODY_OK;
    }
    bool saw(std::uint32_t pc,std::uint32_t address)const{
        for(const auto& event:events)if(event.pc==pc&&event.address==address)return true;
        return false;}
    Nba97ScoringActorAiContext context(){return {access,service,this,100000};}
};

constexpr std::uint32_t TEAM=0x8001edf4,OTHER_TEAM=0x8001eeb8;
constexpr std::uint32_t BALL=0x80130000,PHYSICAL=0x80110000;
constexpr std::uint32_t SCOREBOARD=0x80150000,OPP_SCOREBOARD=0x80150100;
std::uint32_t player(unsigned i){return 0x80120000u+i*0x100u;}
std::uint32_t stats(unsigned i){return 0x80140000u+i*0x40u;}

void setup(Memory& m,unsigned mode=2){
    m.put(0x800fdc48,BALL);m.put(BALL+8,0);m.put(BALL+0xb4,0,2);
    m.put(0x8001ee04,0);m.put(0x800fdbbe,0,2);m.put(0x800fdba4,0);
    m.put(0x800fe8cc,0);m.put(0x800fe8e4,0,2);m.put(0x800fe8ca,0,2);
    m.put(TEAM+4,player(0));m.put(TEAM+0x14,0,2);m.put(TEAM+0x52,0,2);
    m.put(TEAM+0x54,0xffff,2);m.put(TEAM+0x56,0xffff,2);
    m.put(TEAM+0x58,0xffff,2);m.put(TEAM+0x48,0);m.put(TEAM+0x4c,0);
    m.put(OTHER_TEAM+4,player(5));m.put(OTHER_TEAM+0x14,5,2);
    m.put(0x800fdb58,100);m.put(0x800fdb94,0,2);m.put(0x800fdbd8,mode,2);
    m.put(0x800fdbda,0,2);m.put(0x800fdbea,1,2);m.put(0x800fe8bc,0,2);
    m.put(0x80020bec,PHYSICAL);m.put(PHYSICAL,0);m.put(PHYSICAL+4,0xffff,2);
    for(unsigned i=0;i<5;++i){m.put(PHYSICAL+i*0xf4u+0xde,9,1);m.put(PHYSICAL+i*0xf4u+0xdf,9,1);}
    for(unsigned i=0;i<8;++i){m.put(0x800fdc50u+i*4u,player(i));
        m.put(0x800fdc70u+i*4u,stats(i));m.put(player(i)+0x24,i+20,2);
        m.put(player(i)+0x26,0,2);m.put(player(i)+0x1a,0,2);
        m.put(player(i)+0x1c,0,2);}
    m.put(player(0)+0x14,0,2);m.put(player(0)+0x44,0x8004,2);
    m.put(player(0)+0x46,1,2);m.put(0x8001ee0a,77,2);m.put(0x8001eec4,88,2);
    m.put(0x800fdc40,SCOREBOARD);m.put(SCOREBOARD+4,OPP_SCOREBOARD);
    m.put(SCOREBOARD+0x2e,10,2);m.put(OPP_SCOREBOARD+0x2e,7,2);
}

int run(Memory& m,Nba97ScoringActorAiProgress& progress,std::uint32_t& selected){
    auto context=m.context();return nba97_game_scoring_actor_ai(&context,&selected,&progress);
}
}

int main(){
    {Memory m;setup(m,2);Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;
        check(run(m,p,selected)==NBA97_BODY_OK);check(p.completed&&selected==TEAM);
        check(m.get(BALL+0xb4,2)==30&&m.get(0x800fdbbe,2)==1);
        check(m.get(player(0)+0x44,2)==0&&m.get(player(0)+0x46,2)==4);
        check(m.get(TEAM+0x44,2)==2);
        check(m.get(stats(0)+2,2)==1);check(m.get(SCOREBOARD+0xa4,2)==3);
        check(m.get(OPP_SCOREBOARD+0xa4,2)==0xfffd);check(p.cpu_leaves==1);
        check(m.calls.size()==1&&m.calls[0].entry==0x8007f074&&m.calls[0].argument_count==3);}
    {Memory m;setup(m,1);m.put(0x800fdbda,1,2);m.put(stats(0)+8,998,2);
        Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;check(run(m,p,selected)==NBA97_BODY_OK);
        check(m.get(stats(0)+0xa,2)==1);check(m.get(TEAM+0x2e,2)==1);
        check(p.services==0&&p.cpu_leaves==1&&p.players_visited==8);}
    {Memory m;setup(m,3);m.put(0x800fdbda,1,2);m.put(TEAM+0x54,1,2);
        Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;check(run(m,p,selected)==NBA97_BODY_OK);
        check(m.get(stats(0)+2,2)==1&&m.get(stats(0)+6,2)==1);
        check(m.get(player(1)+2,2)==1&&m.get(player(1)+6,2)==1);
        check(m.get(player(1)+0x1a,2)==3);check(p.cpu_leaves==2&&p.players_visited==16);}
    {Memory m;setup(m,2);m.put(0x800fdbea,0,2);m.put(PHYSICAL+4,2,2);
        Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;check(run(m,p,selected)==NBA97_BODY_OK);
        check(m.get(0x800fdbea,2)==1);check(m.get(stats(0),2)==1&&m.get(stats(0)+4,2)==0);
        check(m.get(player(2),2)==1&&m.get(player(2)+4,2)==0);check(p.cpu_leaves==2);}
    {Memory m;setup(m,2);m.put(BALL+8,0x80000000u);m.put(OTHER_TEAM+4,player(5));
        m.put(player(5)+0x14,0,2);m.put(OTHER_TEAM+0x52,0,2);m.put(OTHER_TEAM+0x54,0xffff,2);
        m.put(OTHER_TEAM+0x56,0xffff,2);m.put(OTHER_TEAM+0x58,0xffff,2);
        Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;check(run(m,p,selected)==NBA97_BODY_OK);
        check(selected==OTHER_TEAM);}
    {Memory m;setup(m,2);m.put(0x800fe8cc,10);m.put(0x800fe8e4,0xffff,2);
        Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;check(run(m,p,selected)==NBA97_BODY_OK);
        check(!m.calls.empty()&&m.calls.front().entry==0x80056ffc);
        check(m.calls.front().argument[0]==PHYSICAL&&m.calls.front().argument[1]==1);
        check(m.get(0x800fe8cc,2)==0);}
    {Memory m;setup(m,2);m.put(TEAM+0x56,1,2);m.put(TEAM+0x58,2,2);
        m.put(TEAM+0x14,0,2);Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;
        check(run(m,p,selected)==NBA97_BODY_OK);check(m.get(stats(1)+0x10,2)==1);
        check(m.get(player(2)+0x10,2)==1);check(m.calls.back().entry==0x8007f20c);
        check(m.calls.back().argument[0]==0&&m.calls.back().argument[1]==1);}
    {Memory m;setup(m,2);m.services=false;Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;
        check(run(m,p,selected)==NBA97_SCORING_ACTOR_AI_SERVICE_REQUIRED);
        check(p.stopped_pc==0x8006ea14&&p.stopped_entry==0x8007f074&&p.stores>0);}
    {Memory m;setup(m,2);m.known[0x800fe8cc-0x80000000u]=0;
        Nba97ScoringActorAiProgress p{};std::uint32_t selected=7;check(run(m,p,selected)==NBA97_BODY_UNKNOWN);
        check(selected==0&&p.stopped_pc==0x8006e7b0&&p.stores==0);}
    {Memory full;setup(full,3);full.put(0x800fdbda,1,2);full.put(TEAM+0x54,1,2);
        Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;check(run(full,p,selected)==NBA97_BODY_OK);
        for(std::size_t i=0;i<full.events.size();++i){Memory m;setup(m,3);m.put(0x800fdbda,1,2);
            m.put(TEAM+0x54,1,2);m.refuse=i;auto context=m.context();
            check(nba97_game_scoring_actor_ai(&context,&selected,&p)==NBA97_BODY_BOUNDS);
            check(m.events.size()==i);}}
    {Memory m;setup(m,2);auto context=m.context();context.operation_budget=2;
        Nba97ScoringActorAiProgress p{};std::uint32_t selected=0;
        check(nba97_game_scoring_actor_ai(&context,&selected,&p)==NBA97_BODY_JOURNAL_LIMIT);
        check(p.operations==2&&p.stopped_pc==0x8006e7e4&&m.events.size()==2);}
    std::printf("%u checks, %u failures\n",checks,failures);return failures?1:0;
}
