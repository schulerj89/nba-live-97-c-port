#include "recovered/roster_lists.h"
#include "recovered/roster_reorder.h"
#include "roster_database.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
void pass(const char* id, const char* why) { std::cout << "REORDER PASS " << id << " | " << why << '\n'; }
struct Fixture {
    std::array<uint16_t,100> a{}, b{};
    std::array<uint16_t,30> counts{};
    std::array<uint8_t,512> injury{}, position{};
    // Synthetic rankings, not extracted game data. Provider remains external.
    std::array<uint8_t,25> preference{};
    Nba97RosterList lists[2]{};
    Nba97RosterValidation rules{};
    Nba97RosterCompaction compaction{};
    uint16_t changes=0;
    Fixture() {
        a.fill(UINT16_MAX); b.fill(UINT16_MAX); counts.fill(10);
        for(int i=0;i<10;++i) { a[i]=uint16_t(100+i); b[i]=uint16_t(200+i); }
        for(int i=0;i<25;++i) preference[i]=uint8_t(i%5);
        lists[0]={a.data(),15,0,15,1,0,0,0};
        lists[1]={b.data(),15,1,15,1,15,15,15};
        rules={0,12,0,counts.data(),injury.data(),injury.size()};
        compaction={counts.data(),position.data(),injury.data(),preference.data(),position.size(),0,0};
    }
    Nba97RosterDecision validate() { return nba97_roster_validate(lists,&rules); }
    int mutate() { return nba97_roster_mutate(lists,counts.data(),&changes,nba97_roster_compact,&compaction); }
};
void validation() {
    Fixture f;
    f.rules.mode=1; f.rules.frontend_state=13; f.rules.injuries_enabled=1; f.injury[200]=1;
    auto d=f.validate();
    check(d.result==0 && d.notice==NBA97_ROSTER_NOTICE_INJURED &&
          d.message_address==0x800aebb2 && d.subject==200,"injured notice/player identity");
    f.lists[0].team=29; f.counts[1]=15;
    check(f.validate().notice==NBA97_ROSTER_NOTICE_INJURED,"injury before full-team check");
    f.rules.mode=0; check(f.validate().result==-1,"mode gate");
    f.rules.mode=1; f.rules.injuries_enabled=0; check(f.validate().result==-1,"injuries gate");
    f.rules.injuries_enabled=1; f.rules.frontend_state=12; check(f.validate().result==-1,"state gate");
    f.rules.frontend_state=13; f.b[0]=UINT16_MAX;
    check(f.validate().result==-1,"empty sentinel never reads injury[-1]");
    pass("shared_validate_injury","800556B0: mode/injuries/state13/second-ID gates; injury notice precedes free-agent/full checks");
    f.rules.mode=0; f.a[0]=UINT16_MAX;
    check(f.validate().result==0 && f.validate().notice==NBA97_ROSTER_NOTICE_NONE,"empty free agent");
    f.a[0]=100;
    for (int n : {0,8,14,15,16}) {
        f.counts[1]=uint16_t(n); d=f.validate();
        check(d.result==(n==15?-1:1) && d.notice==NBA97_ROSTER_NOTICE_NONE,"exact capacity sentinel");
    }
    pass("shared_validate_free_agents","team29 empty rejects; exact count15 returns signed -1, others1; no modal manufactured");
    Fixture m; m.a[0]=UINT16_MAX; m.counts[1]=8;
    d=m.validate(); check(d.result==0 && d.notice==NBA97_ROSTER_NOTICE_MINIMUM &&
        d.subject==1 && d.message_address==0x800aecbe,"second donor minimum");
    m.a[0]=100; m.b[0]=UINT16_MAX; m.counts[0]=8;
    d=m.validate(); check(d.result==0 && d.subject==0,"first donor minimum");
    m.counts[0]=7; check(m.validate().result==1,"minimum uses equality, not <=");
    m.counts[0]=9; check(m.validate().result==1,"above minimum");
    pass("shared_validate_minimum","both transfer directions identify donor; exact eight-player rule and original dialog address");
    Fixture r; r.lists[1].kind=2;
    check(r.validate().result==1,"different occupied reorder");
    r.b[0]=100; check(r.validate().result==0 && r.validate().notice==NBA97_ROSTER_NOTICE_NONE,"same silent");
    r.b[0]=UINT16_MAX; r.counts[0]=8;
    d=r.validate(); check(d.notice==NBA97_ROSTER_NOTICE_EMPTY && d.message_address==0x800afffa,"kind2 overrides minimum dialog");
    r.a[0]=UINT16_MAX;
    check(r.validate().notice==NBA97_ROSTER_NOTICE_NONE && r.validate().result==0,"both empty silent before kind2 modal");
    r.b[0]=200; check(r.validate().notice==NBA97_ROSTER_NOTICE_EMPTY,"first empty modal");
    r.a[0]=200; r.lists[1].kind=1;
    check(r.validate().result==1,"shared non-reorder same identity permitted by this helper");
    pass("shared_validate_reorder","kind2 occupied/different; one empty modal, both empty silent; no extra same-ID rule in kind1");
    // Exhaustive compact decision grid, independent expected expression.
    unsigned cases=0;
    for(int free=0;free<2;++free) for(int kind:{1,2}) for(int a:{-1,100}) for(int b:{-1,100,200})
    for(int ca:{7,8,9,15}) for(int cb:{7,8,9,15}) for(int injured=0;injured<2;++injured) {
        Fixture v; v.lists[0].team=free?29:0; v.lists[1].kind=int16_t(kind);
        v.a[0]=uint16_t(a); v.b[0]=uint16_t(b); v.counts[v.lists[0].team]=uint16_t(ca); v.counts[1]=uint16_t(cb);
        v.rules.mode=1; v.rules.frontend_state=13; v.rules.injuries_enabled=1;
        if(b>=0) v.injury[b]=uint8_t(injured);
        int expected; Nba97RosterNotice notice=NBA97_ROSTER_NOTICE_NONE;
        if(injured && b>=0) { expected=0; notice=NBA97_ROSTER_NOTICE_INJURED; }
        else if(free) expected=a<0?0:cb==15?-1:1;
        else if(a<0 && b<0) expected=0;
        else if(kind==2) { expected=a>=0 && b>=0 && a!=b; if(a<0 || b<0) notice=NBA97_ROSTER_NOTICE_EMPTY; }
        else { expected=!((a<0 && cb==8)||(b<0 && ca==8)); if(!expected) notice=NBA97_ROSTER_NOTICE_MINIMUM; }
        auto before_a=v.a, before_b=v.b; const auto before_counts=v.counts;
        d=v.validate(); check(d.result==expected && d.notice==notice,"decision matrix");
        check(v.a==before_a && v.b==before_b && v.counts==before_counts,"validation immutable"); ++cases;
    }
    check(cases==768,"matrix case denominator");
    pass("shared_validate_matrix","768 synthetic decision combinations; result/notice/immutability, not original execution parity");
    Fixture bad; auto original=bad.a;
    bad.lists[1].cursor=14; check(bad.validate().result==0,"invalid cursor guard");
    bad.lists[1].cursor=15; bad.rules.counts=nullptr; check(bad.validate().result==0,"missing counts");
    bad.rules.counts=bad.counts.data(); bad.rules.mode=1; bad.rules.frontend_state=13; bad.rules.injuries_enabled=1;
    bad.b[0]=600; check(bad.validate().result==0 && bad.a==original,"invalid player guard");
    pass("shared_validate_guards","native malformed descriptor/data guards reject without mutation; not original-instruction credit");
}
void mutation() {
    Fixture f; auto counts=f.counts;
    check(f.mutate()==1 && f.a[0]==200 && f.b[0]==100 && f.counts==counts && f.changes==1,"cross occupied swap");
    f.a[0]=f.b[0]; check(f.mutate()==1 && f.changes==2,"mutator does not add validation");
    f.changes=UINT16_MAX; check(f.mutate()==1 && f.changes==0,"uint16 wrap");
    f.lists[1].slots=f.a.data(); f.lists[1].cursor=16;
    const auto before=f.a; check(f.mutate()==1 && f.a[0]==before[1] && f.a[1]==before[0],"alias same-team");
    pass("shared_mutate_occupied","both occupied: exact two writes, alias-safe, no count/compact, identical-ID increment and halfword wrap");
    Fixture empty; empty.a.fill(UINT16_MAX); empty.b.fill(UINT16_MAX);
    empty.lists[0].cursor=14; empty.lists[1].cursor=29;
    const auto emptycounts=empty.counts;
    check(empty.mutate()==0 && empty.changes==0 && empty.counts==emptycounts,"backward searches stop at -1");
    pass("shared_mutate_empty","all-empty searches terminate before -1; both empty performs no writes/count/counter changes");
    Fixture receive; receive.lists[0].cursor=14; receive.lists[1].cursor=21;
    check(receive.mutate()==1 && receive.a[10]==206 && receive.a[14]==UINT16_MAX &&
        receive.b[6]==207 && receive.b[8]==209 && receive.b[9]==UINT16_MAX &&
        receive.counts[0]==11 && receive.counts[1]==9,"incoming first list normalized gap");
    pass("shared_mutate_receive","first empty cursor14 normalizes to10; second donor bench compacted AFTER counts; exact last empty");
    Fixture send; send.lists[0].cursor=6; send.lists[1].cursor=29;
    check(send.mutate()==1 && send.b[10]==106 && send.a[6]==107 && send.a[9]==UINT16_MAX &&
        send.counts[0]==9 && send.counts[1]==11,"outgoing first list");
    pass("shared_mutate_send","symmetric second-empty path, independent cursor/base translation, donor compaction");
    // A hole inside the list snaps only over its contiguous empty run.
    Fixture gap; gap.a[7]=gap.a[8]=UINT16_MAX; gap.lists[0].cursor=8; gap.lists[1].cursor=24;
    check(gap.mutate()==1 && gap.a[7]==209 && gap.a[8]==UINT16_MAX && gap.a[9]==109,"interior gap normalization");
    pass("shared_mutate_gap","backward search stops at nearest occupied predecessor, not global count or end-of-list");
    Fixture free; free.lists[0].team=29; free.lists[0].capacity=100; free.lists[0].count=100;
    for(int i=0;i<100;++i) free.a[i]=uint16_t(100+i);
    free.counts[29]=100; free.lists[0].cursor=98; free.lists[1].cursor=29;
    check(free.mutate()==1 && free.b[10]==198 && free.a[98]==199 && free.a[99]==UINT16_MAX && free.counts[29]==99,"free agent99 tail");
    // Last slot has zero shift iterations, but still must be cleared.
    free.lists[0].cursor=98; free.lists[1].cursor=29;
    check(free.mutate()==1 && free.a[98]==UINT16_MAX && free.b[11]==199,"free tail repeated transfer");
    pass("shared_mutate_free_agents","100-slot free-agent donor shifts through99 (not14); repeated membership transfers");
    Fixture guard; guard.lists[0].cursor=14;
    const auto ga=guard.a,gb=guard.b; const auto gc=guard.counts;
    check(nba97_roster_mutate(guard.lists,guard.counts.data(),&guard.changes,nullptr,nullptr)==-1 &&
        guard.a==ga && guard.b==gb && guard.counts==gc && guard.changes==0,"missing dependency atomic guard");
    guard.lists[0].cursor=200; check(guard.mutate()==-1,"bounds guard");
    pass("shared_mutate_guards","invalid cursors or missing transfer service rejected BEFORE writes; safety is separate from recovery credit");
}
void compaction() {
    Fixture f; f.lists[0].cursor=2; f.lists[1].cursor=29;
    // Prefer position0; row7 is the first matching healthy bench player.
    f.position[105]=4; f.position[106]=3; f.position[107]=0; f.position[108]=0;
    check(f.mutate()==1 && f.a[2]==107 && f.a[5]==105 && f.a[6]==106 &&
          f.a[7]==108 && f.a[8]==109 && f.a[9]==UINT16_MAX && f.b[10]==102,"starter preference order");
    pass("shared_compact_starter","555F4 delegates starter repair: ranked position then earliest bench match, fill starter then compact bench hole");
    Fixture injury; injury.lists[0].cursor=2; injury.lists[1].cursor=29;
    injury.compaction.mode=1; injury.compaction.injuries_enabled=1;
    injury.injury[105]=1; injury.position[106]=0;
    check(injury.mutate()==1 && injury.a[2]==106,"injured bench excluded");
    Fixture off; off.lists[0].cursor=2; off.lists[1].cursor=29; off.injury[105]=1;
    check(off.mutate()==1 && off.a[2]==105,"mode off includes injured bench");
    pass("shared_compact_injury","mode/injury-enabled gate replaces injured bench position with99; mode0 preserves position");
    Fixture boundary; boundary.lists[0].cursor=2; boundary.lists[1].cursor=29;
    boundary.position.fill(99); boundary.position[109]=0;
    // Post-decrement count9 means row9 is NOT searched, even though occupied.
    check(boundary.mutate()==1 && boundary.a[0]==101 && boundary.a[1]==UINT16_MAX && boundary.a[8]==109,
        "original post-decrement bound/no-match zero");
    std::array<uint16_t,100> slots{}; for(int i=0;i<100;++i) slots[i]=uint16_t(i);
    nba97_roster_compact(nullptr,slots.data(),29,99); check(slots[98]==98 && slots[99]==UINT16_MAX,"last free slot");
    nba97_roster_compact(nullptr,slots.data(),0,14); check(slots[13]==13 && slots[14]==UINT16_MAX,"last team slot");
    pass("shared_compact_boundaries","source quirk: post-decrement bench bound and no-match return0 preserved; tail14/99 zero-shift clears");
}
struct Event { Nba97RosterRefreshEvent type; int page,object; int32_t player; uint16_t observed_page; };
struct Capture { Nba97RosterRefresh* state; std::vector<Event> events; };
void sink(void* user,Nba97RosterRefreshEvent type,int page,int object,int32_t player) {
    auto& c=*static_cast<Capture*>(user);
    c.events.push_back({type,page,object,player,c.state->descriptor_page});
}
void refresh() {
    Fixture f;
    Nba97RosterRefresh s{}; s.lists[0]=f.lists[0]; s.lists[1]=f.lists[1];
    s.lists[0].cursor=7; s.lists[0].top=4; s.lists[1].cursor=27; s.lists[1].top=24;
    s.visible_rows=6; s.descriptor_page=42; s.selected[0]=-9; s.selected[1]=-8;
    Capture c{&s,{}}; s.sink=sink; s.user=&c;
    check(nba97_roster_refresh_lists(&s,2)==1,"both refresh");
    int binds=0,draws=0;
    for(const auto& e:c.events) {
        if(e.type==NBA97_ROSTER_BIND) { ++binds; check(e.observed_page==e.page,"temporary page during bind"); }
        if(e.type==NBA97_ROSTER_REDRAW) {
            ++draws; check(e.object>=s.lists[e.page].top && e.object<s.lists[e.page].top+6,"visible redraw bounds");
        }
    }
    check(binds==30 && draws==12 && s.selected[0]==107 && s.selected[1]==-1 && s.descriptor_page==42,"binding totals/sign/restore");
    check(c.events[c.events.size()-2].type==NBA97_ROSTER_PRESENT && c.events[c.events.size()-2].observed_page==1 &&
          c.events.back().type==NBA97_ROSTER_HEADER && c.events.back().observed_page==42,"present/restore/header ordering");
    pass("shared_refresh_both","30 binds/12 visible redraws, sign-extended selected IDs; present under page1 then restore42 then header");
    for(int page=0;page<2;++page) {
        c.events.clear(); s.selected[0]=900; s.selected[1]=901;
        check(nba97_roster_refresh_lists(&s,int16_t(page))==1,"single refresh");
        check(std::count_if(c.events.begin(),c.events.end(),[](const Event& e){return e.type==NBA97_ROSTER_BIND;})==15,"single count");
        check(s.selected[1-page]==900+1-page && s.descriptor_page==42,"other selected untouched");
        check(s.lists[0].cursor==7 && s.lists[0].top==4 && s.lists[1].cursor==27 && s.lists[1].top==24,"cursors/tops preserved");
    }
    pass("shared_refresh_single","selectors0 and1 change only requested list/selected global, preserve both cursors/tops and descriptor page");
    c.events.clear(); s.lists[0].count=0; s.lists[1].count=-1;
    check(nba97_roster_refresh_lists(&s,2)==1 && c.events.size()==2 && s.selected[0]==107 && s.selected[1]==-1,"nonpositive row counts");
    pass("shared_refresh_empty_count","zero/negative display count skips bind loop but selected-ID, present and header contracts still execute");
    s.lists[0].count=15; s.lists[1]={f.b.data(),100,29,100,1,15,114,109};
    f.b[99]=0x8000; c.events.clear();
    check(nba97_roster_refresh_lists(&s,1)==1 && s.selected[1]==-32768,"signed 8000 and100rows");
    check(std::count_if(c.events.begin(),c.events.end(),[](const Event& e){return e.type==NBA97_ROSTER_BIND;})==100,"100 binds");
    check(std::count_if(c.events.begin(),c.events.end(),[](const Event& e){return e.type==NBA97_ROSTER_REDRAW;})==6,"100 lastpage visibility");
    pass("shared_refresh_free_agents","100-row descriptor, absolute cursor114/base15, last six redraws and signed halfword promotion");
    c.events.clear(); check(!nba97_roster_refresh_lists(&s,3) && c.events.empty(),"bad selector");
    s.lists[1].count=101; check(!nba97_roster_refresh_lists(&s,2) && c.events.empty(),"atomic descriptor guards");
    pass("shared_refresh_guards","invalid selector/count rejected before first-page effects; no out-of-bounds writes");
    std::array<uint16_t,15> slots{}; for(int i=0;i<15;++i) slots[i]=uint16_t(100+i);
    Nba97ReorderSession native{}; nba97_reorder_begin(&native,slots.data());
    check(native.visible_redraws==12 && native.presentation_requests==1,"native refresh integration");
    native.top[0]=4; native.cursor[0]=9; native.top[1]=9; native.cursor[1]=14; native.descriptor_page=1;
    nba97_reorder_refresh(&native);
    check(native.visible_redraws==24 && native.presentation_requests==2 && native.descriptor_page==1 &&
        native.selected_ids[0]==109 && native.selected_ids[1]==114,"adapter consumption");
    pass("shared_refresh_native_adapter","real Re-order session consumes shared bind/redraw/present/header contracts; diagnostic counters exposed to CLI");
}
}
void rosterListsTests() { validation(); mutation(); compaction(); refresh(); }

void rosterListsLocalTests(const nba97::RosterDatabase& db, const std::string& databasePath) {
    // Read ONLY the 25-byte rules table from the user's private overlay. No
    // original bytes or roster data are copied into committed test fixtures.
    const auto overlay=std::filesystem::path(databasePath).parent_path().parent_path().parent_path()/"extracted/FEONLY.BIN";
    std::ifstream file(overlay,std::ios::binary);
    check(bool(file),"local starter rules require private FEONLY.BIN");
    std::array<uint8_t,25> preference{};
    file.seekg(0x800265ac-0x80015000);
    file.read(reinterpret_cast<char*>(preference.data()),preference.size());
    check(file.gcount()==25,"truncated starter preference table");
    for(int row=0;row<5;++row) {
        std::array<bool,5> seen{};
        for(int rank=0;rank<5;++rank) {
            const auto p=preference[row*5+rank];
            check(p<5 && !seen[p],"private preference row must rank all five positions"); seen[p]=true;
        }
        check(preference[row*5]==row,"private preferred position identity");
    }
    std::vector<uint8_t> positions(65536,99), injuries(65536,0);
    for(const auto& p:db.players()) positions[p.id]=p.position;
    unsigned cases=0;
    for(const auto& team:db.teams()) for(int starter=0;starter<5;++starter) {
        auto slots=team.roster, expected=team.roster;
        std::array<uint16_t,100> free{}; free.fill(UINT16_MAX);
        std::array<uint16_t,30> counts{};
        const auto count=std::count_if(slots.begin(),slots.end(),[](uint16_t id){return id!=UINT16_MAX;});
        check(count>8 && slots[starter]!=UINT16_MAX,"local starter transfer precondition");
        counts[team.id]=uint16_t(count);
        int chosen=-1;
        // Independent ranking oracle over the original POST-decrement bound.
        for(int rank=0;rank<5 && chosen<0;++rank)
            for(int bench=5;bench<count-1;++bench)
                if(positions[slots[bench]]==preference[starter*5+rank]) { chosen=bench; break; }
        check(chosen>=5,"real roster has a valid starter replacement");
        const auto outgoing=slots[starter];
        expected[starter]=expected[chosen];
        for(int row=chosen;row<14;++row) expected[row]=expected[row+1];
        expected[14]=UINT16_MAX;
        Nba97RosterList lists[2]={{slots.data(),15,int16_t(team.id),15,1,0,uint8_t(starter),0},
                                 {free.data(),100,29,100,1,15,114,109}};
        Nba97RosterValidation rules{0,12,0,counts.data(),injuries.data(),injuries.size()};
        Nba97RosterCompaction data{counts.data(),positions.data(),injuries.data(),preference.data(),positions.size(),0,0};
        uint16_t changes=0;
        check(nba97_roster_validate(lists,&rules).result==1,"local validation");
        check(nba97_roster_mutate(lists,counts.data(),&changes,nba97_roster_compact,&data)==1,"local transfer");
        check(slots==expected && free[0]==outgoing && free[99]==UINT16_MAX &&
              counts[team.id]==count-1 && counts[29]==1 && changes==1,"local transfer exact result");
        check(db.team(team.id)->roster==team.roster,"database not published or saved"); ++cases;
    }
    check(cases==145,"29 teams x five starters");
    pass("shared_local_compaction","145 real-roster starter transfers with private800265AC position rankings; exact slots/counts, isolated copies, no source assets or saves changed");
}
