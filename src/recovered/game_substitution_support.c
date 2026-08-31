#include "game_substitution_support.h"
#include <string.h>

static int valid_value(const Nba97GameClockValue* v) {
    return v && v->known<=1 && (v->known || v->word==0);
}
int nba97_game_clock_sample(Nba97GameClockEffect* out,
    const Nba97GameClockValue* previous,const Nba97GameClockValue* sample) {
    Nba97GameClockEffect next;
    if(!out || !valid_value(previous) || !valid_value(sample))return 0;
    memset(&next,0,sizeof(next));next.previous164=*sample;
    if(previous->known && sample->known) {
        next.delta.word=sample->word-previous->word;next.delta.known=1;
    }
    memcpy(out,&next,sizeof(next));return 1;
}
int nba97_game_wait_begin(Nba97GameWaitCursor* cursor,unsigned owner) {
    if(!cursor || owner>NBA97_WAIT_64964)return NBA97_WAIT_ARGUMENT;
    cursor->owner=(uint8_t)owner;cursor->stage=0;cursor->terminal=0;
    return NBA97_WAIT_PROGRESS;
}
int nba97_game_wait_step(Nba97GameWaitCursor* cursor,Nba97GameWaitLiveState* state,
                         Nba97GameWaitCallback callback,void* context) {
    unsigned stage,boundary=0;int poll=0,result;
    Nba97GameClockValue reply={0,0};
    if(!cursor || !state || cursor->owner>1 || cursor->terminal>2 ||
       cursor->stage>(cursor->owner ? 9:3) || !valid_value(&state->sample_d7a70) ||
       !valid_value(&state->previous164) || !valid_value(&state->frame6c) ||
       state->frame6c.word>65535)return NBA97_WAIT_ARGUMENT;
    if(cursor->terminal)return cursor->terminal==1?NBA97_WAIT_COMPLETE:NBA97_WAIT_ALREADY_FAILED;
    stage=cursor->stage;
    if(stage==0 || stage==2 || (cursor->owner && (stage==6 || stage==8))) {
        Nba97GameClockEffect effect;
        nba97_game_clock_sample(&effect,&state->previous164,&state->sample_d7a70);
        state->previous164=effect.previous164;
        if(stage==2 || stage==8) {
            state->frame6c=effect.delta;state->frame6c.word&=65535;
        }
        cursor->stage=(uint8_t)(stage+1);return NBA97_WAIT_PROGRESS;
    }
    if(!cursor->owner) {
        boundary=stage==1?NBA97_WAIT_POLL_70068:NBA97_WAIT_PUMP_2DD84;poll=stage==1;
    } else {
        switch(stage) {
        case 1:boundary=NBA97_WAIT_QUERY_31CB8;poll=1;break;
        case 3:case 9:boundary=NBA97_WAIT_PUMP_2DD84;break;
        case 4:case 7:boundary=NBA97_WAIT_POLL_70068;poll=1;break;
        case 5:boundary=NBA97_WAIT_STOP_31C8C;break;
        default:return NBA97_WAIT_ARGUMENT;
        }
    }
    if(!callback)return NBA97_WAIT_CALLBACK_REQUIRED;
    result=callback(context,state,boundary,&reply);
    if(result!=1) {cursor->terminal=2;return NBA97_WAIT_CALLBACK_FAILED;}
    if(poll && (!valid_value(&reply) || !reply.known)) {
        cursor->terminal=2;return NBA97_WAIT_RETURN_UNKNOWN;
    }
    if(!cursor->owner) {
        if(stage==1 && reply.word==0) {cursor->terminal=1;return NBA97_WAIT_COMPLETE;}
        cursor->stage=(uint8_t)(stage==1?2:1);
    } else switch(stage) {
    case 1:cursor->stage=(uint8_t)((reply.word&255)?2:6);break;
    case 3:cursor->stage=4;break;
    case 4:cursor->stage=(uint8_t)(reply.word?5:1);break;
    case 5:cursor->stage=1;break;
    case 7:
        if(!reply.word) {cursor->terminal=1;return NBA97_WAIT_COMPLETE;}
        cursor->stage=8;break;
    case 9:cursor->stage=7;break;
    default:return NBA97_WAIT_ARGUMENT;
    }
    return NBA97_WAIT_PROGRESS;
}
