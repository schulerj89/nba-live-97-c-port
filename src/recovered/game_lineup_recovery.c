#include "game_lineup_recovery.h"
#include <stddef.h>

static int32_t signed16(uint16_t value) {
    return value<0x8000u ? (int32_t)value : (int32_t)value-65536;
}

Nba97GameRecoveryResult nba97_game_lineup_auto_substitute(
    Nba97GameLineupRecoveryState* state, unsigned side, int32_t roster_base,
    Nba97GameRecoverySubstitute substitute, void* context) {
    unsigned slot;
    uint32_t first=1;
    if(!state || side>=2) return NBA97_RECOVERY_ARGUMENT;
    state->substitution_lock=1;
    for(slot=0;slot<5;++slot) {
        int32_t preferred=signed16(state->team[side].preferred[slot]);
        int32_t bench;
        uint32_t status_index;
        if(preferred<0 || preferred>=12) return NBA97_RECOVERY_OUTSIDE_STORAGE;
        bench=signed16(state->team[side].inverse[preferred]);
        if(bench<5) continue;
        status_index=(uint32_t)preferred+(uint32_t)roster_base;
        if(status_index>=24) return NBA97_RECOVERY_OUTSIDE_STORAGE;
        if(signed16(state->status[status_index])<0x7332) continue;
        if(!substitute) return NBA97_RECOVERY_CALLBACK_REQUIRED;
        if(!substitute(context,state,side,(int32_t)slot,bench,0,first))
            return NBA97_RECOVERY_CALLBACK_FAILED;
        first=0;
    }
    state->substitution_lock=0;
    return NBA97_RECOVERY_OK;
}

Nba97GameRecoveryResult nba97_game_lineup_recover(
    Nba97GameLineupRecoveryState* state, int32_t elapsed,
    Nba97GameRecoverySubstitute substitute, void* context) {
    unsigned i;
    uint32_t delta=(uint32_t)elapsed*23u;
    if(!state) return NBA97_RECOVERY_ARGUMENT;
    for(i=0;i<24;++i) {
        int32_t current=signed16(state->status[i]);
        if(current==-1) {
            unsigned side,base,position,insert;
            Nba97GameRecoveryTeam* team;
            if(elapsed<120) continue;
            side=i<12 ? 0u:1u;
            base=side*12;
            team=&state->team[side];
            for(position=0;position<12;++position)
                if(signed16(team->lineup[position])==(int32_t)(i-base)) break;
            if(position==12) return NBA97_RECOVERY_OUTSIDE_STORAGE;
            /* Original65140 bug/quirk: position0 never recovers fromFFFF.
             * For later entries, the backward scan has no lower bound in
             * source. Refuse an unowned read instead of inventing a fallback. */
            if(position==0) continue;
            insert=position;
            for(;;) {
                uint32_t previous;
                if(insert==0) return NBA97_RECOVERY_OUTSIDE_STORAGE;
                previous=(uint32_t)signed16(team->lineup[insert-1])+base;
                if(previous>=24) return NBA97_RECOVERY_OUTSIDE_STORAGE;
                if(signed16(state->status[previous])>=0) break;
                --insert;
            }
            if(insert!=position) {
                uint16_t old=team->lineup[position];
                team->lineup[position]=team->lineup[insert];
                team->lineup[insert]=old;
            }
            /* Even without a swap, source increments+66 and writes7332. */
            team->recovery_count=(uint16_t)(team->recovery_count+1u);
            state->status[i]=0x7332;
        } else if(current>=0 && current!=0x7fff) {
            uint16_t value=(uint16_t)((uint32_t)current+delta);
            /* Original uses only the low16 sign bit, not a wide clamp.
             * Large/negative elapsed values can wrap to small positive values. */
            state->status[i]=(value&0x8000u) ? 0x7fff:value;
        }
    }
    if(elapsed>=120) {
        Nba97GameRecoveryResult result;
        state->marker=0x11;
        if(state->team[0].human_count==0 || state->team[0].automatic!=0) {
            result=nba97_game_lineup_auto_substitute(state,0,0,substitute,context);
            if(result!=NBA97_RECOVERY_OK) return result;
        }
        /* Source rereads away state AFTER the home callback, not before. */
        if(state->team[1].human_count==0 || state->team[1].automatic!=0) {
            result=nba97_game_lineup_auto_substitute(state,1,12,substitute,context);
            if(result!=NBA97_RECOVERY_OK) return result;
        }
    }
    return NBA97_RECOVERY_OK;
}
