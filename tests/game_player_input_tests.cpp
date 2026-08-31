#include "recovered/game_player_input.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
Nba97GamePeriodValue value(std::uint32_t word){return {word,1};}
Nba97GamePlayerInputState fixture(){
    Nba97GamePlayerInputState s{};s.player_count=24;
    for(unsigned e=0;e<11;++e){for(auto& v:s.entity[e])v.known=1;s.entity[e][NBA97_INPUT_00]=value(e);
        s.player_reference[e]=value(e);s.entity_table[e]={static_cast<std::uint8_t>(e),1};}
    for(auto& v:s.global)v.known=1;
    for(auto& c:s.controller)for(auto& v:c)v.known=1;
    for(auto& v:s.player1d)v=value(60);
    s.reference_fdc34={10,1};s.team_fdc40={0,1};
    s.global[NBA97_INPUT_FDBCC]=value(65535);s.global[NBA97_INPUT_FDBD2]=value(65535);
    s.global[NBA97_INPUT_FDB90]=value(129);s.entity[0][NBA97_INPUT_1A]=value(7);
    return s;
}
struct Calls {
    std::vector<Nba97GameInputCall> list;std::uint32_t returned=0;unsigned mode=0;
    static int run(void* ctx,Nba97GamePlayerInputState* s,const Nba97GameInputCall* call,Nba97GamePeriodValue* out){
        auto& self=*static_cast<Calls*>(ctx);self.list.push_back(*call);*out=value(self.returned);
        // Explicit synthetic mutable boundary, not credit for the called owner.
        if(self.mode==1&&call->owner==NBA97_INPUT_CALL_6CD50)s->global[NBA97_INPUT_FDB94]=value(1);
        if(self.mode==2&&call->owner==NBA97_INPUT_CALL_5699C){s->entity[0][NBA97_INPUT_C0]=value(54321);s->global[NBA97_INPUT_FDBCC]=value(0);}
        if(self.mode==3&&call->owner==NBA97_INPUT_CALL_6A2E4)s->global[NBA97_INPUT_FDB90]=value(0);
        if(self.mode==4)s->entity[0][NBA97_INPUT_04].known=2;
        return self.mode==5?0:self.mode==6?-1:1;
    }
};
int run(Nba97GamePlayerInputState& s,unsigned mask,Calls* calls,Nba97GameInputReceipt& receipt,unsigned mapped=0){
    return nba97_game_player_input(&s,0,{0,1},mask,mapped,calls?Calls::run:nullptr,calls,&receipt);
}
void gates(){
    Nba97GameInputReceipt r{};auto s=fixture();
    s.entity[0][NBA97_INPUT_04]=value(65535);
    check(run(s,0x180,nullptr,r,0x1000)==NBA97_INPUT_PLAY_CALL_PENDING,"play-call pending");
    check(r.count==2&&r.event[0].value.word==65535&&r.event[1].value.word==127&&r.stopped_pc==0x800617d0,"two ordered marker stores before play route");
    s=fixture();s.team_fdc40={0,0};
    check(run(s,0,nullptr,r,0x3000)==1&&!r.play_call_pending,"zero edge uses ordinary route despite held play modifiers");
    for(unsigned f:{NBA97_INPUT_FE8CC,NBA97_INPUT_FDB7C})for(unsigned x:{1u,65535u}){
        s=fixture();s.global[f]=value(x);check(run(s,0x20,nullptr,r)==1&&r.count==0,"both signed nonzero gates stop");}
    for(unsigned f:{NBA97_INPUT_10,NBA97_INPUT_18,NBA97_INPUT_60,NBA97_INPUT_64}){
        s=fixture();s.entity[0][f]=value(1);check(run(s,0x20,nullptr,r)==1&&r.count==0,"airborne and cached flags stop");}
    s=fixture();s.entity[0][NBA97_INPUT_1A]=value(20);check(run(s,0x20,nullptr,r)==1,"actor20 gate");
    s=fixture();s.global[NBA97_INPUT_FDB90]=value(130);s.global[NBA97_INPUT_FE880]=value(65535);
    check(run(s,0x20,nullptr,r)==1,"signed side claim not narrowed to byte");
    s=fixture();check(run(s,0x20,nullptr,r)==NBA97_INPUT_PENDING&&r.event[0].call.owner==NBA97_INPUT_CALL_6A2E4,"jump boundary reached naturally");
    s=fixture();s.global[NBA97_INPUT_FDBCC]=value(11);check(run(s,0x20,nullptr,r)==NBA97_INPUT_REFERENCE,"unowned positive possessor index refuses");
    s=fixture();s.global[NBA97_INPUT_FDBCC]=value(1);s.entity_table[1]={2,1};s.entity[2][NBA97_INPUT_04]=value(65535);s.entity[0][NBA97_INPUT_1A]=value(6);
    check(run(s,0x20,nullptr,r)==NBA97_INPUT_PENDING&&r.event[0].call.entity==2&&r.event[0].call.argument[0]==0,"pass uses actual aliased table entity and pointer argument");
}
void callbacks(){
    Nba97GameInputReceipt r{};auto s=fixture();Calls c;c.mode=1;s.global[NBA97_INPUT_FDBCC]=value(0);
    check(run(s,0x200,&c,r)==1&&c.list.size()==2&&c.list[1].owner==NBA97_INPUT_CALL_612E4,"side re-read after first owner");
    s=fixture();c={};s.global[NBA97_INPUT_FDBCC]=value(0);s.entity[0][NBA97_INPUT_BA]=value(10);s.player1d[0]=value(75);s.global[NBA97_INPUT_FDB90]=value(65535);c.returned=0x100;
    check(run(s,0x10,&c,r)==1&&s.entity[0][NBA97_INPUT_D8].word==1,"5BDD8 full nonzero v0, signed phase");
    check(c.list[0].argument[0]==0xffffffffu,"logical10 uses negative original argument");
    for(unsigned ret:{0u,0x100u,1u}){
        s=fixture();c={};c.returned=ret;s.global[NBA97_INPUT_FDBCC]=value(0);
        check(run(s,0x40,&c,r)==1&&c.list.size()==(ret==1?2u:1u),"5ADB8 consumes only lowbyte v0");}
    s=fixture();c={};c.mode=2;s.global[NBA97_INPUT_FDBCC]=value(1);s.global[NBA97_INPUT_FDB94]=value(1);
    check(run(s,0x50,&c,r)==1&&s.entity[0][NBA97_INPUT_A4].word==54321&&c.list.size()==3,"setter postcall C0 and later possessor re-read");
    s=fixture();c={};c.mode=3;s.entity[0][NBA97_INPUT_14]=value(65533);s.entity[0][NBA97_INPUT_16]=value(9);
    check(run(s,0x20,&c,r)==1&&c.list.size()==4&&c.list[1].argument[0]==68,"rejected jump re-reads phase for fallback");
    check(s.entity[0][NBA97_INPUT_14].word==65535&&s.entity[0][NBA97_INPUT_16].word==2,"fallback arithmetic quarters existing velocity");
    s=fixture();c={};s.entity[0][NBA97_INPUT_BE]=value(41);s.entity[0][NBA97_INPUT_14]=value(65533);
    check(run(s,0x20,&c,r)==1&&c.list.size()==3&&c.list[0].argument[0]==77&&c.list[2].argument[0]==79&&s.entity[0][NBA97_INPUT_14].word==65533,"phase81 fallback does not quarter");
    s=fixture();c={};c.returned=1;check(run(s,0x20,&c,r)==1&&c.list.size()==1,"actual accepted1 stops before fallback");
    for(unsigned mode:{4u,5u,6u}){s=fixture();c={};c.mode=mode;
        check(run(s,0x20,&c,r)==(mode==4?NBA97_INPUT_ARGUMENT:mode==5?NBA97_INPUT_PENDING:NBA97_INPUT_CALLBACK_FAILED),"bad callback never acknowledged complete");}
    s=fixture();s.entity[0][NBA97_INPUT_04]={9,0};auto before=s;
    check(run(s,0x180,nullptr,r)==NBA97_INPUT_ARGUMENT&&std::memcmp(&s,&before,sizeof s)==0,"malformed initial state atomic");
}
void edge(){
    Nba97GameInputReceipt r{};auto s=fixture();s.global[NBA97_INPUT_FC99C]=value(0);s.global[NBA97_INPUT_D8EEC]=value(0);
    constexpr unsigned expected[]={8,4,0,4,2,3,1,3,6,5,7,5,6,5,7,5};
    for(unsigned mask=0;mask<16;++mask){s.controller[0][NBA97_INPUT_CONTROL_30]=value(0);
        check(nba97_game_input_edge(&s,0,mask,&r)==1&&s.controller[0][NBA97_INPUT_CONTROL_38].word==expected[mask],"contradictory direction priority");}
    s=fixture();s.controller[0][NBA97_INPUT_CONTROL_30]=value(0x8000);
    check(nba97_game_input_edge(&s,0,0x80000001,&r)==1&&r.edge_mask.word==1,"signextended prior suppresses rawhigh edge");
    s.controller[0][NBA97_INPUT_CONTROL_30]=value(0);
    check(nba97_game_input_edge(&s,0,0x80000001,&r)==1&&r.edge_mask.word==0x80000001&&s.controller[0][NBA97_INPUT_CONTROL_34].word==1,"full32 result differs from stored16");
    s=fixture();s.controller[0][NBA97_INPUT_CONTROL_26]=value(2);s.entity_table[2]={5,1};
    check(nba97_game_input_edge(&s,0,0x400,&r)==1&&s.entity[5][NBA97_INPUT_E4].word==10,"held400 actual selected alias");
    s.entity[5][NBA97_INPUT_E4]=value(0);
    check(nba97_game_input_edge(&s,0,0x400,&r)==1&&r.edge_mask.word==0&&s.entity[5][NBA97_INPUT_E4].word==10,"held400 writes again without edge");
    s=fixture();s.controller[0][NBA97_INPUT_CONTROL_26]=value(11);
    check(nba97_game_input_edge(&s,0,0x400,&r)==NBA97_INPUT_REFERENCE&&r.count==8&&s.controller[0][NBA97_INPUT_CONTROL_2A].word==1024,"late selected bound preserves all prefix writes");
    s=fixture();s.controller[0][NBA97_INPUT_CONTROL_30]={0,0};
    check(nba97_game_input_edge(&s,0,1,&r)==NBA97_INPUT_UNRESOLVED&&r.count==2&&s.controller[0][NBA97_INPUT_CONTROL_30].known==1,"old unknown mask refuses after source stores");
    s=fixture();s.controller[0][NBA97_INPUT_CONTROL_3C]={0,0};s.global[NBA97_INPUT_D8EEC]={0,0};s.global[NBA97_INPUT_FC99C]={0,0};
    check(nba97_game_input_edge(&s,0,0,&r)==1&&r.event[5].call.argument_known==1,"neutral helper does not require unused mode/global values");
    check(nba97_game_input_edge(&s,0,1,&r)==NBA97_INPUT_UNRESOLVED&&r.event[5].call.argument_known==1&&r.event[5].call.argument[0]==4,"unresolved mode retains known direction argument");
    Nba97GamePeriodValue direction{0xdeadbeef,1};
    check(nba97_game_input_direction(&direction,&s,0xdead0008,0xff)==1&&direction.word==8,"helper rawlow16 neutral test");
    direction=value(55);check(nba97_game_input_direction(&direction,&s,1,0x100)==NBA97_INPUT_UNRESOLVED&&direction.word==55,"helper low8 mode and atomic failure");
}
}
int main(){try{gates();callbacks();edge();std::cout<<checks<<" player-input checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<checks<<": "<<e.what()<<'\n';return 1;}}
