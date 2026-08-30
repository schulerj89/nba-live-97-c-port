#include "team_select.h"
#include <string.h>

int nba97_team_ranks_valid(const Nba97TeamRanks* ranks) {
    unsigned category, team;
    if (!ranks) return 0;
    for (category=0; category<5; ++category) {
        uint32_t seen=0;
        for (team=0; team<31; ++team) {
            unsigned rank=ranks->value[category][team];
            if (!rank || rank>31 || (seen & (1u<<(rank-1)))) return 0;
            seen |= 1u<<(rank-1);
        }
    }
    return 1;
}

int nba97_team_select_open(Nba97TeamSelect* out, int home, int away,
                          int remembered_home, int remembered_away) {
    Nba97TeamSelect state;
    if (!out || away<0 || away>=31 || home<0 || home>=31 ||
        remembered_away<0 || remembered_away>=29 ||
        remembered_home<0 || remembered_home>=29) return 0;
    memset(&state,0,sizeof(state));
    state.team[0]=(int16_t)home; state.team[1]=(int16_t)away;
    state.remembered_regular[0]=(int16_t)remembered_home;
    state.remembered_regular[1]=(int16_t)remembered_away;
    *out=state;
    return 1;
}

static void finish(Nba97TeamSelect* state, int result) {
    unsigned side;
    /* 8004FC80 runs for both return codes. Select does NOT undo team scans. */
    for (side=0; side<2; ++side)
        if (state->team[side]<29) state->remembered_regular[side]=state->team[side];
    state->result=(int8_t)result;
}

int nba97_team_select_restore_focus(Nba97TeamSelect* state, unsigned descriptor) {
    if (!state || state->result || descriptor>=12) return 0;
    state->side=(uint8_t)(descriptor/6); state->criterion=(uint8_t)(descriptor%6);
    return 1;
}

Nba97TeamSelectEvent nba97_team_select_input(Nba97TeamSelect* state,
                                            const Nba97TeamRanks* ranks,
                                            uint16_t token) {
    int direction, team;
    unsigned rank;
    if (!state || state->side>1 || state->criterion>5 || state->result ||
        state->team[0]<0 || state->team[0]>=31 ||
        state->team[1]<0 || state->team[1]>=31) return NBA97_SELECT_NONE;
    state->sound=0;
    switch (token) {
    case 1: case 2:
        /* 3A5B0/3A5DC, endpoint callbacks 3A608/3A62C. */
        state->criterion=(uint8_t)((state->criterion+(token==1 ? 5:1))%6);
        state->sound=(uint8_t)(token==1 ? 3:4);
        return NBA97_SELECT_CRITERION;
    case 4: case 8:
        direction=token==8 ? -1:1;
        team=state->team[state->side];
        if (!state->criterion) {
            /* 4F17C -> 3ACC4 -> 3AC10/3AC6C: descriptor count 31. */
            team=(team+31+direction)%31;
        } else {
            if (!nba97_team_ranks_valid(ranks)) return NBA97_SELECT_NONE;
            /* 4F058/4F0EC: 1-based category rank wraps; 4F020 inverse scan. */
            rank=(unsigned)((ranks->value[state->criterion-1][team]+30+direction)%31+1);
            for (team=0; team<31; ++team)
                if (ranks->value[state->criterion-1][team]==rank) break;
        }
        state->team[state->side]=(int16_t)team;
        state->sound=(uint8_t)(token==8 ? 2:1);
        return NBA97_SELECT_TEAM;
    case 0x800:
        /* 4F9D8 -> 4F7B8 copies corresponding criterion to the other page. */
        state->side^=1; state->sound=6;
        return NBA97_SELECT_SIDE;
    case 0x40: state->sound=6; return NBA97_SELECT_RANDOM;
    case 0x20: return NBA97_SELECT_HELP;
    case 0x100: state->sound=10; finish(state,-1); return NBA97_SELECT_RETURN;
    case 0x80: state->sound=9; finish(state,1); return NBA97_SELECT_CONTINUE;
    default: return NBA97_SELECT_NONE; /* 4F9D8 clears the sound latch. */
    }
}

int nba97_team_select_random_candidate(Nba97TeamSelect* state, uint16_t word) {
    unsigned team=word&31u;
    if (!state || state->result || state->side>1 || team>28) return 0;
    state->team[state->side]=(int16_t)team;
    return 1;
}

uint32_t nba97_team_select_rng_step(uint32_t state[6]) {
    uint64_t carry=0;
    int i;
    if(!state) return 0;
    for(i=4;i>=0;--i) {
        uint64_t sum=(uint64_t)state[i]+state[i+1]+carry;
        state[i]=(uint32_t)sum;carry=sum>>32;
    }
    for(i=5;i>=0;--i) if(++state[i]) break;
    return state[0];
}

static int random_next(Nba97TeamRandom* animation, Nba97TeamSelect* state, uint32_t rng[6]) {
    do {} while (!nba97_team_select_random_candidate(state,(uint16_t)nba97_team_select_rng_step(rng)));
    animation->wait=(uint8_t)(13-animation->remaining);
    --animation->remaining; /* Caller wait5 and the next poll1 belong to3D930/3AE4C. */
    return 1;
}
int nba97_team_random_busy(const Nba97TeamRandom* animation) {
    return animation && (animation->remaining || animation->wait);
}
int nba97_team_random_begin(Nba97TeamRandom* animation, Nba97TeamSelect* state, uint32_t rng[6]) {
    if (!animation || !state || !rng || state->result || state->side>1 ||
        nba97_team_random_busy(animation)) return 0;
    animation->remaining=12;animation->wait=0;
    return random_next(animation,state,rng);
}
int nba97_team_random_tick(Nba97TeamRandom* animation, Nba97TeamSelect* state, uint32_t rng[6]) {
    if (!animation || !state || !rng || state->result || state->side>1 || !animation->wait) return 0;
    if (--animation->wait || !animation->remaining) return 0;
    return random_next(animation,state,rng);
}
