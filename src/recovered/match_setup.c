#include "match_setup.h"
#include <string.h>

int nba97_match_roster_indices(Nba97MatchRosterIndices* out,unsigned count) {
    unsigned i;Nba97MatchRosterIndices next;
    if(!out || count>15) return 0;
    memset(&next,0,sizeof(next));
    next.count=(uint8_t)count;next.active_count=(uint8_t)(count<12 ? count:12);
    for(i=0;i<12;++i) {
        next.alias[i]=(uint8_t)(i<count ? i:0);
        next.initial_lineup[i]=(uint16_t)i;
    }
    *out=next;return 1;
}
int nba97_match_effective_rules(uint8_t out[14],unsigned style,const uint8_t custom[14]) {
    uint8_t next[14];
    if(!out || style>2 || (style==2 && !custom)) return 0;
    if(style==2) memcpy(next,custom,14);
    else if(style==0) {memset(next,0,14);next[11]=1;}
    else {memset(next,1,14);next[0]=4;next[1]=4;next[2]=5;}
    memcpy(out,next,14);return 1;
}
uint32_t nba97_match_option_address(unsigned index) {
    static const uint32_t addresses[11]={0x80021d86,0x80021d7c,0x80021d7d,0x80021d7e,
        0x80021d7f,0x80021d95,0x80021d81,0x80021d82,0x80021d83,0x80021d84,0x80021d99};
    return index<11 ? addresses[index]:0;
}
