#include "game_team_roles.h"
#include <string.h>

typedef struct RolePair { uint32_t score, id; } RolePair;

static int signed_less(uint32_t a, uint32_t b) {
    return (a ^ UINT32_C(0x80000000)) < (b ^ UINT32_C(0x80000000));
}

static uint32_t source_sort(RolePair pairs[5], uint32_t saved_return) {
    unsigned pass, comparison;
    uint32_t last_left=0;
    /* Original64388 quirk: four passes of FIVE comparisons; a swap does not
     * advance the cursor. This is not a conventional complete/stable sort.
     * A fifth-row comparison reads the saved negativeRA as a sixth score.
     * Real scores are0..355 (or0..255 after subtraction), so that sentinel
     * cannot win and its adjacent uninitialized stack word is never read. */
    for(pass=0;pass<4;++pass) {
        unsigned cursor=0;
        for(comparison=0;comparison<5;++comparison) {
            uint32_t right=cursor<4 ? pairs[cursor+1].score:saved_return;
            last_left=pairs[cursor].score;
            if(signed_less(last_left,right)) {
                RolePair old=pairs[cursor];
                pairs[cursor]=pairs[cursor+1];pairs[cursor+1]=old;
            } else ++cursor;
        }
    }
    return last_left;
}

Nba97GameTeamRolesResult nba97_game_team_roles(
    Nba97GameTeamRolesEffects* out, const Nba97GameTeamRolesInput* input) {
    Nba97GameTeamRolesInput in;
    Nba97GameTeamRolesEffects next;
    uint32_t t6, t1=0;
    unsigned side, i;
    /* SavedRA words of the two actual6459C calls in646A8. They are source
     * stack data for comparisons, never host pointers or native return PCs. */
    static const uint32_t saved_return[2]={UINT32_C(0x800648e8),UINT32_C(0x800648f0)};
    if(!out || !input) return NBA97_TEAM_ROLES_ARGUMENT;
    memcpy(&in,input,sizeof(in));
    if(!in.players) return NBA97_TEAM_ROLES_ARGUMENT;
    if(in.incoming_t6_known!=1) return NBA97_TEAM_ROLES_UNKNOWN_REGISTER;
    memset(&next,0,sizeof(next));t6=in.incoming_t6;

    for(side=0;side<2;++side) {
        RolePair pairs[5];
        uint8_t best17=0;
        for(i=0;i<5;++i) {
            unsigned entity=in.entity_table[side*5+i];
            unsigned opponent, opponent_entity, opponent_player, own_player, status;
            uint32_t score;
            if(entity>=10) return NBA97_TEAM_ROLES_ENTITY_REFERENCE;
            opponent=in.entity[entity].opponent_d6;
            if(opponent>=10) return NBA97_TEAM_ROLES_OPPONENT_INDEX;
            opponent_player=in.active_player_reference[opponent];
            if(opponent_player>=in.player_count) return NBA97_TEAM_ROLES_PLAYER_REFERENCE;
            opponent_entity=in.entity_table[opponent];
            if(opponent_entity>=10) return NBA97_TEAM_ROLES_ENTITY_REFERENCE;
            status=in.entity[opponent_entity].status_reference;
            if(status>=24) return NBA97_TEAM_ROLES_STATUS_REFERENCE;
            next.entity[entity].fieldd4=(uint16_t)opponent;
            next.entity[entity].written|=NBA97_ROLE_D4;
            score=in.players[opponent_player].byte0e;
            if(score<in.players[opponent_player].byte0f) score=in.players[opponent_player].byte0f;
            if(in.status_byte1e[status]==2) score+=100;
            pairs[i].score=score;pairs[i].id=opponent;
            own_player=in.entity[entity].player_reference;
            if(own_player>=in.player_count) return NBA97_TEAM_ROLES_PLAYER_REFERENCE;
            if(best17<in.players[own_player].byte17) {
                best17=in.players[own_player].byte17;t6=in.entity[entity].word00;
            }
        }
        /* All-zero byte17 retains incomingt6, including the firstcall's84
         * low byte or the prior team's winner.64388 never modifies t6. */
        next.team[side].field61=(uint8_t)t6;
        next.after6459c_t6[side]=t6;
        t1=source_sort(pairs,saved_return[side]);
        for(i=0;i<5;++i) {
            uint32_t id=pairs[i].id&255;
            next.team[side].order5c[i]=(uint8_t)id;
            pairs[i].id=id+((id<5 ? id:id-5)<<8);
            /* Source strips100 by threshold, even if a raw rating>=100 had
             * no status bonus. Preserve that byte-domain quirk. */
            if(!signed_less(pairs[i].score,100)) pairs[i].score-=100;
        }
        /* Second pass usesa3 instead oft1. Preserve FIRST pass's finalt1. */
        source_sort(pairs,saved_return[side]);
        for(i=0;i<5;++i) {
            unsigned entity=in.entity_table[pairs[i].id&255];
            next.team[side].orderbb[i]=(uint8_t)(pairs[i].id>>8);
            next.entity[entity].fieldcb=(uint8_t)(5-i);
            next.entity[entity].written|=NBA97_ROLE_CB;
        }
        next.after6459c_t1[side]=t1;
    }

    for(side=0;side<2;++side) {
        unsigned first=in.entity_table[side*5];
        uint32_t best=0, second=0, t0=0;
        int t0_known=0;
        if(first>5) return NBA97_TEAM_ROLES_PHYSICAL_SPAN;
        for(i=0;i<5;++i) {
            const Nba97GameRoleEntity* entity=&in.entity[first+i];
            uint32_t score;
            if(entity->player_reference>=in.player_count) return NBA97_TEAM_ROLES_PLAYER_REFERENCE;
            score=in.players[entity->player_reference].byte0f;
            if(best<score) {
                second=best;t0=t1;t0_known=1;t1=entity->word00;best=score;
            } else if(second<score) {
                t0=entity->word00;t0_known=1;second=score;
            }
        }
        /* t0 arrives as a source scratch-stack address, but it cannot survive
         * to an observable output: two positive candidates establish it, or
         * the secondary<70 branch setsFFFF. No fake stack address is needed. */
        if(best-second>9 || second<70) { t0=UINT32_MAX;t0_known=1; }
        if(!t0_known) return NBA97_TEAM_ROLES_UNKNOWN_REGISTER;
        /* Original all-zero byte0F bug: t1 is NOT initialized in644FC. Home
         * can inherit an opponent-sort SCORE; away can inherit home's winner. */
        next.team[side].fielda6=(uint16_t)t1;
        next.team[side].fielda8=(uint16_t)t0;
        next.after644fc_t0[side]=t0;next.after644fc_t1[side]=t1;
    }
    memcpy(out,&next,sizeof(next));return NBA97_TEAM_ROLES_OK;
}
