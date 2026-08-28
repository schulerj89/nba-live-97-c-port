#include "recovered/roster_release.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void check(bool ok,const char* why) {
    if(!ok) {std::fprintf(stderr,"RELEASE FAIL %s\n",why);std::exit(1);}
}
static void pass(const char* name) {std::printf("RELEASE PASS %s\n",name);}

int main() {
    std::array<uint16_t,535> table;
    table.fill(UINT16_MAX);
    for(unsigned count=0;count<100;++count) {
        std::fill(table.begin()+435,table.end(),UINT16_MAX);
        for(unsigned i=0;i<count;++i)table[435+i]=uint16_t(i);
        auto before=table;
        Nba97ReleasePosition p{255,255};
        check(nba97_release_prepare_free_agents(table.data()+435,&p)==1,"entry accepted");
        check(p.cursor==count,"first empty slot");
        check(p.top==std::clamp(int(count)-4,0,94),"entry top uses minus4, capped94");
        check(p.cursor>=p.top && p.cursor<p.top+6,"vacancy visible");
        check(table==before,"entry does not mutate roster");
    }
    pass("entry_all_100_vacancy_positions");

    table.fill(123);
    table[438]=UINT16_MAX;table[534]=UINT16_MAX;
    Nba97ReleasePosition p{255,255};
    check(nba97_release_prepare_free_agents(table.data()+435,&p)==1 && p.cursor==3 && p.top==0,
          "search stops at first sentinel even if later slots occupied");
    pass("entry_first_sentinel_not_count");

    table.fill(123);p={77,73};
    check(!nba97_release_prepare_free_agents(table.data()+435,&p),"full pool safely rejected");
    check(p.cursor==77 && p.top==73,"failed preparation leaves output unchanged");
    check(!nba97_release_prepare_free_agents(nullptr,&p),"null pool rejected");
    check(!nba97_release_prepare_free_agents(table.data()+435,nullptr),"null output rejected");
    pass("entry_native_bounds_guards");

    for(int mode:{-32768,-1,0,1,2,3,32767})for(unsigned flag=0;flag<256;++flag)
        for(unsigned full=0;full<2;++full) {
            table[534]=full?123:UINT16_MAX;
            check(nba97_release_available(table.data(),int16_t(mode),uint8_t(flag))==
                  int(!(mode==2 && flag) && !full),"mode/restriction/last-slot matrix");
        }
    pass("availability_modes_restrictions_3584_cases");

    for(unsigned value=0;value<65536;++value) {
        table[534]=uint16_t(value);
        check(nba97_release_available(table.data(),0,0)==int(value==65535),"exact signed sentinel");
    }
    pass("availability_all_halfword_values");

    table.fill(UINT16_MAX);table[534]=1;
    check(!nba97_release_available(table.data(),0,0),"earlier vacancies do not enable card");
    table.fill(1);table[534]=UINT16_MAX;
    check(nba97_release_available(table.data(),0,0)==1,"only last vacancy matters");
    auto before=table;
    check(!nba97_release_available(nullptr,0,0),"null table guard");
    check(!nba97_release_available(nullptr,2,1),"restriction can short circuit");
    check(nba97_release_available(table.data(),1,255)==1 && table==before,"read only");
    pass("availability_last_slot_only_and_guards");

    table.fill(UINT16_MAX);
    for(unsigned team=0;team<29;++team)for(unsigned slot=0;slot<10;++slot)
        table[team*15+slot]=uint16_t(team*15+slot);
    Nba97TradeScreen screen{};
    for(unsigned count=0;count<100;++count) {
        std::fill(table.begin()+435,table.end(),UINT16_MAX);
        for(unsigned i=0;i<count;++i)table[435+i]=uint16_t(500+i);
        check(nba97_release_begin(&screen,table.data(),29,0,nullptr,7,2)==1,"begin Release");
        check(screen.frontend_state==17 && screen.team[0]==3 && screen.team[1]==29,"signed sentinel normalization");
        check(screen.list_kind[0]==1 && screen.list_kind[1]==0 &&
              nba97_roster_editor_capacity(&screen,0)==15 && nba97_roster_editor_capacity(&screen,1)==100,"descriptor kinds/capacities");
        check(screen.input_callback[0]==nba97_release_callback && !screen.input_callback[1],"single callback binding");
        check(screen.phase==NBA97_TRADE_FIRST && screen.cursor[0]==7 && screen.top[0]==2 &&
              screen.cursor[1]==count && screen.top[1]==std::clamp(int(count)-4,0,94),"entry cursors independent");
        check(std::equal(table.begin(),table.end(),screen.snapshot) &&
              std::equal(table.begin(),table.end(),screen.undo) && !nba97_trade_dirty(&screen),"isolated entry snapshots");
    }
    pass("constructor_100_positions_kinds_callbacks_snapshot");

    std::fill(table.begin()+435,table.end(),UINT16_MAX);
    for(unsigned i=0;i<33;++i)table[435+i]=uint16_t(500+i);
    check(nba97_release_begin(&screen,table.data(),2,0,nullptr,0,0)==1,"navigation setup");
    for(int i=0;i<20;++i)nba97_trade_input(&screen,2,nullptr);
    check(screen.cursor[0]==14 && screen.top[0]==9 && screen.cursor[1]==33 && screen.top[1]==29,"passive free pool");
    check(nba97_trade_input(&screen,2,nullptr)==NBA97_TRADE_IDLE,"row end silent");
    for(int i=0;i<29;++i)check(nba97_trade_input(&screen,4,nullptr)==NBA97_TRADE_TEAM,"donor scans");
    check(screen.team[0]==2 && screen.team[1]==29 && screen.cursor[1]==33,"team wrap never scans receiver");
    check(nba97_trade_input(&screen,0x800,nullptr)==NBA97_TRADE_IDLE,"empty donor confirm silent");
    check(nba97_trade_input(&screen,0x10,nullptr)==NBA97_TRADE_NOTICE && screen.notice.message_address==0x800afc22,"empty donor View notice");
    nba97_trade_dismiss_notice(&screen,0x10);nba97_trade_frame(&screen,0);
    check(nba97_trade_input(&screen,0x40,nullptr)==NBA97_TRADE_NOTICE,"empty donor Compare notice");
    nba97_trade_dismiss_notice(&screen,0x40);nba97_trade_frame(&screen,0);
    check(nba97_trade_input(&screen,0x100,nullptr)==NBA97_TRADE_DISCARD &&
          nba97_trade_result(&screen)==-1,"signed cancel result");
    check(nba97_release_begin(&screen,table.data(),2,0,nullptr,0,0)==1,"reopen");
    check(nba97_trade_input(&screen,0x80,nullptr)==NBA97_TRADE_ACCEPT &&
          nba97_trade_result(&screen)==1 && !nba97_trade_dirty(&screen),"signed accept without mutation");
    pass("single_stage_navigation_empty_notices_signed_exits");

    int8_t eligible[16];for(int i=0;i<16;++i)eligible[i]=int8_t(i+5);
    check(nba97_release_begin(&screen,table.data(),29,2,eligible,0,0)==1 && screen.team[0]==5,"mode2 sentinel");
    check(nba97_release_begin(&screen,table.data(),1,2,eligible,0,0)==1 && screen.team[0]==5,"constructor eligibility fallback");
    check(nba97_release_begin(&screen,table.data(),20,2,eligible,0,0)==1 && screen.team[0]==20,"eligible donor preserved");
    check(!nba97_release_begin(&screen,table.data(),-1,0,nullptr,0,0),"negative donor native guard");
    check(!nba97_release_begin(&screen,table.data(),30,0,nullptr,0,0),"invalid donor native guard");
    check(!nba97_release_begin(&screen,table.data(),0,2,nullptr,0,0),"mode2 requires context");
    check(!nba97_release_begin(&screen,table.data(),0,0,nullptr,15,0),"invalid cursor");
    check(!nba97_release_begin(&screen,table.data(),0,0,nullptr,14,0),"offscreen cursor");
    const auto before_screen=screen;
    table[534]=12;
    check(!nba97_release_begin(&screen,table.data(),0,0,nullptr,0,0) &&
          std::equal(std::begin(before_screen.working),std::end(before_screen.working),screen.working),"invalid compact pool leaves screen intact");
    pass("constructor_mode_context_and_invalid_inputs");

    std::array<uint8_t,700> positions{},injuries{};
    std::array<uint8_t,25> preference{};
    for(unsigned i=0;i<25;++i)preference[i]=uint8_t(i%5);
    const Nba97TradeData data{positions.data(),injuries.data(),preference.data(),positions.size(),1};
    auto setup=[&](unsigned donor_count,unsigned free_count,unsigned selected=0) {
        table.fill(UINT16_MAX);
        for(unsigned t=0;t<29;++t)for(unsigned i=0;i<(t==2?donor_count:10);++i)table[t*15+i]=uint16_t(t*15+i);
        for(unsigned i=0;i<free_count;++i)table[435+i]=uint16_t(500+i);
        check(nba97_release_begin(&screen,table.data(),2,0,nullptr,uint8_t(selected),uint8_t(selected<9?selected:9)),"callback setup");
    };
    for(unsigned n=0;n<100;++n)for(unsigned size=9;size<=15;++size)for(unsigned slot=0;slot<size;++slot) {
        setup(size,n,slot);const auto id=screen.selected[0];
        auto population=table;std::sort(population.begin(),population.end());
        const auto top=screen.top[1];
        check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_SWAPPED,"single-stage release");
        check(screen.phase==NBA97_TRADE_FIRST && screen.counts[2]==size-1 && screen.counts[29]==n+1 && screen.changes==1,"counts and phase");
        check(screen.working[435+n]==id && nba97_trade_dirty(&screen),"released identity appended");
        std::array<uint16_t,535> after;std::copy_n(screen.working,535,after.begin());std::sort(after.begin(),after.end());
        check(after==population,"all identities preserved");
        const bool scroll=n-top>=4 && top!=94;
        check(screen.release_scroll_remaining==(scroll?9:0),"exact scroll threshold");
        if(scroll) for(int tick=0;tick<9;++tick) {
            check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_IDLE,"no repeated release during scroll");
            check(!nba97_trade_frame(&screen,0),"nine frame wait");
        }
        check(screen.cursor[1]==n+1 && screen.top[1]==top+(scroll?1:0),"post-scroll restoration and next vacancy");
        check(nba97_trade_event_sound(NBA97_TRADE_SWAPPED,0x800)==6,"one selector confirm sound");
        check(!std::memcmp(screen.snapshot,table.data(),sizeof(screen.snapshot)),"no save publication");
    }
    pass("release_8400_mutations_population_and_scroll_matrix");

    setup(10,99,5);check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_SWAPPED,"fill last free slot");
    check(screen.cursor[1]==100 && screen.top[1]==94,"original one-past receiver cursor");
    nba97_roster_editor_bind(&screen);check(screen.selected[1]==UINT16_MAX,"one-past binds empty without OOB");
    check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_NOTICE && screen.notice.message_address==0x800aec1e,"full pool notice in open screen");
    nba97_trade_dismiss_notice(&screen,0x800);nba97_trade_frame(&screen,0);
    screen.counts[2]=8;
    check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_NOTICE && screen.notice.message_address==0x800aeb54,"minimum BEFORE full");
    nba97_trade_dismiss_notice(&screen,0x800);nba97_trade_frame(&screen,0);
    screen.mode=1;injuries[screen.selected[0]]=1;
    check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_NOTICE && screen.notice.message_address==0x800aebea &&
          screen.notice.subject==screen.selected[0],"injury BEFORE minimum and full");
    injuries.fill(0);
    pass("refusal_precedence_and_full_pool_one_past_guard");

    for(int mode:{0,1,2})for(int enabled:{0,1})for(int hurt:{0,1}) {
        setup(10,3,5);screen.mode=int16_t(mode);injuries[screen.selected[0]]=uint8_t(hurt);
        auto d=data;d.injuries_enabled=uint8_t(enabled);
        const auto result=nba97_trade_input(&screen,0x800,&d);
        check(result==(mode&&enabled&&hurt?NBA97_TRADE_NOTICE:NBA97_TRADE_SWAPPED),"injury three-part gate");
        injuries.fill(0);
    }
    setup(7,3,5);check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_SWAPPED,"original minimum is equality not <=");
    setup(8,3,5);check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_NOTICE,"eight-player minimum");
    setup(10,3,5);auto saved=screen;
    check(nba97_trade_input(&screen,0x800,nullptr)==NBA97_TRADE_INVALID && !std::memcmp(screen.working,saved.working,sizeof(screen.working)),"missing providers leave draft intact");
    check(nba97_trade_input(&screen,0x200,&data)==NBA97_TRADE_IDLE && !screen.latch,"unknown input silent");
    pass("injury_matrix_exact_minimum_and_provider_guards");

    for(uint16_t child:{uint16_t(0x10),uint16_t(0x40)})for(uint16_t exit:{uint16_t(0x80),uint16_t(0x100)}) {
        setup(10,4,5);
        check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_SWAPPED,"pre-child release");
        for(int i=0;i<9;++i)nba97_trade_frame(&screen,0);
        nba97_roster_editor_bind(&screen);check(screen.selected[1]==UINT16_MAX,"child receiver really empty");
        check(nba97_trade_input(&screen,child,&data)==(child==0x10?NBA97_TRADE_VIEW:NBA97_TRADE_COMPARE),"Release child allowed with empty receiver");
        const int16_t teams[2]={3,4};const uint8_t slots[2]={1,2};
        check(!nba97_trade_child_proposal(&screen,exit,teams,slots),"Release never adopts child browsing");
        check(nba97_trade_return_child(&screen,exit,teams,slots,0),"child reentry");
        check(screen.team[0]==2 && screen.cursor[0]==5 && screen.cursor[1]==5 && screen.top[1]==1,"recomputed receiver and preserved donor");
        check(nba97_trade_dirty(&screen) && !nba97_trade_undo_dirty(&screen) && !screen.changes,"original child checkpoint quirk, durable baseline unchanged");
        nba97_trade_frame(&screen,0);
        check(nba97_trade_input(&screen,0x100,&data)==NBA97_TRADE_DISCARD && nba97_trade_dirty(&screen),"cancel retains pre-child release");
    }
    pass("view_compare_returns_rebase_undo_without_adoption");

    setup(10,3,5);nba97_trade_input(&screen,0x800,&data);
    check(nba97_trade_input(&screen,0x100,&data)==NBA97_TRADE_DISCARD_PROMPT,"dirty cancel prompts");
    check(nba97_trade_discard_answer(&screen,0,0x800)==NBA97_TRADE_IDLE && nba97_trade_dirty(&screen),"keep edit");
    nba97_trade_frame(&screen,0);
    check(nba97_trade_discard_answer(&screen,1,0x800)==NBA97_TRADE_DISCARD && !nba97_trade_dirty(&screen),"discard restores original");
    setup(10,3,5);nba97_trade_input(&screen,0x800,&data);
    check(nba97_trade_input(&screen,0x80,&data)==NBA97_TRADE_ACCEPT && nba97_trade_dirty(&screen),"accept publishes only through host transaction");
    pass("dirty_accept_keep_discard_transactions");

    // Original session: retain Longley's release through Compare/Cancel,
    // reopen, release Parish, discard. Only the second release is undone.
    // Synthetic identities exercise the same sequence without publishing assets.
    setup(12,4,0);
    const auto first_released=screen.selected[0];
    check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_SWAPPED,"checkpoint release");
    for(int i=0;i<9;++i)nba97_trade_frame(&screen,0);
    check(nba97_trade_input(&screen,0x40,&data)==NBA97_TRADE_COMPARE,"checkpoint Compare");
    check(nba97_trade_return_child(&screen,0x80,nullptr,nullptr,0),"checkpoint child return");
    nba97_trade_frame(&screen,0);
    check(nba97_trade_input(&screen,0x100,&data)==NBA97_TRADE_DISCARD,"checkpoint Cancel skips prompt");
    std::array<uint16_t,535> retained;
    std::copy_n(screen.working,535,retained.begin());
    check(retained[439]==first_released,"first release retained");
    check(nba97_release_begin(&screen,retained.data(),2,0,nullptr,0,0),"reopen retained roster");
    const auto second_released=screen.selected[0];
    check(nba97_trade_input(&screen,0x800,&data)==NBA97_TRADE_SWAPPED,"second visit release");
    for(int i=0;i<9;++i)nba97_trade_frame(&screen,0);
    check(screen.working[440]==second_released,"second release appended");
    check(nba97_trade_input(&screen,0x100,&data)==NBA97_TRADE_DISCARD_PROMPT,"second visit Cancel prompts");
    check(nba97_trade_discard_answer(&screen,1,0x800)==NBA97_TRADE_DISCARD,"discard current visit");
    check(std::equal(retained.begin(),retained.end(),screen.working) &&
          screen.working[30]==second_released && screen.working[439]==first_released &&
          screen.working[440]==UINT16_MAX && !nba97_trade_dirty(&screen),"restore second player, preserve first release and every other slot");
    pass("compare_retained_then_reopen_discard_only_new_release");
    return 0;
}
