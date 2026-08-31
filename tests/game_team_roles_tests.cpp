#include "recovered/game_team_roles.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using Players=std::array<Nba97GameRolePlayer,24>;
static Nba97GameTeamRolesInput fixture(const Players& p) {
    Nba97GameTeamRolesInput in{};in.players=p.data();in.player_count=p.size();
    in.incoming_t6=0x8001f984;in.incoming_t6_known=1;
    for(unsigned i=0;i<10;++i) {
        in.active_player_reference[i]=static_cast<uint16_t>(i);
        in.entity_table[i]=static_cast<uint8_t>(i);
        in.entity[i]={i,static_cast<uint16_t>((i+5)%10),static_cast<uint16_t>(i),static_cast<uint8_t>(i)};
    }
    return in;
}
static Nba97GameTeamRolesEffects run(const Nba97GameTeamRolesInput& in) {
    struct Guard { uint64_t a;Nba97GameTeamRolesEffects out;uint64_t b; } g{};
    g.a=0x123456789abcdef0;g.b=0xfedcba9876543210;const auto before=in;
    CHECK(nba97_game_team_roles(&g.out,&in)==NBA97_TEAM_ROLES_OK);
    CHECK(g.a==0x123456789abcdef0 && g.b==0xfedcba9876543210);
    CHECK(std::memcmp(&before,&in,sizeof(in))==0);return g.out;
}
static void refuse(const Nba97GameTeamRolesInput& in,Nba97GameTeamRolesResult why) {
    Nba97GameTeamRolesEffects out;std::memset(&out,0xa5,sizeof(out));const auto before=out;
    CHECK(nba97_game_team_roles(&out,&in)==why);CHECK(std::memcmp(&before,&out,sizeof(out))==0);
}
static void carried_registers() {
    Players p{};auto in=fixture(p);auto out=run(in);
    for(unsigned s=0;s<2;++s) {
        CHECK(out.team[s].field61==0x84 && out.after6459c_t6[s]==0x8001f984);
        CHECK(out.team[s].fielda6==0 && out.team[s].fielda8==0xffff);
        CHECK(out.after644fc_t0[s]==0xffffffff && out.after644fc_t1[s]==0);
        for(unsigned i=0;i<5;++i) {
            CHECK(out.team[s].order5c[i]==(s?i:i+5));CHECK(out.team[s].orderbb[i]==i);
        }
    }
    for(unsigned i=0;i<10;++i) {
        CHECK(out.entity[i].written==(NBA97_ROLE_D4|NBA97_ROLE_CB));
        CHECK(out.entity[i].fieldd4==(i+5)%10 && out.entity[i].fieldcb==5-i%5);
    }
    p[2].byte17=7;p[3].byte17=7;in.entity[2].word00=0xabcdef42;
    out=run(in);CHECK(out.after6459c_t6[0]==0xabcdef42 && out.after6459c_t6[1]==0xabcdef42);
    CHECK(out.team[0].field61==0x42 && out.team[1].field61==0x42);
    for(unsigned i=0;i<5;++i)p[i].byte0e=77;
    out=run(in);CHECK(out.after6459c_t1[1]==77);
    CHECK(out.team[0].fielda6==77 && out.team[1].fielda6==77); // Retained score, not player index.
    p[2].byte0f=1;in.entity[2].word00=0x1234abcd;
    out=run(in);CHECK(out.after644fc_t1[0]==0x1234abcd && out.after644fc_t1[1]==0x1234abcd);
    CHECK(out.team[0].fielda6==0xabcd && out.team[1].fielda6==0xabcd);
    in.incoming_t6=0xfedc9876;p.fill({});out=run(in);
    CHECK(out.after6459c_t6[1]==0xfedc9876 && out.team[1].field61==0x76);
}
static void secondary_thresholds() {
    for(unsigned first=0;first<256;++first) for(unsigned second: {0u,69u,70u,71u,79u,80u,255u}) {
        Players p{};auto in=fixture(p);p[0].byte0f=static_cast<uint8_t>(first);
        p[1].byte0f=static_cast<uint8_t>(second);auto out=run(in);
        const unsigned best=first>second?first:second,runner=first>second?second:first;
        CHECK(out.team[0].fielda8==((best-runner>9 || runner<70)?0xffff:(first>=second?1:0)));
        if(best) CHECK(out.team[0].fielda6==(first>=second?0:1));
    }
}
static void physical_walk_and_separate_pools() {
    Players p{};auto in=fixture(p);
    in.entity_table[1]=4;in.entity_table[4]=1;
    p[1].byte0f=80;p[4].byte0f=80;
    auto out=run(in);CHECK(out.team[0].fielda6==1 && out.team[0].fielda8==4);
    // 6459C independently uses active-player references and opponents' bound statuses.
    p.fill({});p[23].byte0e=99;in.active_player_reference[7]=23;in.status_byte1e[8]=2;
    out=run(in);CHECK(out.team[0].order5c[0]==8);
    CHECK(out.team[0].orderbb[0]==2); // After removing bonus, player7's99 wins.
    // Explicit duplicate opponents are retained; the last rank write wins.
    in=fixture(p);for(auto& entity:in.entity)entity.opponent_d6=9;
    out=run(in);CHECK(out.entity[9].fieldcb==1);
    for(unsigned i=0;i<9;++i)CHECK(out.entity[i].written==NBA97_ROLE_D4);
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<5;++i)CHECK(out.team[s].order5c[i]==9);
}
static void preserved_sort_quirks() {
    Players p{};auto in=fixture(p);
    for(unsigned i=0;i<5;++i)p[i+5].byte0e=static_cast<uint8_t>(i+1);
    auto out=run(in);
    // Independent original-instruction fixture: first phase is deliberately
    // incomplete for ascending1..5. Do not replace64388 with std::sort.
    const uint8_t first[5]={8,7,9,6,5},second[5]={4,3,2,1,0};
    CHECK(std::memcmp(out.team[0].order5c,first,5)==0);
    CHECK(std::memcmp(out.team[0].orderbb,second,5)==0);
    CHECK(out.after6459c_t1[0]==2); // Last first-phase left score, not the final sorted minimum.
    const uint8_t ratings[5]={100,99,101,98,255};
    for(unsigned i=0;i<5;++i)p[i+5].byte0e=ratings[i];
    out=run(in);
    // No status bonus is present, but raw100/101/255 still lose100.
    const uint8_t raw_first[5]={9,7,5,6,8},raw_second[5]={4,1,3,2,0};
    CHECK(std::memcmp(out.team[0].order5c,raw_first,5)==0);
    CHECK(std::memcmp(out.team[0].orderbb,raw_second,5)==0);
    CHECK(out.after6459c_t1[0]==99);
}
static void guards_and_overlap() {
    Players p{};auto good=fixture(p),in=good;
    in.incoming_t6_known=0;refuse(in,NBA97_TEAM_ROLES_UNKNOWN_REGISTER);
    in=good;in.players=nullptr;refuse(in,NBA97_TEAM_ROLES_ARGUMENT);
    in=good;in.entity_table[3]=10;refuse(in,NBA97_TEAM_ROLES_ENTITY_REFERENCE);
    in=good;in.entity[3].opponent_d6=0xffff;refuse(in,NBA97_TEAM_ROLES_OPPONENT_INDEX);
    in=good;in.active_player_reference[7]=24;refuse(in,NBA97_TEAM_ROLES_PLAYER_REFERENCE);
    in=good;in.entity[3].player_reference=24;refuse(in,NBA97_TEAM_ROLES_PLAYER_REFERENCE);
    in=good;in.entity[7].status_reference=24;refuse(in,NBA97_TEAM_ROLES_STATUS_REFERENCE);
    in=good;in.entity_table[5]=6;refuse(in,NBA97_TEAM_ROLES_PHYSICAL_SPAN);
    CHECK(nba97_game_team_roles(nullptr,&good)==NBA97_TEAM_ROLES_ARGUMENT);
    Nba97GameTeamRolesEffects out{};CHECK(nba97_game_team_roles(&out,nullptr)==NBA97_TEAM_ROLES_ARGUMENT);
    const auto expected=run(good);
    union Overlap { Nba97GameTeamRolesInput in;Nba97GameTeamRolesEffects out; } overlap{};
    overlap.in=good;CHECK(nba97_game_team_roles(&overlap.out,&overlap.in)==NBA97_TEAM_ROLES_OK);
    CHECK(std::memcmp(&expected,&overlap.out,sizeof(expected))==0);
    // Player bytes may share output storage: no publication occurs until all
    // source reads have finished, including the final physical span.
    union PlayerOverlap { Players players;Nba97GameTeamRolesEffects out; } shared{};
    shared.players=p;in=fixture(shared.players);
    CHECK(nba97_game_team_roles(&shared.out,&in)==NBA97_TEAM_ROLES_OK);
    CHECK(std::memcmp(&expected,&shared.out,sizeof(expected))==0);
}
int main() {
    carried_registers();secondary_thresholds();physical_walk_and_separate_pools();preserved_sort_quirks();guards_and_overlap();
    std::printf("game_team_roles: %u checks passed\n",checks);
}
