#include "recovered/roster_trade.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
struct Fixture {
    std::array<uint16_t,535> table;
    std::array<uint8_t,600> positions{},injuries{};
    std::array<uint8_t,25> preference{};
    Nba97TradeScreen s{};
    Fixture(int count=10) {
        table.fill(UINT16_MAX);
        for(int t=0;t<29;++t)for(int i=0;i<count;++i)table[t*15+i]=uint16_t(t*15+i);
        for(unsigned i=0;i<positions.size();++i)positions[i]=i%5;
        for(unsigned i=0;i<25;++i)preference[i]=i%5;
        check(nba97_trade_begin(&s,table.data(),0,1,0,nullptr,nullptr,nullptr),"begin");
    }
    Nba97TradeData data() {return {positions.data(),injuries.data(),preference.data(),positions.size(),1};}
    Nba97TradeEvent key(uint16_t k) {auto d=data();return nba97_trade_input(&s,k,&d);}
    void neutral() {nba97_trade_frame(&s,0);}
};
void conserved(const Fixture& f) {
    auto before=f.table;std::array<uint16_t,535> after;
    std::copy_n(f.s.working,535,after.begin());std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
    check(before==after,"population not conserved");
}
}
int main() {try {
    {Fixture f;check(f.s.team[0]==0 && f.s.team[1]==1 && f.s.counts[0]==10,"construction");
     for(int i=0;i<20;++i)f.key(2);check(f.s.cursor[0]==14 && f.s.top[0]==9,"scroll bottom");
     f.key(4);check(f.s.team[0]==2 && f.s.cursor[0]==14 && f.s.top[0]==9,"scan skips opposite, preserves viewport");
     f.key(0x800);check(f.s.phase==NBA97_TRADE_SECOND,"empty first allowed");
     f.key(4);check(f.s.team[1]==3 && f.s.team[0]==2,"second scans independently");
     check(f.key(0x80)==NBA97_TRADE_IDLE,"Start in replacement blocked");
     f.key(0x100);check(f.s.phase==NBA97_TRADE_FIRST && f.s.waiting,"cancel replacement");
     check(f.key(0x800)==NBA97_TRADE_IDLE,"held cancel barrier");f.neutral();
     check(f.key(0x100)==NBA97_TRADE_DISCARD,"clean cancel exits");}
    std::cout<<"TRADE-SCREEN PASS construction_scrolling_independent_scans_cancel\n";
    {Fixture f;
     auto sound=[&](uint16_t raw){return nba97_trade_event_sound(f.key(raw),raw);};
     check(sound(1)==0,"top endpoint must be silent");
     for(int phase=0;phase<2;++phase) {
         check(sound(2)==4 && sound(1)==3,"down/up selector cues");
         check(sound(4)==1 && sound(8)==2,"right/left selector cues");
         if(!phase)check(sound(0x800)==6,"first-pick cue");
     }
     check(sound(0x80)==0,"blocked second-stage Start must be silent");
     check(sound(0x100)==10,"second-stage cancel override");
     check(sound(2)==0,"input barrier must be silent");f.neutral();
     check(sound(0x10)==6,"View callback cue");
     check(sound(2)==0,"child owns input, no parent cue");
     check(nba97_trade_return_child(&f.s,0x100,nullptr,nullptr,0),"return View");f.neutral();
     check(sound(0x40)==6,"Compare callback cue");
     check(nba97_trade_return_child(&f.s,0x100,nullptr,nullptr,0),"return Compare");f.neutral();
     check(sound(0x800)==6 && sound(0x800)==6,"pick and swap cues");
     for(int i=0;i<14;++i)sound(2);
     check(sound(2)==0,"bottom endpoint must be silent");
     check(sound(0x10)==0,"empty View handled by notice, not selector cue");
     for(auto e:{NBA97_TRADE_IDLE,NBA97_TRADE_INVALID,NBA97_TRADE_NOTICE,
                 NBA97_TRADE_DISCARD_PROMPT,NBA97_TRADE_ACCEPT,NBA97_TRADE_DISCARD})
         for(unsigned raw=0;raw<65536;++raw)
             check(!nba97_trade_event_sound(e,uint16_t(raw)),"non-selector event emitted cue");
     check(!nba97_trade_event_sound(NBA97_TRADE_ROW,3),"combined direction has no cue");
     Fixture blocked;blocked.s.mode=2;std::fill_n(blocked.s.eligible,16,int8_t(0));
     auto e=blocked.key(4);check(e==NBA97_TRADE_IDLE && !nba97_trade_event_sound(e,4),"no eligible scan must be silent");
     // 56B44/56C50 explicitly admit only10/40 for child routes. Do not
     // infer shoulder aliases from an emulator keyboard name (S is R1).
     for(int phase=0;phase<2;++phase)for(uint16_t raw:{0x200,0x400,0x1000,0x2000,0x50,0xa00}) {
         Fixture gate;if(phase)gate.key(0x800);const auto before=gate.s;
         const auto ignored=gate.key(raw);
         check(ignored==NBA97_TRADE_IDLE&&!nba97_trade_event_sound(ignored,raw)&&
             !gate.s.child&&nba97_trade_result(&gate.s)==0&&gate.s.phase==before.phase&&
             !std::memcmp(gate.s.working,before.working,sizeof(gate.s.working))&&
             !std::memcmp(gate.s.selected,before.selected,sizeof(gate.s.selected)),
             "shoulder/combined callback input must not select, mutate or open a child");}}
    std::cout<<"TRADE-SCREEN PASS source_selector_audio_and_silent_noops\n";
    {Fixture f;f.key(0x800);f.key(2);check(f.key(0x800)==NBA97_TRADE_SWAPPED,"occupied swap");
     check(f.s.working[0]==16 && f.s.working[16]==0 && f.s.counts[0]==10 && f.s.changes==1,"swap identities/counts");
     conserved(f);check(f.key(0x100)==NBA97_TRADE_DISCARD_PROMPT,"dirty discard prompt");
     nba97_trade_discard_answer(&f.s,0,0x800);f.neutral();check(nba97_trade_dirty(&f.s),"decline keeps draft");
     nba97_trade_discard_answer(&f.s,1,0x800);check(!nba97_trade_dirty(&f.s),"discard restores entire snapshot");}
    std::cout<<"TRADE-SCREEN PASS swap_and_full_snapshot_discard\n";
    // Preserve the original quirk intentionally: even Select/ignore child
    // returns create a new undo checkpoint (56494), not a disk save.
    for(uint16_t child:{0x10,0x40})for(int phase=0;phase<2;++phase)for(int answer=0;answer<3;++answer) {
        Fixture f;f.key(0x800);f.key(0x800);
        const auto first_trade=f.s;
        if(phase)f.key(0x800);
        check(f.key(child)==(child==0x10?NBA97_TRADE_VIEW:NBA97_TRADE_COMPARE),"quirk child route");
        int16_t teams[]{0,1};uint8_t slots[]{1,1};
        check(nba97_trade_return_child(&f.s,answer==0?0x100:0x80,teams,slots,answer==2),"quirk child return");
        f.neutral();
        check(nba97_trade_dirty(&f.s)&&!nba97_trade_undo_dirty(&f.s)&&f.s.changes==0&&
            !std::memcmp(f.s.snapshot,f.table.data(),sizeof(f.s.snapshot))&&
            !std::memcmp(f.s.undo,first_trade.working,sizeof(f.s.undo))&&f.s.phase==phase,
            "child must rebase undo, preserve phase and leave durable baseline alone");
        if(phase){check(f.key(0x100)==NBA97_TRADE_CANCEL_PICK,"second-stage cancel before exit");f.neutral();}
        const auto checkpoint=f.s;
        check(f.key(0x100)==NBA97_TRADE_DISCARD&&nba97_trade_result(&f.s)==-1&&
            !std::memcmp(f.s.working,checkpoint.working,sizeof(f.s.working)),
            "original quirk: clean checkpoint cancel exits and retains earlier trade");
        f.s=checkpoint;
        f.key(0x800);for(int i=0;i<14;++i)f.key(2);
        check(f.key(0x800)==NBA97_TRADE_SWAPPED&&nba97_trade_undo_dirty(&f.s),"post-child transfer");
        check(f.key(0x100)==NBA97_TRADE_DISCARD_PROMPT,"post-child edits require discard prompt");
        const auto later=f.s;
        nba97_trade_discard_answer(&f.s,0,0x800);f.neutral();
        check(!std::memcmp(f.s.working,later.working,sizeof(f.s.working)),"declining post-child discard lost edits");
        check(nba97_trade_discard_answer(&f.s,1,0x800)==NBA97_TRADE_DISCARD&&
            !std::memcmp(f.s.working,checkpoint.working,sizeof(f.s.working))&&
            !std::memcmp(f.s.counts,checkpoint.counts,sizeof(f.s.counts))&&
            nba97_trade_dirty(&f.s)&&!nba97_trade_undo_dirty(&f.s),
            "post-child discard must restore checkpoint slots/counts, not initial roster");
        conserved(f);
    }
    std::cout<<"TRADE-SCREEN PASS original_child_undo_quirk_12_routes_and_later_discard\n";
    for(int direction=0;direction<2;++direction) {
        Fixture f;
        if(direction==0)for(int i=0;i<14;++i)f.key(2);
        f.key(0x800);
        if(direction==1)for(int i=0;i<14;++i)f.key(2);
        check(f.key(0x800)==NBA97_TRADE_SWAPPED,"transfer");
        check(f.s.counts[direction]==11 && f.s.counts[1-direction]==9,"transfer counts");conserved(f);
    }
    std::cout<<"TRADE-SCREEN PASS both_transfer_directions_starter_repair\n";
    {Fixture f(8);for(int i=0;i<14;++i)f.key(2);f.key(0x800);
     check(f.key(0x800)==NBA97_TRADE_NOTICE && f.s.notice.notice==NBA97_ROSTER_NOTICE_MINIMUM,"minimum-eight rejection");
     check(!nba97_trade_dirty(&f.s),"minimum rejection modified draft");
     nba97_trade_dismiss_notice(&f.s,0x800);f.neutral();f.key(0x100);f.neutral();
     f.s.mode=1;f.s.cursor[0]=0;f.s.selected[0]=0;f.injuries[0]=1;
     check(f.key(0x800)==NBA97_TRADE_NOTICE,"first injury gate");
     nba97_trade_dismiss_notice(&f.s,0x800);f.neutral();f.injuries[0]=0;f.key(0x800);f.injuries[15]=1;
     check(f.key(0x800)==NBA97_TRADE_NOTICE && f.s.notice.subject==15,"second injury gate");
     nba97_trade_dismiss_notice(&f.s,0x800);f.neutral();check(f.key(0x100)==NBA97_TRADE_CANCEL_PICK,"cancel bypasses injury validation");}
    std::cout<<"TRADE-SCREEN PASS minimum_injury_notice_and_cancel\n";
    {Fixture f;f.key(0x800);for(int i=0;i<14;++i)f.key(2);f.key(0x100);f.neutral();
     for(int i=0;i<14;++i)f.key(2);f.key(0x800);check(f.key(0x800)==NBA97_TRADE_IDLE,"both empty silent");
     check(!nba97_trade_dirty(&f.s),"both empty mutated");
     check(f.key(0x40)==NBA97_TRADE_NOTICE,"empty compare refused");}
    std::cout<<"TRADE-SCREEN PASS empty_selection_and_child_guard\n";
    {Fixture f;check(f.key(0x10)==NBA97_TRADE_VIEW,"View request");
     int16_t team[2]{2,1};uint8_t slot[2]{9,0};
     check(nba97_trade_child_proposal(&f.s,0x80,team,slot)==1,"View keep prompt");
     check(!nba97_trade_child_proposal(&f.s,0x100,team,slot),"Cancel no keep prompt");
     check(nba97_trade_return_child(&f.s,0x80,team,slot,1),"View adopt");
     check(f.s.team[0]==2 && f.s.cursor[0]==9 && f.s.top[0]==9 && !nba97_trade_dirty(&f.s),"View selection not a trade");
     f.neutral();f.key(0x40);team[0]=3;team[1]=4;slot[0]=2;slot[1]=3;
     check(nba97_trade_return_child(&f.s,0x80,team,slot,1),"Compare adopt");
     check(f.s.team[0]==3 && f.s.team[1]==4 && f.s.cursor[1]==3,"Compare both sides");
     f.neutral();f.key(0x40);team[0]=team[1]=5;check(!nba97_trade_child_proposal(&f.s,0x80,team,slot),"same-team Compare cannot adopt");
     check(nba97_trade_return_child(&f.s,0x100,team,slot,0),"Cancel child returns");}
    std::cout<<"TRADE-SCREEN PASS View_Compare_proposals_and_writeback\n";
    {Fixture f;f.key(0x10);int16_t teams[2]{1,1};uint8_t slots[2]{0,0};
     check(nba97_trade_return_child(&f.s,0x80,teams,slots,1),"same-team View return");
     check(f.s.team[0]==0 && f.s.team[1]==1,"entry wrapper separates View team collision");}
    std::cout<<"TRADE-SCREEN PASS View_same_team_reentry_normalization\n";
    {Fixture f;auto before=f.s;auto malformed=f.table;malformed[0]=UINT16_MAX;
     check(!nba97_trade_begin(&f.s,malformed.data(),0,1,0,nullptr,nullptr,nullptr),"holes rejected");
     check(!std::memcmp(&before,&f.s,sizeof(before)),"failed entry changed state");
     auto d=f.data();d.positions=nullptr;check(nba97_trade_input(&f.s,0x800,&d)==NBA97_TRADE_INVALID,"missing provider fails");
     check(!std::memcmp(&before,&f.s,sizeof(before)),"provider failure changed state");}
    std::cout<<"TRADE-SCREEN PASS malformed_entry_and_provider_guards\n";
    for(int a=0;a<15;++a)for(int b=0;b<15;++b) {Fixture f;for(int i=0;i<a;++i)f.key(2);f.key(0x800);
        for(int i=0;i<b;++i)f.key(2);auto e=f.key(0x800);
        check(e==(a>=10 && b>=10?NBA97_TRADE_IDLE:NBA97_TRADE_SWAPPED),"all-pair result");conserved(f);}
    std::cout<<"TRADE-SCREEN PASS all_225_slot_pairs_conserve_population\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"TRADE-SCREEN FAIL "<<e.what()<<'\n';return 1;}}
