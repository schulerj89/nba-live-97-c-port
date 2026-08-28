#include "recovered/roster_compare.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
void pass(const char* id) { std::cout<<"COMPARE PASS "<<id<<'\n'; }
static_assert(sizeof(Nba97Compare)==12,"Compare state budget excludes borrowed immutable catalogue/table");
using Table=std::array<std::uint16_t,535>;
Table fixture() {
    Table t; t.fill(UINT16_MAX);
    for(unsigned team=0;team<29;++team) for(unsigned slot=0;slot<13;++slot)
        t[team*15+slot]=static_cast<std::uint16_t>(1000+team*15+slot);
    return t;
}
Nba97Compare opened(const Table& t,unsigned stage=0,unsigned team=0) {
    Nba97ReorderChild child{}; child.state=35; child.parent_page=static_cast<std::uint8_t>(stage);
    child.team=static_cast<std::int16_t>(team); child.cursor[0]=0; child.cursor[1]=7; child.top[1]=2;
    child.player_id[0]=t[team*15]; child.player_id[1]=t[team*15+7];
    Nba97Compare s{};
    check(nba97_compare_begin(&s,&child,t.data())!=0,"Compare entry");
    return s;
}
void run() {
    const auto original=fixture(); auto t=original;
    {Nba97Compare cross{};int16_t teams[]{2,24};uint8_t slots[]{1,7};
     check(nba97_compare_begin_teams(&cross,teams,slots,t.data()) && cross.player[0]==t[31] &&
           cross.player[1]==t[367] && cross.layer==2,"Trade independent Compare identities");
     const auto before=cross;teams[0]=-1;
     check(!nba97_compare_begin_teams(&cross,teams,slots,t.data()) && !std::memcmp(&before,&cross,sizeof(before)),"invalid Trade Compare entry changed state");}
    for(unsigned stage=0;stage<2;++stage) {
        const auto s=opened(t,stage);
        check(s.active_side==0 && s.layer==2 && s.top==0 && s.slot[1]==7 &&
              s.player[1]==1007 && nba97_compare_stat_count(&s)==24,"entry differs by parent phase");
    }
    pass("both_parent_phases_two_identities_left_active");
    auto s=opened(t);
    const auto right=s.player[1];
    check(nba97_compare_input(&s,t.data(),8)==NBA97_COMPARE_PLAYER && s.slot[0]==12 && s.player[1]==right,"left wrap/independence");
    check(nba97_compare_input(&s,t.data(),4)==NBA97_COMPARE_PLAYER && s.slot[0]==0,"right wrap");
    check(nba97_compare_input(&s,t.data(),0x800)==NBA97_COMPARE_SIDE && s.active_side==1,"Cross active-side toggle");
    check(nba97_compare_input(&s,t.data(),4)==NBA97_COMPARE_PLAYER && s.slot[1]==8 && s.slot[0]==0,"right side cycle");
    pass("cross_switches_side_player_wrap_is_independent");
    for(int i=0;i<19;++i) check(nba97_compare_input(&s,t.data(),2)==NBA97_COMPARE_SCROLL,"scroll down extent");
    const auto bottom=s;
    check(s.top==19 && s.active_side==1 && nba97_compare_input(&s,t.data(),2)==NBA97_COMPARE_NO_CHANGE &&
          std::memcmp(&s,&bottom,sizeof(s))==0,"scroll bottom changed state");
    for(int i=0;i<19;++i) check(nba97_compare_input(&s,t.data(),1)==NBA97_COMPARE_SCROLL,"scroll up extent");
    check(s.top==0 && nba97_compare_input(&s,t.data(),1)==NBA97_COMPARE_NO_CHANGE,"scroll top boundary");
    pass("five_row_shared_scroll_limits_preserve_active_side");
    for(int i=0;i<7;++i) nba97_compare_input(&s,t.data(),2);
    check(nba97_compare_input(&s,t.data(),0x2000)==NBA97_COMPARE_LAYER && s.layer==3 && s.top==7,"equal extent reset");
    check(nba97_compare_input(&s,t.data(),0x1000)==NBA97_COMPARE_LAYER && s.layer==2 && s.top==7,"reverse equal extent reset");
    check(nba97_compare_input(&s,t.data(),0x1000)==NBA97_COMPARE_LAYER && s.layer==1 && s.top==0 &&
          nba97_compare_stat_count(&s)==17,"ratings extent reset");
    nba97_compare_input(&s,t.data(),0x1000);
    check(s.layer==0 && nba97_compare_stat_count(&s)==14,"attribute extent");
    nba97_compare_input(&s,t.data(),0x1000); check(s.layer==3,"previous normal layer wrap");
    nba97_compare_input(&s,t.data(),0x2000); check(s.layer==0,"next normal layer wrap");
    pass("layer_wrap_and_extent_dependent_scroll_reset");
    s=opened(t); nba97_compare_input(&s,t.data(),0x800);
    for(int i=0;i<3;++i) nba97_compare_input(&s,t.data(),2);
    // A shorter next team requires walking backward; scrolling and the other side survive.
    for(unsigned i=4;i<15;++i) t[15+i]=UINT16_MAX;
    check(nba97_compare_input(&s,t.data(),0x400)==NBA97_COMPARE_TEAM && s.team[1]==1 && s.slot[1]==3 &&
          s.player[1]==1018 && s.team[0]==0 && s.top==3,"team scan failed to retain/clamp slot");
    check(nba97_compare_input(&s,t.data(),0x200)==NBA97_COMPARE_TEAM && s.slot[1]==3 && s.team[1]==0,"scan reset slot");
    check(nba97_compare_input(&s,t.data(),0x200)==NBA97_COMPARE_TEAM && s.team[1]==28,"empty free agents not skipped");
    pass("team_scan_retains_slot_scroll_skips_empty_free_list");
    t=original;
    for(unsigned i=0;i<100;++i) t[435+i]=static_cast<std::uint16_t>(2000+i);
    s=opened(t);
    check(nba97_compare_input(&s,t.data(),0x200)==NBA97_COMPARE_TEAM && s.team[0]==29,"populated free list omitted");
    check(nba97_compare_input(&s,t.data(),8)==NBA97_COMPARE_PLAYER && s.slot[0]==99 && s.player[0]==2099,"free list100 wrap");
    check(nba97_compare_input(&s,t.data(),0x400)==NBA97_COMPARE_TEAM && s.team[0]==0 && s.slot[0]==0,"free slot>14 not reset on team scan");
    pass("free_agent_hundred_slot_wrap_and_team_return");
    s=opened(t); const auto before=s;
    for(auto mask:{0,0x10,0x20,0x40,0x80,0x100,0x1001,0x4000})
        check(nba97_compare_input(&s,t.data(),static_cast<std::uint16_t>(mask))==NBA97_COMPARE_NO_CHANGE &&
              std::memcmp(&s,&before,sizeof(s))==0,"unhandled mask changed state");
    auto bad=s; bad.player[0]=42; const auto bad_before=bad;
    check(nba97_compare_input(&bad,t.data(),4)==NBA97_COMPARE_INVALID &&
          std::memcmp(&bad,&bad_before,sizeof(bad))==0,"stale identity not rejected atomically");
    bad=s; bad.top=20; check(nba97_compare_input(&bad,t.data(),1)==NBA97_COMPARE_INVALID,"bad scroll accepted");
    check(nba97_compare_input(nullptr,t.data(),1)==NBA97_COMPARE_INVALID &&
          nba97_compare_input(&s,nullptr,1)==NBA97_COMPARE_INVALID,"null guards");
    Nba97ReorderChild child{};
    check(!nba97_compare_begin(&s,&child,t.data()) && std::memcmp(&s,&before,sizeof(s))==0,"invalid begin changed destination");
    pass("exact_masks_stale_identity_and_atomic_rejection");
    // Exhaust every normal team/player/active-side combination; the selected
    // ID must resolve through the borrowed draft, never an original baseline.
    t=original; std::swap(t[0],t[7]); const auto draft=t;
    for(unsigned team=0;team<29;++team) for(unsigned side=0;side<2;++side) {
        s=opened(t,side,team);
        if(side) nba97_compare_input(&s,t.data(),0x800);
        const auto start=s;
        for(unsigned i=0;i<13;++i) {
            nba97_compare_input(&s,t.data(),4);
            check(s.player[side]==t[team*15+s.slot[side]],"cycle bypassed draft ID mapping");
        }
        check(std::memcmp(&s,&start,sizeof(s))==0,"full cycle failed to restore identity");
    }
    check(t==draft,"Compare mutated borrowed draft");
    pass("all_29_teams_both_sides_draft_identity_and_no_writes");
    unsigned sequences=0;
    for(unsigned team=0;team<29;++team) for(unsigned side=0;side<2;++side) for(unsigned mask:{4u,8u}) {
        s=opened(t,side,team);
        if(side) nba97_compare_input(&s,t.data(),0x800);
        s.top=7;
        Nba97CompareRefresh refresh{};
        check(nba97_compare_refresh_begin(&refresh,&s),"refresh entry");
        const auto prior=s;
        check(nba97_compare_refresh_input(&s,&refresh,t.data(),static_cast<uint16_t>(mask))==NBA97_COMPARE_PLAYER &&
            refresh.remaining==2 && std::memcmp(&refresh.text,&prior,sizeof(prior))==0,"text changed before wait");
        const auto requested=s;
        for(unsigned phase=0;phase<2;++phase) {
            const auto frozen=refresh;
            for(unsigned blocked:{1u,2u,4u,8u,0x200u,0x400u,0x800u,0x1000u,0x2000u,0x20u,0x80u,0x100u})
                check(nba97_compare_refresh_input(&s,&refresh,t.data(),static_cast<uint16_t>(blocked))==NBA97_COMPARE_NO_CHANGE &&
                    !std::memcmp(&s,&requested,sizeof(s)) && !std::memcmp(&refresh,&frozen,sizeof(refresh)),"input escaped callback");
            const int cue=nba97_compare_refresh_presented(&refresh,&s);
            check(cue==(phase ? (mask==8 ? 2:1):0),"wrong sound/text ordering");
            check(!std::memcmp(&refresh.text,phase?&requested:&prior,sizeof(prior)),"wrong text phase");
        }
        check(!refresh.remaining && nba97_compare_refresh_presented(&refresh,&s)==0 && s.top==7,"duplicate completion/scroll reset");
        const auto before_team=s;
        check(nba97_compare_refresh_input(&s,&refresh,t.data(),0x400)==NBA97_COMPARE_TEAM &&
            !refresh.remaining && !std::memcmp(&refresh.text,&s,sizeof(s)) && s.team[side]!=before_team.team[side],"team scan acquired player wait");
        auto bad_refresh=refresh; bad_refresh.remaining=3;
        const auto invalid=bad_refresh;
        check(nba97_compare_refresh_presented(&bad_refresh,&s)==-1 &&
            !std::memcmp(&bad_refresh,&invalid,sizeof(invalid)),"bad refresh accepted/mutated");
        ++sequences;
    }
    check(t==draft,"refresh mutated draft");
    std::cout<<"COMPARE REFRESH sequences="<<sequences<<" state_bytes="<<sizeof(Nba97CompareRefresh)<<'\n';
    pass("two_present_text_barrier");
    unsigned free_sequences=0;
    for(unsigned side=0;side<2;++side) for(unsigned mask:{4u,8u}) {
        t=original;
        for(unsigned i=0;i<100;++i) t[435+i]=static_cast<uint16_t>(2000+i);
        s=opened(t);
        if(side) nba97_compare_input(&s,t.data(),0x800);
        nba97_compare_input(&s,t.data(),0x200);
        Nba97CompareRefresh refresh{};
        check(nba97_compare_refresh_begin(&refresh,&s),"free refresh begin");
        check(nba97_compare_refresh_input(&s,&refresh,t.data(),static_cast<uint16_t>(mask))==NBA97_COMPARE_PLAYER &&
            refresh.remaining==2,"hundred-free-agent callback");
        check(nba97_compare_refresh_presented(&refresh,&s)==0 &&
            nba97_compare_refresh_presented(&refresh,&s)==(mask==8?2:1),"free callback sound");
        for(unsigned i=436;i<535;++i) t[i]=UINT16_MAX;
        s.slot[side]=0;s.player[side]=t[435];nba97_compare_refresh_begin(&refresh,&s);
        const auto one=s;const auto held=refresh;
        check(nba97_compare_refresh_input(&s,&refresh,t.data(),static_cast<uint16_t>(mask))==NBA97_COMPARE_NO_CHANGE &&
            !std::memcmp(&s,&one,sizeof(s)) && !std::memcmp(&refresh,&held,sizeof(refresh)),"single free agent generated wait/cue");
        ++free_sequences;
    }
    // Do not generalize the special free-agent check to an ordinary team.
    t=original;for(unsigned i=1;i<15;++i)t[i]=UINT16_MAX;
    s=opened(original);s.slot[1]=0;s.player[1]=t[0];
    Nba97CompareRefresh refresh{};nba97_compare_refresh_begin(&refresh,&s);
    check(nba97_compare_refresh_input(&s,&refresh,t.data(),4)==NBA97_COMPARE_PLAYER && refresh.remaining==2,
        "ordinary one-player team incorrectly suppressed");
    std::cout<<"COMPARE FREE REFRESH sequences="<<free_sequences<<" counts=100/1 ordinary_single=callback\n";
    pass("single_free_agent_suppression_is_not_generic_team_lock");
    Nba97CompareRepeat pacing{};
    unsigned delay_sum=0;
    for(unsigned action=0;action<40;++action) {
        const unsigned counter=2+action*4<48?2+action*4:48;
        const unsigned delay=counter<16?7:counter<28?5:counter<38?3:1;
        check(nba97_compare_repeat_request(&pacing,4)==static_cast<int>(delay) && pacing.counter==counter,
            "repeat accelerates per timer tick instead of accepted poll");
        const auto pending=pacing;
        check(!nba97_compare_repeat_request(&pacing,8) && !std::memcmp(&pending,&pacing,sizeof(pacing)),"post-callback wait bypassed");
        for(unsigned frame=0;frame<=delay;++frame)
            check(nba97_compare_repeat_presented(&pacing)==static_cast<int>(frame==delay),"post delay plus polling frame");
        check(nba97_compare_repeat_presented(&pacing)==0,"duplicate ready event");
        delay_sum+=delay+1;
    }
    check(delay_sum==120,"normal double-pass pacing must total120 post/poll presentations");
    check(nba97_compare_repeat_request(&pacing,8)==7 && pacing.counter==2,"direction change must reset then increment in common tail");
    nba97_compare_repeat_idle(&pacing);
    check(nba97_compare_repeat_request(&pacing,8)==7 && pacing.counter==2,"release must reset then increment in common tail");
    auto bad_pacing=pacing;bad_pacing.post_frames=9;
    check(nba97_compare_repeat_presented(&bad_pacing)==-1 &&
        !nba97_compare_repeat_request(nullptr,4),"pacing guards");
    std::cout<<"COMPARE PACING actions=40 post_and_poll_presents="<<delay_sum<<" state_bytes="<<sizeof(pacing)<<'\n';
    pass("left_right_acceleration_post_delay_and_poll_frame");
    // Independent closed-form result for both source blocks; includes prior
    // masks after release, same direction, and reversal at every legal counter.
    unsigned counter_vectors=0;
    for(unsigned initial=0;initial<=48;initial+=2) for(unsigned previous:{0u,4u,8u})
        for(unsigned mask:{4u,8u}) {
            Nba97CompareRepeat sample{static_cast<uint16_t>(previous),static_cast<uint8_t>(initial),0};
            const unsigned expected=previous==mask ? (initial<44?initial+4:48) : 2;
            const unsigned delay=expected<16?7:expected<28?5:expected<38?3:1;
            check(nba97_compare_repeat_request(&sample,static_cast<uint16_t>(mask))==static_cast<int>(delay) &&
                sample.counter==expected && sample.previous_mask==mask && sample.post_frames==delay+1,
                "normal polling branch and common tail must both record input");
            ++counter_vectors;
        }
    for(unsigned mask=0;mask<=UINT16_MAX;++mask) if(mask!=4 && mask!=8) {
        Nba97CompareRepeat sample{4,46,0};const auto before=sample;
        check(!nba97_compare_repeat_request(&sample,static_cast<uint16_t>(mask)) &&
            !std::memcmp(&sample,&before,sizeof(sample)),"non-direction mask changed pacing state");
    }
    std::cout<<"COMPARE COUNTER normal_record_passes=2 vectors="<<counter_vectors
        <<" rejected_masks=65534 first=2 repeat_step=4 cap=48\n";
    pass("normal_poll_records_counter_in_branch_and_common_tail");
    unsigned callback_masks=0;
    for(unsigned mask=0;mask<=UINT16_MAX;++mask) {
        Nba97CompareRepeat sample{};
        const bool callback=(mask&0x3e50)!=0;
        check(nba97_compare_callback_mask(static_cast<uint16_t>(mask))==int(callback),"generic callback mask routing");
        check(nba97_compare_callback_request(&sample,static_cast<uint16_t>(mask))==(callback?5:0),"generic callback fixed delay");
        if(!callback) { check(!sample.post_frames && !sample.counter,"unhandled callback changed pacing");continue; }
        ++callback_masks;
        auto state=opened(original);const auto before=state;
        const auto event=nba97_compare_input(&state,original.data(),static_cast<uint16_t>(mask));
        const bool supported=mask==0x200 || mask==0x400 || mask==0x800 || mask==0x1000 || mask==0x2000;
        check(supported ? event!=NBA97_COMPARE_NO_CHANGE : event==NBA97_COMPARE_NO_CHANGE &&
            !std::memcmp(&state,&before,sizeof(state)),"unsupported Compare callback must be silent no-op");
        for(unsigned frame=0;frame<6;++frame) {
            const auto pending=sample;
            check(!nba97_compare_callback_request(&sample,0x400) && !nba97_compare_repeat_request(&sample,4) &&
                !std::memcmp(&sample,&pending,sizeof(sample)),"input bypassed callback post wait");
            check(nba97_compare_repeat_presented(&sample)==int(frame==5),"callback must wait five presents then one poll");
        }
        check(sample.counter==2 && nba97_compare_callback_request(&sample,static_cast<uint16_t>(mask))==5 &&
            sample.counter==6,"held callback changed fixed delay or lost shared counter history");
    }
    check(!nba97_compare_callback_request(nullptr,0x400),"null callback pacing");
    std::cout<<"COMPARE CALLBACK masks="<<callback_masks<<" delay=5 poll=1 supported_exact_masks=5 silent_noops_wait=yes\n";
    pass("generic_callback_wait_chords_and_silent_noops");
    unsigned scroll_sequences=0,scroll_endpoints=0;
    for(unsigned layer=0;layer<4;++layer) for(unsigned side=0;side<2;++side)
        for(unsigned top=0;top<20;++top) for(unsigned mask:{1u,2u}) {
            auto state=opened(original);state.layer=static_cast<uint8_t>(layer);
            state.active_side=static_cast<uint8_t>(side);
            const auto count=nba97_compare_stat_count(&state);
            if(top+5>count) continue;
            state.top=static_cast<uint8_t>(top);
            const auto before=state;
            Nba97CompareRepeat pacing{};
            const unsigned post=mask==1 && !top ? 1 : 4;
            check(nba97_compare_scroll_request(&pacing,&before,static_cast<uint16_t>(mask))==int(post),
                "first-row null callback vs dispatched scroll wait");
            check(pacing.post_frames==post && pacing.counter==2,"scroll dispatch poll history");
            Nba97CompareRefresh r{};check(nba97_compare_refresh_begin(&r,&state),"scroll refresh begin");
            const bool endpoint=mask==1 ? top==0 : top+5==count;
            check(nba97_compare_refresh_input(&state,&r,original.data(),static_cast<uint16_t>(mask))==
                (endpoint?NBA97_COMPARE_NO_CHANGE:NBA97_COMPARE_SCROLL),"scroll endpoint decision");
            if(endpoint) {
                check(!r.remaining && !r.cue && !std::memcmp(&state,&before,sizeof(state)),"endpoint generated scroll/cue");
                ++scroll_endpoints;continue;
            }
            const auto target=mask==1 ? top-1 : top+1;
            for(unsigned phase=0;phase<2;++phase) {
                check(r.remaining==2-phase && nba97_compare_refresh_top(&r,0)==(phase?target:top) &&
                    nba97_compare_refresh_top(&r,1)==top,"group0 then group1 presentation order");
                const auto frozen=r;const auto requested=state;
                for(unsigned blocked:{1u,2u,4u,8u,0x20u,0x80u,0x100u,0x400u,0x800u,0x2000u})
                    check(nba97_compare_refresh_input(&state,&r,original.data(),static_cast<uint16_t>(blocked))==NBA97_COMPARE_NO_CHANGE &&
                        !std::memcmp(&r,&frozen,sizeof(r)) && !std::memcmp(&state,&requested,sizeof(state)),"scroll callback input escaped");
                check(nba97_compare_refresh_presented(&r,&state)==(phase ? (mask==1?3:4):0),"scroll cue before both internal pumps");
            }
            check(nba97_compare_refresh_top(&r,0)==target && nba97_compare_refresh_top(&r,1)==target &&
                !r.remaining && !r.cue && nba97_compare_refresh_presented(&r,&state)==0,"scroll completion duplicated or desynchronized");
            check(state.player[0]==before.player[0] && state.player[1]==before.player[1] && state.active_side==side,
                "scroll changed identities or retained temporary group focus");
            ++scroll_sequences;
        }
    check(scroll_sequences==236 && scroll_endpoints==16,"scroll coverage cardinality");
    std::cout<<"COMPARE SCROLL sequences="<<scroll_sequences<<" endpoints="<<scroll_endpoints<<" groups=old/old,new/old,new/new\n";
    pass("scroll_group_order_both_sides_all_layers_and_endpoints");
    auto middle=opened(original);middle.top=1;
    for(unsigned mask=0;mask<=UINT16_MAX;++mask) {
        Nba97CompareRepeat sample{};const bool accepted=mask==1 || mask==2;
        check(nba97_compare_scroll_request(&sample,&middle,static_cast<uint16_t>(mask))==(accepted?4:0),"scroll exact mask/fixed delay");
        if(!accepted) { check(!sample.post_frames && !sample.counter,"non-scroll mask changed counter");continue; }
        for(unsigned action=0;action<40;++action) {
            check(sample.post_frames==4,"scroll accelerated like left/right");
            const auto frozen=sample;
            check(!nba97_compare_scroll_request(&sample,&middle,1) && !nba97_compare_callback_request(&sample,0x400) &&
                !nba97_compare_repeat_request(&sample,4) && !std::memcmp(&sample,&frozen,sizeof(sample)),"pending scroll wait escaped");
            for(unsigned frame=0;frame<4;++frame)
                check(nba97_compare_repeat_presented(&sample)==int(frame==3),"scroll3 delay plus1 poll");
            check(sample.counter==(std::min)(2+action*4,48u),"scroll shared two-pass counter history");
            check(nba97_compare_scroll_request(&sample,&middle,static_cast<uint16_t>(mask))==4,"held scroll stopped");
        }
    }
    check(!nba97_compare_scroll_request(nullptr,&middle,1),"scroll null pacing");
    pass("scroll_fixed_wait_held_input_exact_masks");
    unsigned null_up_polls=0;
    for(unsigned side=0;side<2;++side) for(unsigned layer=0;layer<4;++layer) {
        auto state=opened(original);state.active_side=static_cast<uint8_t>(side);
        state.layer=static_cast<uint8_t>(layer);
        Nba97CompareRepeat pacing{};
        for(unsigned held=0;held<40;++held) {
            check(nba97_compare_scroll_request(&pacing,&state,1)==1 && pacing.post_frames==1,
                "null Up callback incorrectly incurred delay3");
            check(pacing.counter==(std::min)(2+held*4,48u),"null Up input not recorded by selector");
            const auto frozen=pacing;
            check(!nba97_compare_scroll_request(&pacing,&state,2) &&
                !std::memcmp(&pacing,&frozen,sizeof(pacing)),"input escaped null-Up poll frame");
            check(nba97_compare_repeat_presented(&pacing)==1 && !pacing.post_frames,"null Up needs exactly one poll");
            ++null_up_polls;
        }
        check(nba97_compare_scroll_request(&pacing,&state,2)==4,"Down at first row incorrectly disabled");
        for(unsigned frame=0;frame<4;++frame) nba97_compare_repeat_presented(&pacing);
        // A valid Up landing on zero must use the PRE-input state, not the target.
        state.top=1;const auto before=state;
        check(nba97_compare_input(&state,original.data(),1)==NBA97_COMPARE_SCROLL && !state.top,
            "Up-to-first-row transition");
        check(nba97_compare_scroll_request(&pacing,&before,1)==4,"Up landing at zero lost its callback delay");
        for(unsigned frame=0;frame<4;++frame) nba97_compare_repeat_presented(&pacing);
        state.top=static_cast<uint8_t>(nba97_compare_stat_count(&state)-5);
        check(nba97_compare_scroll_request(&pacing,&state,2)==4,"bottom Down must retain callback delay");
    }
    Nba97CompareRepeat invalid_pacing{};
    check(!nba97_compare_scroll_request(&invalid_pacing,nullptr,1),"missing scroll state accepted");
    for(unsigned corruption=0;corruption<4;++corruption) {
        auto state=opened(original);
        if(corruption==0) state.initialized=0;
        if(corruption==1) state.active_side=2;
        if(corruption==2) state.layer=4;
        if(corruption==3) state.top=255;
        check(!nba97_compare_scroll_request(&invalid_pacing,&state,1) && !invalid_pacing.counter &&
            !invalid_pacing.post_frames,"invalid state partially changed scroll pacing");
    }
    std::cout<<"COMPARE NULL-UP polls="<<null_up_polls<<" sides=2 layers=4 callback=none delay=0 poll=1 source=5A1EC/3D930\n";
    pass("first_row_up_callback_disabled_both_groups_preserves_poll");
    unsigned animation_vectors=0;
    for(unsigned flags=0;flags<256;++flags) for(unsigned a=0;a<256;++a) for(unsigned b=0;b<256;++b) {
        const unsigned expected=(flags&16) && (a^128u)<(b^128u) ? 16u :
            (flags&8) && a<b ? 8u : 0u;
        check(nba97_compare_animation_pending(static_cast<uint8_t>(flags),static_cast<uint8_t>(a),
            static_cast<uint8_t>(b),static_cast<uint8_t>(a),static_cast<uint8_t>(b))==expected,
            "display animation flags/signedness/precedence");
        ++animation_vectors;
    }
    check(nba97_compare_animation_pending(24,0,1,0,1)==16 &&
          nba97_compare_animation_pending(24,0,1,1,0)==8 &&
          nba97_compare_animation_pending(24,1,0,0,1)==16 &&
          nba97_compare_animation_pending(24,1,0,1,0)==0,"independent animation channels");
    std::cout<<"COMPARE ANIMATION vectors="<<animation_vectors<<" source=2C610\n";
    pass("display_animation_wait_is_not_keyboard_release");
    for(unsigned flags=0;flags<256;++flags) {
        const auto rebuilt=nba97_compare_rebuilt_text_flags(static_cast<uint8_t>(flags));
        check(rebuilt==(flags&199u),"2C244 color-only flag copy");
        check(nba97_compare_animation_pending(rebuilt,0,255,128,127)==0,
            "rebuilt selected text still delays the next player-cycle poll");
    }
    pass("rebuilt_selected_text_clears_geometry_wait_flags");
}
}
int main() {
    try { run(); std::cout<<"COMPARE SUMMARY 18 controller scenarios; state=12 plus14 refresh plus4 pacing bytes; original UI/audio parity not asserted\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"COMPARE FAIL "<<e.what()<<'\n'; return 1; }
}
