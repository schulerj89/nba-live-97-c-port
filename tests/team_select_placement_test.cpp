#include "recovered/team_select_placement.h"
#include "recovered/team_select_poll.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

static void at(const Nba97TeamPlacementNode& n,int x,int y) {
    CHECK(n.alive && n.x==x && n.y==y);
}
static void steady(const Nba97TeamSelectPlacement& s,unsigned side) {
    at(s.value[0],388,86);at(s.value[6],112,86);
    CHECK(!s.label[0].alive && !s.label[6].alive);
    const int y[5]={122,138,154,170,186};
    for(unsigned i=0;i<5;++i) {
        at(s.value[i+1],388,y[i]);at(s.value[i+7],112,y[i]);
        at(s.label[i+1],248,y[i]+(side ? 200:0));
        at(s.label[i+7],248,y[i]+(side ? 0:200));
    }
    const int home_arrows[4]={320,460,-458,-318};
    const int away_arrows[4]={820,960,42,182};
    for(unsigned i=0;i<4;++i) at(s.arrow[i],side ? away_arrows[i]:home_arrows[i],96);
}

int main() {
    // Explicit presentation anchors, not a generic menu layout or asset fixture.
    for(unsigned side=0;side<2;++side) {
        Nba97TeamSelectPlacement s{};CHECK(nba97_team_select_placement_open(&s,side));
        CHECK(s.arrow_group==120+side && s.graphic_count==2);
        at(s.value[0],388,86);at(s.value[6],112,86);
        const int initial_away_y[5]={218,234,250,266,282};
        for(unsigned i=0;i<5;++i) {
            at(s.value[i+7],388,initial_away_y[i]);
            CHECK(s.value[i+7].relative && s.value[i+7].elapsed==0 && s.value[i+7].duration==1);
            CHECK(s.value[i+7].dx==-276 && s.value[i+7].dy==-96);
        }
        for(unsigned id=0;id<12;++id)
            CHECK(nba97_team_select_placement_selected_moving(&s,id)==(id>=7 ? 8:0));
        const auto before=s;
        nba97_team_select_placement_tick(&s);steady(s,side);
        for(unsigned id=7;id<12;++id) {
            CHECK(s.value[id].offset_x==-276 && s.value[id].offset_y==-96);
            CHECK(s.value[id].x-s.value[id].offset_x==before.value[id].x);
            CHECK(s.value[id].y-s.value[id].offset_y==before.value[id].y);
            CHECK(s.value[id].relative && s.value[id].elapsed==1);
            CHECK(!nba97_team_select_placement_selected_moving(&s,id));
        }
        // The second page cleanup does not add another delta or block polling.
        nba97_team_select_placement_tick(&s);steady(s,side);
        for(const auto& n:s.value) CHECK(!n.relative);
        for(const auto& n:s.label) CHECK(!n.relative);
        for(const auto& n:s.arrow) CHECK(!n.relative);
        const auto stopped=s;
        for(unsigned i=0;i<9;++i) nba97_team_select_placement_tick(&s);
        CHECK(!std::memcmp(&s,&stopped,sizeof(s)));

        // A Cross callback queues motion; the completed frame remains unchanged
        // until the next requested presentation. All four arrows survive.
        unsigned current=side;
        for(unsigned turn=0;turn<24;++turn) {
            const auto old=s;CHECK(nba97_team_select_placement_switch_side(&s,current));
            steady(s,current);CHECK(!std::memcmp(s.value,old.value,sizeof(s.value)));
            for(unsigned id=0;id<12;++id) CHECK(!nba97_team_select_placement_selected_moving(&s,id));
            for(unsigned i=0;i<4;++i) CHECK(s.arrow[i].relative && s.arrow[i].elapsed==0);
            current^=1;nba97_team_select_placement_tick(&s);steady(s,current);
            CHECK(s.arrow_group==120+side); // Group is entry-owned, not active-page-owned.
            nba97_team_select_placement_tick(&s);steady(s,current);
        }
        // Reopening discards the prior screen's completed placement, and starts
        // the new screen with its own pending constructor commands.
        CHECK(nba97_team_select_placement_open(&s,side^1));
        CHECK(s.arrow_group==120+(side^1) && s.value[7].x==388 && s.value[7].offset_x==0);
    }

    // 2B1E4 replaces pending deltas, not a queue of both requests.
    Nba97TeamSelectPlacement s{};CHECK(nba97_team_select_placement_open(&s,0));
    nba97_team_select_placement_tick(&s);nba97_team_select_placement_tick(&s);
    CHECK(nba97_team_select_placement_switch_side(&s,0));
    CHECK(nba97_team_select_placement_switch_side(&s,0));
    nba97_team_select_placement_tick(&s);steady(s,1);
    CHECK(s.arrow[0].offset_x==500 && s.label[1].offset_y==200);
    // Replace an unapplied command with its opposite: only the last delta runs.
    CHECK(nba97_team_select_placement_switch_side(&s,0));
    CHECK(nba97_team_select_placement_switch_side(&s,1));
    nba97_team_select_placement_tick(&s);steady(s,0);

    // Active-side redraws recreate only value nodes. Existing position offsets
    // must not be copied onto newly created text at already-adjusted coordinates.
    for(unsigned entry=0;entry<2;++entry) for(unsigned side=0;side<2;++side)
        for(unsigned ticks=0;ticks<3;++ticks) {
            CHECK(nba97_team_select_placement_open(&s,entry));
            for(unsigned i=0;i<ticks;++i) nba97_team_select_placement_tick(&s);
            const auto old=s;CHECK(nba97_team_select_placement_refresh_values(&s,side));
            CHECK(!std::memcmp(s.label,old.label,sizeof(s.label)));
            CHECK(!std::memcmp(s.arrow,old.arrow,sizeof(s.arrow)));
            CHECK(s.arrow_group==old.arrow_group && s.graphic_count==old.graphic_count);
            CHECK(!std::memcmp(s.value+(side^1)*6,old.value+(side^1)*6,6*sizeof(s.value[0])));
            for(unsigned i=0;i<6;++i) {
                const auto& n=s.value[side*6+i];
                at(n,side ? 112:388,i ? 106+int(i)*16:86);
                CHECK(!n.relative && !n.offset_x && !n.offset_y && !n.elapsed && !n.duration);
            }
            nba97_team_select_placement_tick(&s);steady(s,entry);
        }

    // A moving nonhead label or arrow does not make a stationary value head move.
    CHECK(nba97_team_select_placement_open(&s,1));
    CHECK(s.label[1].relative && s.arrow[0].relative);
    CHECK(!nba97_team_select_placement_selected_moving(&s,1));
    s.value[7].alive=0;CHECK(!nba97_team_select_placement_selected_moving(&s,7));

    // Actual state3 has two type41 graphics, so3AE4C bypasses the head query.
    // Pending away rank text therefore completes on the FIRST mandatory poll
    // presentation. The generic no-graphics gate would instead pump once first.
    for(unsigned entry=0;entry<2;++entry) for(unsigned focus=0;focus<12;++focus) {
        CHECK(nba97_team_select_placement_open(&s,entry));
        Nba97TeamPoll poll{};nba97_team_poll_open(&poll);
        const int moving=nba97_team_select_placement_selected_moving(&s,focus);
        CHECK(nba97_team_poll_prepare(&poll,s.graphic_count ? 0:moving));
        nba97_team_select_placement_tick(&s);steady(s,entry);
        const uint16_t pads[8]={4};Nba97TeamSample sample{};
        CHECK(nba97_team_poll_presented(&poll,pads,&sample)==NBA97_TEAM_POLL_INPUT);
        CHECK(sample.token==4);
    }
    CHECK(nba97_team_select_placement_open(&s,1));
    Nba97TeamPoll generic{};nba97_team_poll_open(&generic);
    CHECK(nba97_team_poll_prepare(&generic,nba97_team_select_placement_selected_moving(&s,7)));
    nba97_team_select_placement_tick(&s);
    const uint16_t pads[8]={4};Nba97TeamSample sample{};
    CHECK(nba97_team_poll_presented(&generic,pads,&sample)==NBA97_TEAM_POLL_NONE);
    CHECK(nba97_team_poll_prepare(&generic,nba97_team_select_placement_selected_moving(&s,7)));
    nba97_team_select_placement_tick(&s);
    CHECK(nba97_team_poll_presented(&generic,pads,&sample)==NBA97_TEAM_POLL_INPUT);

    const auto unchanged=s;
    for(unsigned invalid:{2u,12u,std::numeric_limits<unsigned>::max()}) {
        CHECK(!nba97_team_select_placement_open(&s,invalid));
        CHECK(!nba97_team_select_placement_switch_side(&s,invalid));
        CHECK(!nba97_team_select_placement_refresh_values(&s,invalid));
        CHECK(!std::memcmp(&s,&unchanged,sizeof(s)));
    }
    CHECK(!nba97_team_select_placement_selected_moving(&s,12));
    CHECK(!nba97_team_select_placement_open(nullptr,0));
    CHECK(!nba97_team_select_placement_switch_side(nullptr,0));
    CHECK(!nba97_team_select_placement_refresh_values(nullptr,0));
    CHECK(!nba97_team_select_placement_selected_moving(nullptr,0));
    nba97_team_select_placement_tick(nullptr);
    std::puts("TEAM SELECT PLACEMENT PASS: both entry pages, 48 side switches, 12 value refresh cases, replacement/cleanup, head-only query, 24 graphics-bypassed first polls, guards");
}
