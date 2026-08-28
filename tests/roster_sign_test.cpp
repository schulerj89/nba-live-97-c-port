#include "recovered/roster_sign.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"SIGN FAIL %s\n",why);std::exit(1);}}
static void pass(const char* name){std::printf("SIGN PASS %s\n",name);}
using Table=std::array<uint16_t,535>;
static Table base(){Table t;t.fill(UINT16_MAX);unsigned id=0;
    for(int team=0;team<29;++team)for(int i=0;i<13;++i)t[team*15+i]=uint16_t(id++);
    for(int i=0;i<67;++i)t[435+i]=uint16_t(id++);return t;}
static std::array<uint8_t,600> positions{},injuries{};
static const uint8_t preference[25]{};
static Nba97TradeData data{positions.data(),injuries.data(),preference,600,0};
static Nba97TradeScreen start(const Table& t,int source=0,int dest=0,int team=2){
    Nba97TradeScreen s{};uint8_t c[]{uint8_t(source),uint8_t(dest)};
    uint8_t top[]{uint8_t(std::min(source,94)),uint8_t(std::min(dest,9))};
    check(nba97_sign_begin(&s,t.data(),int16_t(team),0,nullptr,c,top)!=0,"entry");return s;}
static Nba97TradeEvent key(Nba97TradeScreen& s,uint16_t k){nba97_trade_frame(&s,0);return nba97_trade_input(&s,k,&data);}
int main(){
    auto t=base();auto s=start(t);
    check(s.frontend_state==14&&s.team[0]==29&&s.team[1]==2&&s.list_kind[0]==0&&s.list_kind[1]==1&&
        s.input_callback[0]==nba97_sign_first&&s.input_callback[1]==nba97_sign_second&&
        nba97_roster_editor_capacity(&s,0)==100&&nba97_roster_editor_capacity(&s,1)==15,"constructor bindings");
    auto sentinel=start(t,0,0,29);check(sentinel.team[1]==3,"normalize sentinel");
    check(nba97_trade_result(&s)==0,"running result");pass("entry_bindings_signed_results");
    for(int i=0;i<99;++i)check(key(s,2)==NBA97_TRADE_ROW,"100-row movement");
    check(s.cursor[0]==99&&s.top[0]==94&&key(s,2)==NBA97_TRADE_IDLE,"end clamp");
    for(int i=0;i<99;++i)check(key(s,1)==NBA97_TRADE_ROW,"reverse scroll");
    check(!s.cursor[0]&&!s.top[0]&&key(s,1)==NBA97_TRADE_IDLE,"start clamp");pass("source_100_row_scroll");
    s=start(t);check(key(s,8)==NBA97_TRADE_TEAM&&s.team[0]==29&&s.team[1]==1,"scan receiver first");
    check(key(s,0x800)==NBA97_TRADE_PICK,"pick");
    check(key(s,4)==NBA97_TRADE_TEAM&&s.team[0]==29&&s.team[1]==2,"scan receiver second");pass("scan_receiver_both_phases");
    s=start(t,99);auto before=s;
    check(key(s,0x800)==NBA97_TRADE_NOTICE&&s.notice.message_address==0x800aed20&&s.notice.subject==2&&
        !std::memcmp(s.working,before.working,sizeof(s.working))&&s.phase==NBA97_TRADE_FIRST,"empty source");
    nba97_trade_dismiss_notice(&s,0x800);check(!nba97_trade_frame(&s,0x800),"notice held barrier");
    check(nba97_trade_frame(&s,0),"notice release");pass("empty_source_notice_barrier");
    s=start(t);s.mode=1;data.injuries_enabled=1;injuries[s.selected[0]]=1;
    check(key(s,0x800)==NBA97_TRADE_NOTICE&&s.notice.message_address==0x800aebb2,"injury delegate");
    injuries.fill(0);data.injuries_enabled=0;pass("source_injury_delegate");
    s=start(t);check(key(s,0x800)==NBA97_TRADE_PICK,"first pick");before=s;
    check(key(s,0x800)==NBA97_TRADE_NOTICE&&s.notice.message_address==0x800aed88&&
        s.phase==NBA97_TRADE_SECOND&&!std::memcmp(s.working,before.working,sizeof(s.working)),"occupied target");
    auto full=t;full[2*15+13]=550;full[2*15+14]=551;s=start(full);
    check(key(s,0x800)==NBA97_TRADE_PICK&&key(s,0x800)==NBA97_TRADE_NOTICE&&s.notice.message_address==0x800aec72&&s.notice.subject==2,"full precedence");pass("destination_refusal_precedence");
    s=start(t,66,14);const auto id=s.selected[0];
    check(key(s,0x800)==NBA97_TRADE_PICK&&key(s,0x800)==NBA97_TRADE_SWAPPED,"sign success");
    check(s.working[43]==id&&s.working[44]==UINT16_MAX&&s.working[501]==UINT16_MAX&&
        s.counts[29]==66&&s.counts[2]==14&&s.changes==1&&s.phase==NBA97_TRADE_FIRST&&s.cursor[0]==66,
        "compact insertion and last-row cursor retained");
    check(nba97_trade_event_sound(NBA97_TRADE_SWAPPED,0x800)==6&&s.latch==0,"single cue contract");
    // Direct callback boundary, NOT a claim this state is reachable through UI.
    // 56EF8 compares only -1; two empties return0 from validation/mutation,
    // yet 56F5C still requests cue6 and finishes the second selection.
    s=start(t,99,14);s.phase=NBA97_TRADE_SECOND;before=s;
    check(key(s,0x800)==NBA97_TRADE_SWAPPED&&!s.changes&&s.phase==NBA97_TRADE_FIRST&&
        !std::memcmp(s.working,before.working,sizeof(s.working)),"validation zero is not general refusal");
    pass("transfer_compaction_counts_cue");
    s=start(t);key(s,0x800);before=s;s.selector_action=2;
    check(key(s,0x800)==NBA97_TRADE_CANCEL_PICK&&s.phase==NBA97_TRADE_FIRST&&!s.changes&&
        !std::memcmp(s.working,before.working,sizeof(s.working)),"selector+11==2 skip mutation");pass("selector_action_two_preserved");
    s=start(t);s.latch=99;check(key(s,0x200)==NBA97_TRADE_IDLE&&!s.latch,"unknown latch");
    check(key(s,0x10)==NBA97_TRADE_VIEW&&s.child==0x24&&nba97_trade_result(&s)==2,"view route");
    int16_t teams[]{29,2};uint8_t slots[]{0,0};
    check(nba97_trade_return_child(&s,0x100,teams,slots,0)&&s.team[0]==29,"return preserves free descriptor");
    check(key(s,0x40)==NBA97_TRADE_COMPARE&&s.child==0x23&&nba97_trade_result(&s)==3,"compare route");
    s=start(t,0,14);check(key(s,0x40)==NBA97_TRADE_NOTICE&&s.notice.message_address==0x800afc22,"empty compare");
    // Original 5A3FC/5A6F0 allow adoption only from Trade (state13), not Sign.
    // Exercise both children in both phases, including browsing to NBA teams.
    for(int phase=0;phase<2;++phase)for(auto child:{uint16_t(0x10),uint16_t(0x40)}) {
        s=start(t);if(phase)key(s,0x800);
        s.latch=99;check(key(s,0x200)==NBA97_TRADE_IDLE&&!s.latch,"second/first unknown latch");
        check(key(s,child)==(child==0x10?NBA97_TRADE_VIEW:NBA97_TRADE_COMPARE),"both phase child routing");
        int16_t browsed[]{4,5};uint8_t rows[]{3,4};
        check(!nba97_trade_child_proposal(&s,0x80,browsed,rows),"Sign must not offer Trade adoption");
        check(!nba97_trade_return_child(&s,0x80,browsed,rows,1),"reject illegal Sign adoption");
        check(nba97_trade_return_child(&s,0x80,browsed,rows,0)&&s.team[0]==29&&s.team[1]==3&&
            s.cursor[0]==0&&s.cursor[1]==0&&s.phase==phase,
            "Sign child return normalizes left29 to Chicago and restores saved phase");
    }
    pass("children_and_unknown_inputs");
    s=start(t);key(s,0x800);check(key(s,0x100)==NBA97_TRADE_CANCEL_PICK&&s.phase==NBA97_TRADE_FIRST,"cancel second");
    check(!nba97_trade_frame(&s,0x100),"cancel repeat barrier");
    check(key(s,0x100)==NBA97_TRADE_DISCARD&&nba97_trade_result(&s)==-1,"cancel first signed");
    s=start(t,0,14);key(s,0x800);key(s,0x800);
    check(key(s,0x100)==NBA97_TRADE_DISCARD_PROMPT,"dirty prompt");
    check(nba97_trade_discard_answer(&s,0,0x800)==NBA97_TRADE_IDLE&&s.changes==1,"decline discard");
    check(nba97_trade_discard_answer(&s,1,0x800)==NBA97_TRADE_DISCARD&&!std::memcmp(s.working,t.data(),sizeof(s.working)),"restore535");
    s=start(t);check(key(s,0x80)==NBA97_TRADE_ACCEPT&&nba97_trade_result(&s)==1,"accept signed");pass("cancel_accept_discard");
    s=start(t,0,14);key(s,0x800);key(s,0x800);key(s,0x10);before=s;
    check(nba97_trade_return_child(&s,0x100,teams,slots,0),"child return");
    check(!nba97_trade_undo_dirty(&s)&&nba97_trade_dirty(&s)&&!s.changes&&
        !std::memcmp(s.working,before.working,sizeof(s.working)),"shared original reentry snapshot quirk");pass("child_reentry_undo_quirk");
    check(nba97_sign_available(t.data(),0,0,nullptr)==58,"vacancy count not bool");
    auto empty=t;empty[435]=UINT16_MAX;check(!nba97_sign_available(empty.data(),0,0,nullptr),"first sentinel gate");
    std::array<int8_t,16> eligible;for(int i=0;i<16;++i)eligible[i]=int8_t(i);
    check(nba97_sign_available(t.data(),2,0,eligible.data())==32&&!nba97_sign_available(t.data(),2,1,eligible.data()),"mode2 restriction/eligible");
    auto allfull=t;for(int i=0;i<435;++i)allfull[i]=uint16_t(i);
    check(!nba97_sign_available(allfull.data(),0,0,nullptr),"no vacancies");
    check(nba97_sign_available(full.data(),0,0,nullptr)==56,"full current not global disable");pass("availability_normal_special");
    s={};check(nba97_sign_begin(&s,t.data(),29,2,eligible.data(),nullptr,nullptr)&&s.team[1]==0,"special normalization");
    check(key(s,8)==NBA97_TRADE_TEAM&&s.team[1]==15&&s.team[0]==29,"mode2 reverse scan skips ineligible teams");
    check(key(s,0x800)==NBA97_TRADE_PICK&&key(s,4)==NBA97_TRADE_TEAM&&s.team[1]==0&&s.team[0]==29,
        "mode2 forward second-stage scan skips ineligible teams");
    key(s,4);key(s,0x10);
    check(nba97_trade_return_child(&s,0x100,teams,slots,0)&&s.team[1]==0&&s.phase==NBA97_TRADE_SECOND,
        "mode2 Sign child re-entry normalizes receiver to context team");
    check(!nba97_sign_begin(&s,t.data(),-1,0,nullptr,nullptr,nullptr)&&
        !nba97_sign_begin(&s,t.data(),0,2,nullptr,nullptr,nullptr),"native guards");
    for(const auto badCursor: {std::array<uint8_t,2>{100,0},std::array<uint8_t,2>{0,15},std::array<uint8_t,2>{6,0}}) {
        const uint8_t top[]{0,0};before=s;
        check(!nba97_sign_begin(&s,t.data(),2,0,nullptr,badCursor.data(),top)&&
            !std::memcmp(&s,&before,sizeof(s)),"invalid cursor/capacity leaves caller untouched");
    }
    s=start(t,0,14);key(s,0x800);before=s;auto missing=data;missing.positions=nullptr;
    check(nba97_trade_input(&s,0x800,&missing)==NBA97_TRADE_INVALID&&
        !std::memcmp(&s,&before,sizeof(s)),"missing provider cannot partially sign");
    pass("special_entry_and_guards");
    unsigned cases=0,transfers=0,refused=0;
    for(int team=0;team<29;++team)for(int source=0;source<100;++source)for(int target=0;target<15;++target){
        s=start(t,source,target,team);before=s;const auto first=key(s,0x800);
        if(source>=67){check(first==NBA97_TRADE_NOTICE&&s.notice.message_address==0x800aed20,"matrix empty");++refused;}
        else {check(first==NBA97_TRADE_PICK,"matrix pick");const auto result=key(s,0x800);
            if(target<13){check(result==NBA97_TRADE_NOTICE&&s.notice.message_address==0x800aed88,"matrix occupied");++refused;}
            else {check(result==NBA97_TRADE_SWAPPED,"matrix transfer");++transfers;
                auto expected=t;expected[team*15+13]=t[435+source];
                std::move(expected.begin()+436+source,expected.end(),expected.begin()+435+source);expected.back()=UINT16_MAX;
                check(std::equal(expected.begin(),expected.end(),s.working),"independent full-table oracle");}}
        if(s.changes==0)check(!std::memcmp(before.working,s.working,sizeof(s.working)),"matrix refusal mutation");
        ++cases;
    }
    check(cases==43500&&transfers==3886&&refused==39614,"matrix totals");pass("matrix_43500_slot_pairs");
    return 0;
}
