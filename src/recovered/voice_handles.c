#include "voice_handles.h"

static int32_t s32(uint32_t u) {return u<=0x7fffffffu?(int32_t)u:-1-(int32_t)~u;}
static Nba97VoiceApiResult result(int completion,int32_t value) {
    Nba97VoiceApiResult r;r.completion=completion;r.value=value;return r;
}
static int valid(const Nba97VoiceHandles* s) {
    return s&&s->clock&&s->voices&&s->stop&&s->call;
}
static int unlock(Nba97VoiceHandles* s) {
    --s->clock->lock_depth; /* Original underflow is retained. */
    if (!s->clock->lock_depth) {
        while (s->clock->pending) {
            --s->clock->pending;
            if (nba97_music_voice_timer(s->clock,s->voices,s->call,s->context)!=1)
                return NBA97_VOICE_API_TIMER_TRAP;
        }
    }
    return NBA97_VOICE_API_COMPLETE;
}
Nba97VoiceApiResult nba97_voice_handle_lock(Nba97VoiceHandles* s) {
    if(!valid(s))return result(NBA97_VOICE_API_ARGUMENT,0);
    ++s->clock->lock_depth;return result(NBA97_VOICE_API_COMPLETE,s32(s->clock->lock_depth));
}
Nba97VoiceApiResult nba97_voice_handle_unlock(Nba97VoiceHandles* s) {
    if(!valid(s))return result(NBA97_VOICE_API_ARGUMENT,0);
    /* 7AD48's return register is unused; value0 is a native API convention. */
    return result(unlock(s),0);
}
Nba97VoiceApiResult nba97_voice_handle_resolve(Nba97VoiceHandles* s,uint32_t handle) {
    uint32_t index;int32_t found;int completion;
    if(!valid(s))return result(NBA97_VOICE_API_ARGUMENT,0);
    ++s->clock->lock_depth;
    index=handle&31u;
    if(index>=24)return result(NBA97_VOICE_API_UNOWNED_SLOT,0);
    found=s->voices[index].active==1&&s->voices[index].handle==handle?(int32_t)index:-8;
    completion=unlock(s);
    return result(completion,found);
}
Nba97VoiceApiResult nba97_voice_handle_status(Nba97VoiceHandles* s,uint32_t handle) {
    Nba97VoiceApiResult r;
    if(!valid(s))return result(NBA97_VOICE_API_ARGUMENT,0);
    if(!s->enabled)return result(NBA97_VOICE_API_COMPLETE,-10);
    r=nba97_voice_handle_resolve(s,handle);
    if(r.completion==NBA97_VOICE_API_COMPLETE)r.value=(int32_t)((uint32_t)r.value>>31);
    return r;
}
Nba97VoiceApiResult nba97_voice_handle_fade(Nba97VoiceHandles* s,uint32_t handle,uint32_t ticks,uint32_t target) {
    Nba97VoiceApiResult r;int32_t value;
    if(!valid(s))return result(NBA97_VOICE_API_ARGUMENT,0);
    if(!s->enabled)return result(NBA97_VOICE_API_COMPLETE,-10);
    if(target+1u>=129u)return result(NBA97_VOICE_API_COMPLETE,-8);
    ++s->clock->lock_depth;r=nba97_voice_handle_resolve(s,handle);
    if(r.completion!=NBA97_VOICE_API_COMPLETE)return r;
    value=r.value<0?-8:nba97_music_voice_fade(&s->voices[r.value],ticks,target);
    return result(unlock(s),value);
}
Nba97VoiceApiResult nba97_voice_handle_gain(Nba97VoiceHandles* s,uint32_t handle,uint32_t gain) {
    Nba97VoiceApiResult r;int32_t value=-8;
    if(!valid(s))return result(NBA97_VOICE_API_ARGUMENT,0);
    if(!s->enabled)return result(NBA97_VOICE_API_COMPLETE,-10);
    if(gain>=128u)return result(NBA97_VOICE_API_COMPLETE,-8);
    ++s->clock->lock_depth;r=nba97_voice_handle_resolve(s,handle);
    if(r.completion!=NBA97_VOICE_API_COMPLETE)return r;
    if(r.value>=0) {
        Nba97MusicVoice* v=&s->voices[r.value];
        value=nba97_music_voice_gain(v,gain);
        nba97_music_voice_effective(v,s->clock->master_gain,s->call,s->context);
        s->call(s->context,NBA97_VOICE_APPLY,(uint32_t)r.value,
            v->effective_gain<128?v->effective_gain:(uint32_t)v->effective_gain-256u,0);
    }
    return result(unlock(s),value);
}
int nba97_voice_stop_request(Nba97VoiceStopState* s,uint32_t index) {
    uint32_t tracked;
    if(!s||index>=24)return NBA97_VOICE_API_ARGUMENT;
    if(index==s->excluded_voice)return NBA97_VOICE_API_COMPLETE;
    s->changing=1;
    tracked=s->tracked_stream<128?s->tracked_stream:(uint32_t)s->tracked_stream-256u;
    if(index==tracked)s->stream_stop=1;
    else {
        s->keyoff_mask|=1u<<index;
        if(s->channel[index].kind>=2)
            s->keyoff_mask|=1u<<(s->channel[index].paired_voice&31u);
    }
    s->changing=0;
    return NBA97_VOICE_API_COMPLETE;
}
Nba97VoiceApiResult nba97_voice_handle_stop(Nba97VoiceHandles* s,uint32_t handle) {
    Nba97VoiceApiResult r;int32_t value=-8;
    if(!valid(s))return result(NBA97_VOICE_API_ARGUMENT,0);
    if(!s->enabled)return result(NBA97_VOICE_API_COMPLETE,-10);
    ++s->clock->lock_depth;r=nba97_voice_handle_resolve(s,handle);
    if(r.completion!=NBA97_VOICE_API_COMPLETE)return r;
    if(r.value>=0){nba97_voice_stop_request(s->stop,(uint32_t)r.value);value=0;}
    return result(unlock(s),value);
}
