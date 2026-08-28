#include "main_menu.hpp"
#include "recovered/reorder_children.h"
#include "recovered/roster_compare.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
static_assert(sizeof(Nba97ReorderChild)==16,"child return-context memory budget");
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
void pass(const std::string& name) { std::cout<<"CHILD PASS "<<name<<'\n'; }
// Tiny synthetic v1 catalogue; no copyrighted names, stats, or original IDs.
// Loading the public file format exercises the real loader, not a test-only
// constructor or mutable backdoor to the immutable catalogue.
struct Fixture {
    std::filesystem::path directory, path;
    Fixture() {
        const auto seed=std::chrono::steady_clock::now().time_since_epoch().count();
        for(int i=0;i<100;++i) {
            const auto candidate=std::filesystem::temp_directory_path()/
                ("nba97-child-test-"+std::to_string(seed)+"-"+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) { directory=candidate; break; }
        }
        check(!directory.empty(),"cannot create isolated fixture directory");
        path=directory/"synthetic.n97db";
        constexpr unsigned players=29*13, player_offset=84, team_offset=player_offset+players*61;
        constexpr unsigned string_offset=team_offset+29*74, file_size=string_offset+10;
        std::vector<std::uint8_t> b(file_size,0);
        auto half=[&](unsigned at,unsigned v) { b[at]=v&255; b[at+1]=(v>>8)&255; };
        auto word=[&](unsigned at,unsigned v) { half(at,v); half(at+2,v>>16); };
        std::copy_n("N97RDB\0\0",8,b.begin()); word(8,1); word(12,0x12345678); word(16,3); word(20,file_size);
        auto section=[&](unsigned at,const char* tag,unsigned offset,unsigned size,unsigned count,unsigned stride) {
            std::copy_n(tag,4,b.begin()+at); word(at+4,offset); word(at+8,size); word(at+12,count); word(at+16,stride);
        };
        section(24,"PLAY",player_offset,players*61,players,61);
        section(44,"TEAM",team_offset,29*74,29,74);
        section(64,"STRS",string_offset,10,10,1);
        std::copy_n("Synthetic\0",10,b.begin()+string_offset);
        for(unsigned i=0;i<players;++i) {
            const unsigned at=player_offset+i*61;
            half(at,1000+i); b[at+7]=static_cast<std::uint8_t>(i%99); b[at+8]=i%5;
            std::fill_n(b.begin()+at+14,17,50);
        }
        for(unsigned i=0;i<29;++i) {
            const unsigned at=team_offset+i*74;
            half(at,i); half(at+2,13);
            for(unsigned j=0;j<15;++j) half(at+24+2*j,j<13 ? 1000+i*13+j : 0xffff);
        }
        std::ofstream out(path,std::ios::binary);
        out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
        check(bool(out),"cannot write synthetic fixture");
    }
    ~Fixture() {
        std::error_code ignored;
        std::filesystem::remove(path,ignored);
        std::filesystem::remove(directory,ignored); // Never recursive; only this test's empty directory.
    }
};

void core() {
    std::array<std::uint16_t,535> table{};
    for(std::size_t i=0;i<table.size();++i) table[i]=static_cast<std::uint16_t>(i);
    for(unsigned stage=0;stage<2;++stage) for(unsigned compare=0;compare<2;++compare) {
        Nba97ReorderScreen parent{}; Nba97ReorderChild child{};
        check(nba97_reorder_screen_enter(&parent,table.data(),3,0,nullptr,nullptr,nullptr,0),"entry");
        if(stage) {
            nba97_reorder_screen_input(&parent,NBA97_REORDER_SELECT);
            for(int i=0;i<7;++i) nba97_reorder_screen_input(&parent,NBA97_REORDER_DOWN);
        }
        const auto before=parent;
        const auto mask=static_cast<std::uint16_t>(compare ? 0x40 : 0x10);
        const auto result=nba97_reorder_child_begin(&parent,&child,mask);
        check(result==(compare ? NBA97_REORDER_REQUEST_COMPARE : NBA97_REORDER_REQUEST_VIEW),"child request");
        check(child.state==(compare ? 35 : 36) && child.parent_page==stage && child.team==3,"route/page/team");
        check(child.player_id[0]==(compare ? 45 : (stage ? 52 : 45)) &&
              child.player_id[1]==(compare ? (stage ? 52 : 45) : UINT16_MAX),"context IDs");
        check(child.cursor[1]==(stage ? 7 : 0) && child.top[1]==(stage ? 2 : 0),"absolute-to-local context");
        check(!nba97_reorder_child_input_ready(&child,mask) && nba97_reorder_child_input_ready(&child,0),"invoking input barrier");
        const auto suspended=parent;
        check(nba97_reorder_screen_input(&parent,NBA97_REORDER_SELECT)==NBA97_REORDER_NO_CHANGE &&
              !nba97_reorder_screen_scan(&parent,1) && std::memcmp(&suspended,&parent,sizeof(parent))==0,"parent not frozen");
        check(nba97_reorder_child_begin(&parent,&child,mask)==NBA97_REORDER_NO_CHANGE,"nested duplicate child");
        const auto exit_mask=static_cast<std::uint16_t>(stage ? 0x80 : 0x100);
        check(!nba97_reorder_child_return(&parent,&child,0x800),"nonexit button returned");
        check(nba97_reorder_child_return(&parent,&child,exit_mask) && !child.state,"child return");
        auto expected=before;
        expected.selection.input_latch=0; expected.selection.waiting_input_change=1; expected.selection.held_mask=exit_mask;
        check(std::memcmp(&expected,&parent,sizeof(parent))==0,"return changed parent state beyond input barrier");
        check(!nba97_reorder_frame(&parent.selection,exit_mask) && nba97_reorder_frame(&parent.selection,0),"return held-input barrier");
    }
    pass("original_view_compare_request_contexts");
    pass("parent_frozen_and_both_return_masks");
    pass("child_opener_and_return_input_barriers");
    for(auto mask : {0,0x11,0x50,0x100,0x800}) {
        Nba97ReorderScreen p{}; Nba97ReorderChild c{};
        nba97_reorder_screen_enter(&p,table.data(),0,0,nullptr,nullptr,nullptr,0);
        const auto before=p;
        check(nba97_reorder_child_begin(&p,&c,static_cast<std::uint16_t>(mask))==NBA97_REORDER_NO_CHANGE &&
              std::memcmp(&p,&before,sizeof(p))==0,"inexact child mask accepted");
    }
    pass("exact_child_masks_only");
    for(unsigned stage=0;stage<2;++stage) for(auto mask : {0x10,0x40}) {
        auto empty=table; empty[0]=UINT16_MAX;
        Nba97ReorderScreen p{}; Nba97ReorderChild c{};
        nba97_reorder_screen_enter(&p,empty.data(),0,0,nullptr,nullptr,nullptr,0);
        if(stage) nba97_reorder_begin_second(&p.selection);
        check(nba97_reorder_child_begin(&p,&c,static_cast<std::uint16_t>(mask))==NBA97_REORDER_REJECTED_EMPTY &&
            !c.state && !p.selection.screen_result && p.selection.modal!=NBA97_REORDER_MODAL_NONE,"empty child launched");
    }
    pass("empty_view_and_compare_rejected_both_stages");
}

void viewTeamScan(const std::filesystem::path& path) {
    nba97::RosterDatabase base; base.load(path);
    const auto original=base.slotTable();
    // Synthetic team1 has every populated length from 1..13. Keep the whole
    // population, moving the tail into the free-agent list via the public API.
    // 59ABC keeps a <=14 slot and backs over -1 entries; it does NOT reset the
    // player/stat indices to zero. Exercise both directions and every source slot.
    for(unsigned count=1;count<=13;++count) {
        auto table=original;
        for(unsigned i=count;i<13;++i) {
            table[435+i-count]=table[15+i]; table[15+i]=UINT16_MAX;
        }
        const auto draft=base.prepareSlotTable(table);
        for(int direction : {-1,1}) for(unsigned slot=0;slot<13;++slot) {
            nba97::RosterViewer viewer;
            const auto team=static_cast<std::int16_t>(direction<0 ? 2 : 0);
            check(viewer.openPlayerFromRoster(draft,
                {static_cast<std::int16_t>(slot),static_cast<std::int16_t>(slot>5 ? slot-5 : 0),team},
                {0,false,12,0}),"team scan View entry");
            check(viewer.move(0,1,draft),"team scan stat scroll setup");
            check(viewer.scanTeam(direction,draft,1234),"team scan failed");
            const auto wanted=std::min(slot,count-1);
            check(viewer.teamIndex()==1 && viewer.playerIndex()==wanted &&
                  viewer.selectedPlayer(draft)->id==table[15+wanted],"59ABC slot retain/backtrack mismatch");
            check(viewer.firstVisiblePlayerStat()==1 && viewer.category()==2,
                  "59ABC changed stat scroll/layer");
            check(viewer.paletteFromTeamIndex()==static_cast<unsigned>(team) &&
                  viewer.paletteTransitionStartMs()==1234,"team scan lost palette source/timestamp");
            check(!viewer.scanTeam(0,draft,9999) && viewer.playerIndex()==wanted &&
                  viewer.paletteTransitionStartMs()==1234,"zero team input changed state");
        }
    }
    pass("synthetic_view_team_scan_338_slot_retention_cases");
    auto empty_table=original;
    for(unsigned i=0;i<13;++i) { empty_table[435+i]=empty_table[15+i]; empty_table[15+i]=UINT16_MAX; }
    const auto empty=base.prepareSlotTable(empty_table);
    nba97::RosterViewer guarded;
    check(guarded.openPlayerFromRoster(empty,{7,2,0},{0,false,12,0}),"empty target setup");
    guarded.move(0,1,empty);
    check(!guarded.scanTeam(1,empty,1234) && guarded.teamIndex()==0 && guarded.playerIndex()==7 &&
          guarded.firstVisiblePlayer()==2 && guarded.firstVisiblePlayerStat()==1 &&
          guarded.paletteTransitionStartMs()==0,"empty target partially published/underflowed");
    pass("synthetic_view_empty_team_guard");
    // 59610 only touches the second descriptor group for layout23 (Compare).
    // The real View entry 5A538 sets layout24, not the old native default23.
    nba97::RosterViewer viewer;
    check(viewer.openPlayerFromRoster(base,{4,0,0},{0,false,12,0}),"View layout setup");
    viewer.move(0,1,base); viewer.cycleCategory(1);
    const auto same=viewer.lastStatLayerChange();
    check(!same.secondary_layout && !same.secondary_animation_reset && same.secondary_refresh_count==0 &&
          same.primary_refresh_count==6 && viewer.firstVisiblePlayerStat()==1,"View falsely refreshed Compare column");
    viewer.cycleCategory(1);
    const auto changed=viewer.lastStatLayerChange();
    check(changed.primary_animation_reset && !changed.secondary_animation_reset &&
          changed.secondary_refresh_count==0 && viewer.firstVisiblePlayerStat()==0,"View layer extent reset wrong group");
    check(base.slotTable()==original,"View scan/layer mutated base");
    pass("synthetic_view_layout24_single_group_refresh");
}

void catalogueAndView(const std::filesystem::path& path,const std::string& prefix) {
    nba97::RosterDatabase live; live.load(path);
    const auto original=live.slotTable();
    for(unsigned team=0;team<29;++team) for(unsigned slot=0;slot<15;++slot) {
        if(original[team*15+slot]==UINT16_MAX) break;
        for(int direction : {-1,1}) {
            nba97::RosterViewer viewer;
            check(viewer.openPlayerFromRoster(live,
                {static_cast<std::int16_t>(slot),static_cast<std::int16_t>(slot>5 ? slot-5 : 0),
                 static_cast<std::int16_t>(team)},{0,false,12,0}),"catalogue scan entry");
            viewer.move(0,1,live);
            const unsigned next=(team+29+direction)%29;
            unsigned wanted=slot;
            while(wanted && original[next*15+wanted]==UINT16_MAX) --wanted;
            check(viewer.scanTeam(direction,live) && viewer.teamIndex()==next &&
                  viewer.playerIndex()==wanted && viewer.selectedPlayer(live)->id==original[next*15+wanted] &&
                  viewer.firstVisiblePlayerStat()==1,"catalogue scan identity/scroll/wrap mismatch");
        }
    }
    check(live.slotTable()==original,"catalogue scan mutated live slots");
    pass(prefix+"view_all_normal_team_slots_scan_both_directions");
    Nba97ReorderScreen parent{};
    nba97_reorder_screen_enter(&parent,original.data(),0,0,nullptr,nullptr,nullptr,0);
    auto swap=[&] {
        nba97_reorder_screen_input(&parent,NBA97_REORDER_SELECT);
        nba97_reorder_screen_input(&parent,NBA97_REORDER_DOWN);
        nba97_reorder_screen_input(&parent,NBA97_REORDER_SELECT);
    };
    swap(); nba97_reorder_screen_scan(&parent,1); swap();
    check(parent.selection.changes==2,"two-team draft");
    for(unsigned stage=0;stage<2;++stage) {
        if(stage) {
            nba97_reorder_screen_input(&parent,NBA97_REORDER_SELECT);
            for(int i=0;i<6;++i) nba97_reorder_screen_input(&parent,NBA97_REORDER_DOWN);
        }
        const auto before=parent;
        Nba97ReorderChild context{};
        check(nba97_reorder_child_begin(&parent,&context,0x10)==NBA97_REORDER_REQUEST_VIEW,"view request");
        const auto draft=live.draftView(parent);
        check(&draft.players()==&live.players(),"player catalogue was cloned");
        for(unsigned team=0;team<29;++team) {
            const auto* a=draft.team(team); const auto* b=live.team(team);
            check(a->nickname.data()==b->nickname.data() && a->city.data()==b->city.data() &&
                  a->alternate_name.data()==b->alternate_name.data() && a->location.data()==b->location.data() &&
                  a->abbreviation.data()==b->abbreviation.data(),"immutable team names were cloned");
        }
        for(const auto& player : live.players())
            check(draft.player(player.id)==&player && draft.rosterOwner(player.id)==live.rosterOwner(player.id),"shared player identity/owner");
        const auto projected=draft.slotTable();
        check(std::equal(projected.begin(),projected.end(),parent.working) &&
              projected[0]!=original[0] && projected[15]!=original[15] &&
              live.slotTable()==original,"lost staged teams or published draft");
        nba97::RosterViewer viewer;
        check(viewer.openPlayerFromRoster(draft,{context.cursor[stage],context.top[stage],context.team},{0,false,12,static_cast<std::uint8_t>(stage)}),"direct View entry");
        const auto& run=viewer.playerCardRunState();
        check(viewer.mode()==nba97::RosterViewMode::PlayerCard && run.selected_player_id==context.player_id[0] &&
              run.parent_active_page==static_cast<int>(stage) && !run.parent_roster_viewer_selected &&
              run.layout_id==36 && run.visible_row_count==6 && !viewer.runDrawCallbackBound(),"wrong parent-aware View construction");
        const auto starting=viewer.playerIndex();
        const auto count=std::count_if(draft.team(context.team)->roster.begin(),draft.team(context.team)->roster.end(),
                                     [](auto id) { return id!=UINT16_MAX; });
        viewer.move(0,1,draft);
        for(int i=0;i<count;++i) check(viewer.cyclePlayer(1,draft),"draft player cycle");
        check(viewer.playerIndex()==starting && viewer.firstVisiblePlayerStat()==1,"wrap or stat retention");
        check(viewer.cycleCategory(1) && viewer.category()==3 && viewer.firstVisiblePlayerStat()==1 &&
              viewer.lastStatLayerChange().layer_label_object==27 &&
              !viewer.lastStatLayerChange().descriptor_extent_changed,
              "same-extent layer switch lost stat scroll or mislabeled text redraw");
        check(viewer.cycleCategory(-1) && viewer.firstVisiblePlayerStat()==1,
              "reverse same-extent layer lost scroll");
        check(viewer.cycleCategory(-1) && viewer.category()==1 && viewer.firstVisiblePlayerStat()==0,
              "changed descriptor extent did not reset scroll");
        check(viewer.cycleCategory(1) && viewer.category()==2,"restore layer after regression");
        check(viewer.scanTeam(1,draft) && viewer.cycleCategory(1),"child team/layer browsing");
        check(viewer.selectedTeam(draft)->id==2 && viewer.category()==3,"child browse result");
        check(nba97_reorder_child_return(&parent,&context,0x80),"return after browse");
        auto expected=before;
        expected.selection.input_latch=0; expected.selection.waiting_input_change=1; expected.selection.held_mask=0x80;
        check(std::memcmp(&parent,&expected,sizeof(parent))==0 && live.slotTable()==original,"child adopted selection/published roster");
        nba97_reorder_frame(&parent.selection,0);
    }
    pass(prefix+"shared_catalogue_multi_team_projection");
    pass(prefix+"both_stage_view_browse_wrap_return");
    for(unsigned stage=0;stage<2;++stage) {
        // The actual editor remains in replacement after the View tests.
        // A temporary parent exercises both entry stages without altering it.
        auto compare_parent=parent;
        if(!stage) {
            nba97_reorder_screen_input(&compare_parent,NBA97_REORDER_CANCEL);
            nba97_reorder_frame(&compare_parent.selection,0);
        }
        const auto before=compare_parent;
        Nba97ReorderChild context{}; Nba97Compare compare{};
        check(nba97_reorder_child_begin(&compare_parent,&context,0x40)==NBA97_REORDER_REQUEST_COMPARE,"Compare request");
        const auto compare_draft=live.draftView(compare_parent);
        const auto table=compare_draft.slotTable();
        check(nba97_compare_begin(&compare,&context,table.data()) && compare.active_side==0 &&
              compare.player[0]==context.player_id[0] && compare.player[1]==context.player_id[1],"Compare draft entry identities");
        check(&compare_draft.players()==&live.players(),"Compare copied catalogue");
        for(unsigned side=0;side<2;++side) {
            if(side) nba97_compare_input(&compare,table.data(),0x800);
            for(int i=0;i<3;++i) nba97_compare_input(&compare,table.data(),4);
            check(nba97_compare_input(&compare,table.data(),0x400)==NBA97_COMPARE_TEAM &&
                  compare_draft.player(compare.player[side])!=nullptr,"Compare browse lost catalogue ID");
        }
        nba97_compare_input(&compare,table.data(),2);
        nba97_compare_input(&compare,table.data(),0x2000);
        check(compare.top==1 && compare.layer==3 && compare.active_side==1,"Compare shared layer/scroll");
        check(nba97_reorder_child_return(&compare_parent,&context,0x100),"Compare return");
        auto expected=before; expected.selection.input_latch=0;
        expected.selection.waiting_input_change=1; expected.selection.held_mask=0x100;
        check(std::memcmp(&compare_parent,&expected,sizeof(expected))==0 && live.slotTable()==original,
              "Compare return adopted browsed identities or published draft");
    }
    pass(prefix+"compare_draft_both_stage_return");
    auto draft=live.draftView(parent);
    auto rejected=[&](Nba97ReorderScreen bad) {
        bool refused=false;
        try { (void)live.draftView(bad); } catch(const std::runtime_error&) { refused=true; }
        check(refused && live.slotTable()==original,"invalid draft accepted/mutated live");
    };
    auto bad=parent; bad.snapshot[0]=UINT16_MAX; rejected(bad);
    bad=parent; bad.working[435]=original[0]; rejected(bad);
    bad=parent; bad.working[0]=UINT16_MAX; rejected(bad);
    bad=parent; bad.selection.slots[0]=UINT16_MAX; rejected(bad);
    bad=parent; bad.selection.accepted=1; rejected(bad);
    bad=parent; bad.team=-1; rejected(bad);
    pass(prefix+"stale_membership_and_lifecycle_rejected");
    const auto* retained=draft.player(original[0]);
    const auto retained_team=*live.team(0);
    const auto retained_name=retained_team.displayName();
    live.load(path);
    check(&live.players()!=&draft.players() && retained==draft.player(original[0]) &&
          retained->id==original[0],"reload invalidated held catalogue");
    check(retained_team.city.data()==draft.team(0)->city.data() &&
          retained_team.city.data()!=live.team(0)->city.data() && retained_team.displayName()==retained_name,
          "reload invalidated standalone shared team names");
    pass(prefix+"projection_lifetime_survives_catalogue_reload");
    nba97::RosterViewer invalid;
    check(!invalid.openPlayerFromRoster(draft,{15,0,0},{0,false,12,0}) &&
          !invalid.openPlayerFromRoster(draft,{0,0,-1},{0,false,12,0}),"invalid View entry accepted");
    pass(prefix+"view_entry_guards");
    auto accepted=parent;
    nba97_reorder_screen_input(&accepted,NBA97_REORDER_CANCEL); // replacement -> first
    nba97_reorder_frame(&accepted.selection,0);
    nba97_reorder_screen_input(&accepted,NBA97_REORDER_ACCEPT);
    auto publication=live;
    check(publication.applyReorderScreen(accepted) && publication.slotTable()==draft.slotTable() &&
          live.slotTable()==original && &publication.players()==&live.players(),"accept after child lost/duplicated data");
    pass(prefix+"accept_after_child_atomic_in_memory");
    nba97_reorder_screen_input(&parent,NBA97_REORDER_CANCEL);
    nba97_reorder_frame(&parent.selection,0);
    check(nba97_reorder_screen_input(&parent,NBA97_REORDER_CANCEL)==NBA97_REORDER_ASK_DISCARD,"missing discard after child");
    nba97_reorder_screen_input(&parent,NBA97_REORDER_DISCARD_YES);
    check(std::equal(original.begin(),original.end(),parent.working) && live.slotTable()==original,
          "discard after child lost original entry baseline");
    pass(prefix+"discard_after_child_restores_whole_entry");
}
}

int main(int argc,char** argv) {
    try {
        if(argc>2) throw std::runtime_error("usage: nba97_reorder_child_tests [private roster.n97db]");
        core();
        Fixture fixture; viewTeamScan(fixture.path); catalogueAndView(fixture.path,"synthetic_");
        if(argc==2) catalogueAndView(argv[1],"private_");
        std::cout<<"CHILD SUMMARY View round trips; Compare draft/controller/return tested; UI covered separately by host captures; disk writes=none\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"CHILD FAIL "<<e.what()<<'\n'; return 1; }
}
