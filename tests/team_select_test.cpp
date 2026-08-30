#include "recovered/team_select.h"
#include "recovered/game_setup.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

int main() {
    for(unsigned card=0;card<4;++card) {
        uint8_t choices[4]={};const unsigned count=card==0 ? 4:3;
        for(unsigned i=0;i<count;++i) {CHECK(choices[card]==i);CHECK(nba97_setup_step(choices,card,1));}
        CHECK(choices[card]==0);CHECK(nba97_setup_step(choices,card,-1));CHECK(choices[card]==count-1);
        for(unsigned other=0;other<4;++other) if(other!=card) CHECK(choices[other]==0);
        CHECK(!nba97_setup_step(choices,4,1));
    }
    // Synthetic vectors exercise updated-neighbor accumulation and counter carry.
    uint32_t zero[6]={};CHECK(nba97_team_select_rng_step(zero)==0);CHECK(zero[5]==1);
    uint32_t small[6]={1,2,3,4,5,6};const uint32_t small_expected[6]={21,20,18,15,11,7};
    CHECK(nba97_team_select_rng_step(small)==21);CHECK(!std::memcmp(small,small_expected,sizeof(small)));
    uint32_t carry[6]={UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
    const uint32_t carry_expected[6]={UINT32_MAX-1,UINT32_MAX-1,UINT32_MAX-1,UINT32_MAX-1,UINT32_MAX,0};
    CHECK(nba97_team_select_rng_step(carry)==UINT32_MAX-1);CHECK(!std::memcmp(carry,carry_expected,sizeof(carry)));
    Nba97TeamRanks ranks{};
    /* Synthetic, independent category orders; not extracted retail data. */
    const int steps[5]={1,3,7,11,17};
    for(int c=0;c<5;++c) for(int r=0;r<31;++r)
        ranks.value[c][(r*steps[c]+4)%31]=static_cast<uint8_t>(r+1);
    CHECK(nba97_team_ranks_valid(&ranks));
    auto bad=ranks; bad.value[2][0]=bad.value[2][1];
    CHECK(!nba97_team_ranks_valid(&bad));
    bad=ranks; bad.value[0][0]=0; CHECK(!nba97_team_ranks_valid(&bad));
    bad=ranks; bad.value[0][0]=32; CHECK(!nba97_team_ranks_valid(&bad));
    CHECK(!nba97_team_ranks_valid(nullptr));

    Nba97TeamSelect s{};
    CHECK(nba97_team_select_open(&s,3,24,3,24));
    CHECK(s.side==0 && s.criterion==0 && !s.result);
    for(unsigned focus=0;focus<12;++focus) {
        CHECK(nba97_team_select_restore_focus(&s,focus));CHECK(s.side==focus/6 && s.criterion==focus%6);
    }
    auto focus_before=s;CHECK(!nba97_team_select_restore_focus(&s,12));CHECK(!std::memcmp(&s,&focus_before,sizeof(s)));
    CHECK(nba97_team_select_restore_focus(&s,0));
    auto unchanged=s;
    CHECK(!nba97_team_select_open(&s,31,24,3,24));
    CHECK(std::memcmp(&s,&unchanged,sizeof(s))==0);
    CHECK(nba97_team_select_input(&s,&ranks,1)==NBA97_SELECT_CRITERION);
    CHECK(s.criterion==5 && s.sound==3);
    CHECK(nba97_team_select_input(&s,&ranks,2)==NBA97_SELECT_CRITERION);
    CHECK(s.criterion==0 && s.sound==4);
    CHECK(nba97_team_select_input(&s,&ranks,8)==NBA97_SELECT_TEAM);
    CHECK(s.team[0]==2 && s.team[1]==24 && s.sound==2);
    CHECK(nba97_team_select_input(&s,&ranks,4)==NBA97_SELECT_TEAM);
    CHECK(s.team[0]==3 && s.sound==1);

    /* Each side/category visits every team exactly once in both directions,
       including both endpoint wraps, and never changes the opposite team. */
    for(int side=0;side<2;++side) for(int c=0;c<6;++c) for(int dir : {4,8}) {
        CHECK(nba97_team_select_open(&s,4,4,3,24));
        if(side) CHECK(nba97_team_select_input(&s,&ranks,0x800)==NBA97_SELECT_SIDE);
        for(int i=0;i<c;++i) nba97_team_select_input(&s,&ranks,2);
        std::array<bool,31> visited{};
        for(int i=0;i<31;++i) {
            CHECK(!visited[s.team[side]]); visited[s.team[side]]=true;
            const int before=s.team[side];
            CHECK(nba97_team_select_input(&s,&ranks,static_cast<uint16_t>(dir))==NBA97_SELECT_TEAM);
            CHECK(s.team[side^1]==4 && s.criterion==c);
            if(c) {
                const int old_rank=ranks.value[c-1][before], new_rank=ranks.value[c-1][s.team[side]];
                CHECK(dir==4 ? (new_rank==old_rank+1 || (old_rank==31 && new_rank==1)) :
                               (new_rank==old_rank-1 || (old_rank==1 && new_rank==31)));
            }
        }
        CHECK(s.team[side]==4);
    }
    /* Side changes preserve criterion; same-team pairs and special pairs
       are not refused by the retail Team Select constructor/callbacks. */
    CHECK(nba97_team_select_open(&s,30,30,3,24));
    for(int i=0;i<4;++i) nba97_team_select_input(&s,&ranks,2);
    nba97_team_select_input(&s,&ranks,0x800);
    CHECK(s.side==1 && s.criterion==4 && s.team[0]==30 && s.team[1]==30);
    CHECK(nba97_team_select_input(&s,&ranks,0x100)==NBA97_SELECT_RETURN);
    CHECK(s.result==-1 && s.team[0]==30 && s.remembered_regular[0]==3 && s.remembered_regular[1]==24);
    unchanged=s; CHECK(nba97_team_select_input(&s,&ranks,4)==NBA97_SELECT_NONE);
    CHECK(std::memcmp(&s,&unchanged,sizeof(s))==0);
    for(uint16_t exit_token : {0x80,0x100}) {
        CHECK(nba97_team_select_open(&s,10,10,3,24));
        nba97_team_select_input(&s,&ranks,exit_token);
        CHECK(s.remembered_regular[0]==10 && s.remembered_regular[1]==10);
    }
    for(unsigned word=0;word<65536;++word) {
        CHECK(nba97_team_select_open(&s,3,24,3,24));
        const bool accepted=nba97_team_select_random_candidate(&s,static_cast<uint16_t>(word))!=0;
        CHECK(accepted==((word&31)<29));
        CHECK(s.team[0]==(accepted ? int(word&31):3) && s.team[1]==24);
    }
    CHECK(nba97_team_select_open(&s,3,24,3,24));
    Nba97TeamRandom random{};uint32_t rng[6]={1,2,3,4,5,6};
    CHECK(nba97_team_random_begin(&random,&s,rng));CHECK(random.remaining==11 && random.wait==1);
    unsigned changes=1;
    for(unsigned tick=1;tick<=78;++tick) {
        CHECK(nba97_team_random_busy(&random));
        changes+=nba97_team_random_tick(&random,&s,rng);
        if(tick==66) CHECK(changes==12 && random.wait==12 && !random.remaining);
        CHECK(nba97_team_random_busy(&random)==(tick<78));
    }
    CHECK(changes==12 && s.team[0]<29 && s.team[1]==24);
    /* Callback input domain, not a claim of simultaneous-pad precedence. */
    for(unsigned token=0;token<65536;++token) {
        if(token==1 || token==2 || token==4 || token==8 || token==0x20 || token==0x40 ||
           token==0x80 || token==0x100 || token==0x800) continue;
        CHECK(nba97_team_select_open(&s,3,24,3,24)); unchanged=s;
        CHECK(nba97_team_select_input(&s,&ranks,static_cast<uint16_t>(token))==NBA97_SELECT_NONE);
        CHECK(std::memcmp(&s,&unchanged,sizeof(s))==0);
    }
    std::puts("TEAM SELECT CORE PASS: 24 complete navigation cycles, criterion/side wrapping, exit persistence, special/same teams, random candidate domain, callback token isolation");
}
