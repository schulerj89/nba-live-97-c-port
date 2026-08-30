#include "team_select_poll.h"

void nba97_team_poll_open(Nba97TeamPoll* s) {
    if(s) {s->phase=NBA97_TEAM_SETTLE;s->post_remaining=0;}
}
int nba97_team_poll_prepare(Nba97TeamPoll* s,int moving) {
    if(!s || s->phase==NBA97_TEAM_CALLBACK || s->phase==NBA97_TEAM_POLL_CLOSED) return 0;
    if(s->phase==NBA97_TEAM_SETTLE && !moving) s->phase=NBA97_TEAM_POLL;
    return 1;
}
static int sample(Nba97TeamPadHistory* s,const uint16_t pads[8],Nba97TeamSample* out) {
    unsigned i;uint16_t raw=0;
    for(i=0;i<8;++i) if((raw=pads[i])!=0) break;
    if(!raw) {s->prior_controller=255;return 0;}
    if(raw==s->prior_mask && i==s->prior_controller) {
        /* Do not clamp after increment: the source maps odd47 to49. */
        if(s->repeat_counter<48) s->repeat_counter+=2;
    } else s->repeat_counter=0;
    s->prior_mask=raw;s->prior_controller=(uint8_t)i;
    out->token=raw;out->controller=(uint8_t)i;
    out->delay=(raw==4 || raw==8) ?
        (s->repeat_counter<=15 ? 7:s->repeat_counter<=27 ? 5:s->repeat_counter<=37 ? 3:1):4;
    return 1;
}
int nba97_team_poll_presented(Nba97TeamPoll* s,const uint16_t pads[8],Nba97TeamSample* out) {
    if(!s || !pads || !out) return NBA97_TEAM_POLL_NONE;
    if(s->phase==NBA97_TEAM_POST_WAIT) {
        if(s->post_remaining && !--s->post_remaining) s->phase=NBA97_TEAM_SETTLE;
    } else if(s->phase==NBA97_TEAM_POLL && sample(&s->pad,pads,out)) {
        s->phase=NBA97_TEAM_CALLBACK;return NBA97_TEAM_POLL_INPUT;
    } else if(s->phase==NBA97_TEAM_EXIT_CHANGE) {
        if(s->pad.prior_controller<8 && pads[s->pad.prior_controller]!=s->pad.prior_mask)
            s->phase=NBA97_TEAM_EXIT_FINAL;
    } else if(s->phase==NBA97_TEAM_EXIT_FINAL) {
        s->phase=NBA97_TEAM_POLL_CLOSED;return NBA97_TEAM_POLL_EXITED;
    }
    return NBA97_TEAM_POLL_NONE;
}
unsigned nba97_team_poll_caller_wait(uint16_t token,unsigned delay) {
    if(token==1 || token==2 || token==4 || token==8) return delay;
    return token&0x3e50u ? 5:0;
}
void nba97_team_poll_finish_callback(Nba97TeamPoll* s,unsigned wait) {
    if(!s || s->phase!=NBA97_TEAM_CALLBACK || wait>65535) return;
    s->post_remaining=(uint16_t)wait;s->phase=wait ? NBA97_TEAM_POST_WAIT:NBA97_TEAM_SETTLE;
}
int nba97_team_poll_exit(Nba97TeamPoll* s) {
    if(!s || s->phase!=NBA97_TEAM_CALLBACK || s->pad.prior_controller>=8) return 0;
    s->phase=NBA97_TEAM_EXIT_CHANGE;return 1;
}
int nba97_team_poll_setup_start(Nba97TeamPoll* s,unsigned controller) {
    uint16_t pads[8]={0};Nba97TeamSample next;
    if(!s || controller>=8) return 0;
    pads[controller]=0x80;
    sample(&s->pad,pads,&next);sample(&s->pad,pads,&next);
    s->phase=NBA97_TEAM_EXIT_CHANGE;s->post_remaining=0;return 1;
}
