#include "recovered/team_ratings.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)

int main() {
    Nba97TeamRatingInput teams[29]{};int16_t adjustment[29]{};
    uint16_t scores[5][29]{};Nba97TeamRanks ranks{};
    for(auto& t:teams) {std::memset(t.ratings,50,sizeof(t.ratings));t.count=8;}
    CHECK(nba97_team_ratings(teams,adjustment,scores,&ranks));
    for(unsigned c=0;c<5;++c) for(unsigned t=0;t<29;++t) {
        CHECK(scores[c][t]==(c==4 ? 3065:0));CHECK(ranks.value[c][t]==t+1);
    }
    for(unsigned c=0;c<5;++c) CHECK(ranks.value[c][29]==30 && ranks.value[c][30]==31);
    // Independent hand-derived values distinguish signed / from arithmetic >>3.
    std::fill_n(adjustment,29,int16_t(1000));
    teams[0].ratings[0][0]=51;
    CHECK(nba97_team_ratings(teams,adjustment,scores,&ranks));
    CHECK(scores[0][0]==969 && scores[0][1]==941);
    CHECK(ranks.value[0][0]==1 && ranks.value[0][1]==2);
    for(auto& t:teams) {std::memset(t.ratings,50,sizeof(t.ratings));t.count=9;}
    std::fill_n(adjustment,29,int16_t(0));
    CHECK(nba97_team_ratings(teams,adjustment,scores,&ranks));CHECK(scores[4][0]==3125);
    for(auto& t:teams) {std::memset(t.ratings,255,sizeof(t.ratings));t.count=15;}
    std::fill_n(adjustment,29,int16_t(32767));
    CHECK(nba97_team_ratings(teams,adjustment,scores,&ranks));
    for(auto& cat:scores) for(auto score:cat) CHECK(score==65535);
    for(auto& t:teams) std::memset(t.ratings,0,sizeof(t.ratings));
    CHECK(nba97_team_ratings(teams,adjustment,scores,&ranks));
    for(auto& cat:scores) for(auto score:cat) CHECK(score==0);
    const auto saved_ranks=ranks;uint16_t saved_scores[5][29];std::memcpy(saved_scores,scores,sizeof(scores));
    // Native adapter rejects outside roster size8..15, without partial output.
    for(unsigned bad: {0u,7u,16u,255u}) {
        teams[28].count=static_cast<uint8_t>(bad);
        CHECK(!nba97_team_ratings(teams,adjustment,scores,&ranks));
        CHECK(!std::memcmp(saved_scores,scores,sizeof(scores)) && !std::memcmp(&saved_ranks,&ranks,sizeof(ranks)));
    }
    CHECK(!nba97_team_ratings(nullptr,adjustment,scores,&ranks));
    std::puts("TEAM RATINGS PASS: weighted roster tiers, eight-player quirk, signed shift, stable ties, special ranks, saturation, invalid adapter input");
}
