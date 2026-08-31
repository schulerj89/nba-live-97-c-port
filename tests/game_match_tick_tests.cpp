#include "recovered/game_match_tick.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using U=std::uint32_t;
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Event {U pc,address,entry,a0,a1;unsigned kind,width,count;};
struct Fixture {
    std::vector<std::uint8_t> byte=std::vector<std::uint8_t>(0x200000);
    std::vector<std::uint8_t> known=std::vector<std::uint8_t>(0x200000);
    std::vector<Event> event;Nba97MatchTickProgress progress{};
    U ball_seen=0,service_fail_pc=0,input0=1;bool mutate_player=true,repeat_ai=true;
    bool mutate_live_s6=false,early_end=false;unsigned timing_calls=0,frame_calls=0,exit_after_frames=0;
    std::vector<U> timing_returns;
    bool missing_player=false,missing_ball=false,missing_net=false,missing_frame=false;
    static std::size_t offset(U address,unsigned width){
        if(address>=0x80000000u&&std::uint64_t(address)+width<=0x80200000u)return address-0x80000000u;
        throw std::out_of_range("unowned memory");
    }
    void put(U address,U value,unsigned width=4){auto at=offset(address,width);for(unsigned i=0;i<width;++i){byte[at+i]=std::uint8_t(value>>(i*8));known[at+i]=1;}}
    U get(U address,unsigned width=4)const{auto at=offset(address,width);U value=0;for(unsigned i=0;i<width;++i)value|=U(byte[at+i])<<(i*8);return value;}
    static int access(void* user,U pc,U address,unsigned width,unsigned kind,Nba97PlayerFrameValue* value){
        auto& f=*static_cast<Fixture*>(user);f.event.push_back({pc,address,0,0,0,kind,width,0});
        try{auto at=offset(address,width);if(kind==NBA97_FRAME_READ){*value={};for(unsigned i=0;i<width;++i)if(f.known[at+i]){value->word|=U(f.byte[at+i])<<(i*8);value->known_mask|=std::uint8_t(1u<<i);}}
            else f.put(address,value->word,width);
            return NBA97_BODY_OK;}catch(const std::out_of_range&){return NBA97_BODY_BOUNDS;}
    }
    static int service(void* user,const Nba97MatchTickCall* call,Nba97GamePeriodValue* out){
        auto& f=*static_cast<Fixture*>(user);f.event.push_back({call->pc,0,call->entry,call->args[0],call->args[1],3,0,call->count});
        if(call->pc==f.service_fail_pc)return NBA97_BODY_BOUNDS;
        if(call->entry==0x80060fbc&&f.repeat_ai)f.put(0x800fdb88,1,2);
        if(call->entry==0x80067a60&&f.mutate_live_s6)f.put(0x800fdb6c,0xfffe,2);
        if(call->entry==0x8008f224){*out={call->args[0]==0?f.input0:0u,1};}
        else if(call->entry==0x80067664&&out)*out={f.early_end?1u:0u,1};
        else if(call->entry==0x800a584c&&out){U value=f.timing_calls<f.timing_returns.size()?f.timing_returns[f.timing_calls]:0;++f.timing_calls;*out={value,1};}
        else if(out)*out={0,1};
        return NBA97_BODY_OK;
    }
    static int player(void* user,U pc){auto& f=*static_cast<Fixture*>(user);f.event.push_back({pc,0,0x8006801c,0,0,4,0,0});if(f.mutate_player)f.put(0x800fdc48,0x80140000);return NBA97_BODY_OK;}
    static int ball(void* user,U pc,U pointer){auto& f=*static_cast<Fixture*>(user);f.event.push_back({pc,0,0x8006ef60,pointer,0,5,0,1});f.ball_seen=pointer;f.put(0x800fdc48,0x80140040);return NBA97_BODY_OK;}
    static int net(void* user,U pc){auto& f=*static_cast<Fixture*>(user);f.event.push_back({pc,0,0x8002dc88,0,0,6,0,0});f.put(0x800fdb6c,0xffff,2);return NBA97_BODY_OK;}
    static int frame(void* user,U pc){auto& f=*static_cast<Fixture*>(user);f.event.push_back({pc,0,0x80049018,0,0,7,0,0});++f.frame_calls;if(f.exit_after_frames&&f.frame_calls>=f.exit_after_frames)f.put(0x800fdb78,1,1);return NBA97_BODY_OK;}
    Fixture(){
        put(0x8001edec,1,2);put(0x800fdb92,2,2);put(0x800fdb8a,1,2);put(0x80021d82,1,1);
        put(0x800fdb7c,0,2);put(0x800fe8cc,1,2);put(0x800fe8c4,3,2);put(0x800fdc48,0x80130000);
        put(0x800fdbae,5,2);put(0x800fdb9c,0,2);put(0x800fa038,0,2);put(0x800fdb90,0,2);put(0x800fdb68,0,2);put(0x800fdb78,0,1);put(0x800fdbde,0,2);
    }
    int run(std::size_t budget=10000,bool generic=true,U incoming_word=7,unsigned incoming_known=1){
        Nba97MatchTickContext c{};c.access=access;c.service=generic?service:nullptr;
        c.player_update=missing_player?nullptr:player;c.ball_simulation=missing_ball?nullptr:ball;
        c.net_transform=missing_net?nullptr:net;c.match_frame=missing_frame?nullptr:frame;
        c.user=this;c.operation_budget=budget;c.incoming_s6={incoming_word,std::uint8_t(incoming_known)};return nba97_game_match_tick(&c,&progress);
    }
    const Event& at(U pc)const{for(const auto& e:event)if(e.pc==pc)return e;throw std::runtime_error("missing event");}
    unsigned calls(U entry)const{unsigned n=0;for(const auto& e:event)n+=e.entry==entry;return n;}
};
void natural_order(){
    Fixture f;check(f.run()==NBA97_BODY_OK&&f.progress.completed,"natural bounded exit completes");
    check(f.progress.player_updates==1&&f.progress.ball_ticks==1&&f.progress.net_transforms==1&&f.progress.frame_pumps==1,"all typed recovered owners reached once");
    check(f.ball_seen==0x80140000&&f.get(0x800fdc3c)==0x80140000&&f.get(0x800fdc48)==0x80140040,"player mutation is reread then exact ball pointer is published and captured");
    std::size_t player=0,read=0,write=0,ball=0;for(std::size_t i=0;i<f.event.size();++i){const auto& e=f.event[i];if(e.entry==0x8006801c)player=i;if(e.pc==0x80068d90)read=i;if(e.pc==0x80068d98)write=i;if(e.entry==0x8006ef60)ball=i;}
    check(player<read&&read<write&&write<ball,"68D84, live FDC48 read, FDC3C publication, 6EF60 order");
    check(f.calls(0x80060ef8)==2,"60FBC mutation is observed by live FDB88 reread");
    check(f.at(0x8002dd9c).a0==1&&f.at(0x8002dd9c).count==1,"2DD84 passes signed pre-net FDB6C to 798B4");
    check(f.at(0x8002dda4).entry==0x8002dc88&&f.at(0x8002ddb4).entry==0x80049018,"2DD84 net then match-frame typed owners");
    check(f.get(0x800fdb78,1)==1&&f.get(0x8001edec,2)==99&&f.calls(0x80067930)==1,"controller event exits through original tail");
    check(f.get(0x800fe8c4,2)==2&&f.get(0x800fe8a8,2)==0,"ordered global masks and 57B18 prefix");
}
void branches_and_quirks(){
    Fixture countdown;countdown.put(0x800fdb7c,1,2);countdown.put(0x800fdb6c,2,2);
    check(countdown.run()==NBA97_BODY_OK&&countdown.get(0x800fdb7c,2)==0,"countdown subtracts and clamps negative signed half");
    check(countdown.calls(0x8007a668)==1&&!countdown.calls(0x8006ef60),"countdown bypasses player/ball owners");
    Fixture carried;carried.put(0x800fdb8a,0,2);carried.put(0x800fe8cc,0,2);carried.put(0x800fe8c4,0,2);carried.put(0x800fdb68,5,2);
    carried.service_fail_pc=0x80068d6c;check(carried.run()==NBA97_BODY_BOUNDS,"carried-s6 path reaches consumed 67664 boundary");
    check(carried.at(0x80068d58).a0==7&&carried.at(0x80068d64).a0==7,"uninitialized original s6 is explicit and reused for both calls");
    Fixture unknown;unknown.put(0x800fdb8a,0,2);unknown.put(0x800fe8cc,0,2);unknown.put(0x800fe8c4,0,2);
    Nba97MatchTickContext c{};Nba97MatchTickProgress p{};c.access=Fixture::access;c.service=Fixture::service;c.user=&unknown;c.operation_budget=100;c.incoming_s6={0,0};
    check(nba97_game_match_tick(&c,&p)==NBA97_BODY_UNKNOWN&&p.stopped_pc==0x80068d38,"unknown incoming s6 refuses where the original consumes it");
    Fixture timing;timing.put(0x800fdb92,0,2);timing.put(0x800fdbde,1,2);timing.put(0x800fdb68,5,2);
    check(timing.run()==NBA97_BODY_OK&&timing.at(0x8002dd9c).a0==2,"timing path uses random four then halves to two before the net callback");
    Fixture live;live.put(0x800fe8cc,0,2);live.put(0x800fe8c4,0,2);live.put(0x800fdb68,5,2);live.mutate_live_s6=true;live.early_end=true;
    int live_status=live.run();if(live_status!=NBA97_BODY_OK)std::cerr<<"live status="<<live_status<<" pc="<<std::hex<<live.progress.stopped_pc<<" address="<<live.progress.stopped_address<<" entry="<<live.progress.stopped_entry<<std::dec<<'\n';
    check(live_status==NBA97_BODY_OK,"active FDB8A branch completes through directed early end");
    check(live.at(0x80068d40).a0==1&&live.at(0x80068d64).a0==0xfffffffeu,"67D38 receives the live FDB6C reread after 67A60 mutation");
    std::size_t first=0,mutation=0,second=0;for(std::size_t i=0;i<live.event.size();++i){const auto& e=live.event[i];if(e.pc==0x80068d3c)first=i;if(e.pc==0x80068d40)mutation=i;if(e.pc==0x80068d48)second=i;}
    check(first<mutation&&mutation<second,"68D3C read, 67A60 call, 68D48 reread source order");
    Fixture established;established.put(0x800fdb92,0,2);established.put(0x800fdb68,5,2);
    int established_status=established.run(10000,true,0,0);if(established_status!=NBA97_BODY_OK)std::cerr<<"established status="<<established_status<<" pc="<<std::hex<<established.progress.stopped_pc<<" address="<<established.progress.stopped_address<<" entry="<<established.progress.stopped_entry<<std::dec<<'\n';
    check(established_status==NBA97_BODY_OK&&established.get(0x800fdb6c,2)==0xffff,"random timing establishes s6 even when incoming caller s6 is unknown");
    Fixture unknown_store;unknown_store.put(0x800fdb92,4,2);unknown_store.put(0x800fdb9c,0xffff,2);
    check(unknown_store.run(10000,true,0,0)==NBA97_BODY_UNKNOWN&&unknown_store.progress.stopped_pc==0x80068f98&&unknown_store.progress.stopped_address==0x800fdb6c,"skipped timing block refuses unknown incoming s6 at its first store consumption");
}
void refusal_prefix(){
    Fixture baseline;check(baseline.run()==NBA97_BODY_OK,"prefix baseline");auto expected=baseline.event;
    for(std::size_t limit=0;limit<baseline.progress.operations;++limit){Fixture f;int status=f.run(limit);check(status==NBA97_BODY_JOURNAL_LIMIT&&!f.progress.completed,"every operation budget refuses");
        check(f.event.size()==limit,"refusal exposes exactly completed callback/access prefix");for(std::size_t i=0;i<limit;++i)check(f.event[i].pc==expected[i].pc&&f.event[i].entry==expected[i].entry&&f.event[i].address==expected[i].address,"refusal prefix order stable");}
    Fixture no_service;check(no_service.run(10000,false)==NBA97_MATCH_TICK_SERVICE_REQUIRED&&no_service.progress.stopped_entry==0x80066f88,"generic service is mandatory at first reached call");
    Fixture no_player;no_player.missing_player=true;check(no_player.run()==NBA97_MATCH_TICK_PLAYER_UPDATE_REQUIRED&&no_player.progress.stopped_pc==0x80068d84,"player owner refuses at exact call");
    Fixture no_ball;no_ball.missing_ball=true;check(no_ball.run()==NBA97_MATCH_TICK_BALL_SIMULATION_REQUIRED&&no_ball.get(0x800fdc3c)==0x80140000,"ball refusal retains pointer publication");
    Fixture no_net;no_net.missing_net=true;check(no_net.run()==NBA97_MATCH_TICK_NET_TRANSFORM_REQUIRED&&no_net.progress.stopped_pc==0x8002dda4,"net owner refuses after timing services");
    Fixture no_frame;no_frame.missing_frame=true;check(no_frame.run()==NBA97_MATCH_TICK_MATCH_FRAME_REQUIRED&&no_frame.progress.stopped_pc==0x8002ddb4,"frame owner refuses after 32B10");
    Nba97MatchTickProgress p{};check(nba97_game_match_tick(nullptr,&p)==NBA97_BODY_ARGUMENT&&!p.completed,"invalid context refuses");
}
}
int main(){try{natural_order();branches_and_quirks();refusal_prefix();std::cout<<checks<<" match tick checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
