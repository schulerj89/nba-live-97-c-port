#include "roster_reset.h"
#include <string.h>

int nba97_reset_enabled(const uint16_t *working, const uint16_t *defaults,
                        uint8_t special_active, int8_t special_kind) {
    unsigned i;
    if (!working || !defaults || (special_active && special_kind == 1)) return 0;
    for (i=0;i<535;++i) if (working[i]!=defaults[i]) return 1;
    return 0;
}
int nba97_reset_open(Nba97ResetPrompt *p, Nba97HelpRect rect, uint16_t held, int preference) {
    if (!p || nba97_help_open(&p->modal,rect,held)!=NBA97_HELP_OPEN_SOUND) return 0;
    p->choice=preference ? 0 : 1;
    p->initial_choice=p->choice;
    memset(p->tint,0,sizeof(p->tint));
    memset(p->tint[0].rgb,128,3);memset(p->tint[1].rgb,128,3);
    p->cooldown=0;
    return NBA97_RESET_OPEN; /* 40A1C style1, choice dialog: sound12 */
}
int nba97_reset_input(Nba97ResetPrompt *p, uint16_t raw) {
    int event=0;
    uint8_t previous;
    if (!p) return 0;
    if (p->modal.phase==NBA97_HELP_WAIT_CHANGE) {
        if (raw==p->modal.held) return 0;
        p->modal.phase=NBA97_HELP_READY;
    }
    if (p->modal.phase!=NBA97_HELP_READY || !raw || p->cooldown) return 0;
    previous=p->choice;
    if (raw==1 && p->choice) {p->choice=0;event=NBA97_RESET_UP;}
    if (raw==2 && !p->choice) {p->choice=1;event=NBA97_RESET_DOWN;}
    if(previous!=p->choice) {
        nba97_reorder_tint_unpulse(&p->tint[previous]);
        nba97_reorder_tint_pulse(&p->tint[p->choice]);
    }
    /* Generic modal advances eight updates after each sampled input. */
    p->cooldown=8;
    if (raw==0x800) {
        p->modal.held=raw;
        p->modal.phase=NBA97_HELP_SHRINKING;
        event=NBA97_RESET_CHOSEN; /* sound6, remove text, sound8 */
    }
    return event;
}
int nba97_reset_tick(Nba97ResetPrompt *p, uint16_t raw) {
    if (!p) return 0;
    if (p->modal.phase==NBA97_HELP_GROWING || p->modal.phase==NBA97_HELP_SHRINKING ||
        p->modal.phase==NBA97_HELP_RETURN_BARRIER) {
        const int growing=p->modal.phase==NBA97_HELP_GROWING;
        const int event=nba97_help_tick(&p->modal,raw);
        if(growing && p->modal.phase==NBA97_HELP_WAIT_CHANGE)
            nba97_reorder_tint_pulse(&p->tint[p->choice]);
        return event==NBA97_HELP_RETURNED ? NBA97_RESET_RETURN : 0;
    }
    if(nba97_help_text_visible(&p->modal)) {
        nba97_reorder_tint_tick(&p->tint[0]);nba97_reorder_tint_tick(&p->tint[1]);
    }
    if (p->cooldown) --p->cooldown;
    return nba97_reset_input(p,raw);
}
