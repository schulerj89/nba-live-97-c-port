#include "game_controllers.h"
#include <string.h>

int nba97_game_controllers_initialize(Nba97GameControllersEffects* out,
                                     const Nba97GameControllersInput* input) {
    Nba97GameControllersInput source;
    Nba97GameControllersEffects next;
    int i;
    if(!out || !input) return 0;
    memcpy(&source,input,sizeof(source));
    for(i=0;i<8;++i)
        if(source.previous_selected[i].known>NBA97_GAME_SELECTION_KNOWN ||
           (!source.previous_selected[i].known && source.previous_selected[i].word)) return 0;

    /* Zero only this native effect object, never an original record. */
    memset(&next,0,sizeof(next));
    for(i=9;i>=0;--i) next.player_claim[i]=-1;
    next.marker=-1;
    for(i=7;i>=0;--i) {
        next.controller_binding[i]=(uint8_t)i;
        if(source.assignment[i]==1) {
            next.team_base[i]=0;
            ++next.human_count[0];
        } else if(source.assignment[i]==2) {
            next.team_base[i]=5;
            ++next.human_count[1];
        } else {
            /* Source quirk: nonstandard assignment bytes are also neutral. */
            next.team_base[i]=-1;
        }
        if(next.team_base[i]<0) {
            next.selected[i].word=0xffff;
            next.selected[i].known=NBA97_GAME_SELECTION_KNOWN;
            next.selected_written[i]=1;
        } else {
            /* Preserve the source's potentially stale/invalid selected word.
             * Joining is not permission to repair it or pick a new entity. */
            next.selected[i].word=source.previous_selected[i].word;
            next.selected[i].known=source.previous_selected[i].known;
        }
    }
    memcpy(out,&next,sizeof(next));
    return 1;
}
