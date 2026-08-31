#include "match_strategy.h"
#include <string.h>

int nba97_match_strategy_cold(Nba97MatchStrategy* out,uint32_t initialized_word) {
    static const Nba97MatchStrategy cold={{{1,1,0,7,5,0,0},{1,1,0,7,5,0,0}}};
    if(!out) return 0;
    if(!initialized_word) memcpy(out,&cold,sizeof(cold));
    return 1;
}

int nba97_match_strategy_apply(Nba97MatchTeamStrategy* inout,
    const Nba97MatchStrategy* resident,uint16_t side_word,uint16_t launch,
    uint16_t human_count,uint8_t injury_slot) {
    Nba97MatchTeamStrategy next;
    Nba97MatchStrategy source;
    if(!inout || !resident || injury_slot<12) return 0;
    memcpy(&next,inout,sizeof(next));
    memcpy(&source,resident,sizeof(source));
    if(launch) next.fields[0]=1;
    else if(!human_count) {next.fields[0]=1;next.fields[1]=1;}
    else memcpy(next.fields,source.side[side_word!=0],sizeof(next.fields));
    memcpy(inout,&next,sizeof(next));
    return 1;
}

int nba97_match_strategy_writeback(Nba97MatchStrategy* resident,
    const Nba97MatchTeamStrategy team[2],uint16_t launch) {
    Nba97MatchTeamStrategy source[2];
    Nba97MatchStrategy next;
    if(!resident || !team) return 0;
    if(launch) return 1;
    memcpy(source,team,sizeof(source));
    memcpy(next.side[0],source[0].fields,sizeof(next.side[0]));
    memcpy(next.side[1],source[1].fields,sizeof(next.side[1]));
    memcpy(resident,&next,sizeof(next));
    return 1;
}
