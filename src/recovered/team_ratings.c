#include "team_ratings.h"
#include <string.h>

static int32_t arithmetic_eighth(int32_t value) {
    return value>=0 ? value/8 : -((-value+7)/8);
}

int nba97_team_ratings(const Nba97TeamRatingInput teams[29],
                       const int16_t adjustments[29],
                       uint16_t scores[5][29], Nba97TeamRanks* ranks) {
    uint16_t values[5][29];
    Nba97TeamRanks output;
    unsigned team, category, slot, i, j;
    if (!teams || !adjustments || !scores || !ranks) return 0;
    for(team=0;team<29;++team) if(teams[team].count<8 || teams[team].count>15) return 0;
    for(team=0;team<29;++team) {
        int32_t totals[5][3]={{0}};
        for(slot=0;slot<teams[team].count;++slot) {
            const uint8_t* r=teams[team].ratings[slot];
            int32_t a=r[0]+r[1]+r[2]+r[3]-200;
            int32_t b=r[8]+r[9]+r[10]+r[11]-200;
            int32_t c=r[12]+r[13]+r[14]+r[15]-200;
            int32_t d=r[4]+r[5]+r[6]+r[7]-200;
            int32_t raw[5]={4*a,4*b,4*c,4*d,a+b+c+d+r[16]};
            unsigned tier=slot<5 ? 0:slot<8 ? 1:2;
            for(category=0;category<5;++category)
                totals[category][tier]+=raw[category]*(slot<5 ? 2:1);
        }
        for(category=0;category<5;++category) {
            int32_t* sum=totals[category];
            int32_t deep_count=teams[team].count-8;
            int32_t score;
            /* Retail's exact eight-player fallback, not a divide-by-zero fix. */
            if (!deep_count) { sum[2]=sum[1]-20; deep_count=3; }
            score=(sum[0]*250/5 + sum[1]*250/3 +
                   arithmetic_eighth(sum[2]*1000/deep_count))/14 + adjustments[team];
            values[category][team]=(uint16_t)(score<0 ? 0:score>65535 ? 65535:score);
        }
    }
    for(category=0;category<5;++category) {
        uint8_t order[29];
        for(i=0;i<29;++i) order[i]=(uint8_t)i;
        /* Stable descending unsigned scores: equality never exchanges IDs. */
        for(i=1;i<29;++i) {
            uint8_t candidate=order[i]; j=i;
            while(j && values[category][order[j-1]]<values[category][candidate]) {
                order[j]=order[j-1]; --j;
            }
            order[j]=candidate;
        }
        for(i=0;i<29;++i) output.value[category][order[i]]=(uint8_t)(i+1);
        output.value[category][29]=30; output.value[category][30]=31;
    }
    memcpy(scores,values,sizeof(values)); *ranks=output;
    return 1;
}
