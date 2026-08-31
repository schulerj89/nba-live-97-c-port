#include "voice_channels.h"

static int invoke(Nba97VoiceChannelInvoke call,void* ctx,enum Nba97VoiceChannelCall kind,
    uint32_t a,uint32_t b,uint32_t* value) {
    return call(ctx,kind,a,b,value)==1?NBA97_CHANNEL_COMPLETE:NBA97_CHANNEL_IO_REFUSED;
}
static int keys(Nba97VoiceChannels* s,Nba97VoiceChannelInvoke call,void* ctx,int on) {
    uint32_t* mask=on?&s->keyon_mask:&s->stop->keyoff_mask;
    unsigned i;uint32_t unused=0;int r;
    if(!*mask)return NBA97_CHANNEL_COMPLETE;
    r=invoke(call,ctx,NBA97_CHANNEL_KEY_6F858,(uint32_t)on,*mask,&unused);if(r!=1)return r;
    for(i=0;i<24;++i) {
        if((*mask>>i)&1u) {
            s->state[i]=on?2u:1u;
            /* KEY batch kind==2 differs from state4/71A68's kind>=2. */
            if(s->stop->channel[i].kind==2) {
                unsigned pair=s->stop->channel[i].paired_voice;
                if(pair>=24)return NBA97_CHANNEL_UNOWNED_PAIR;
                s->state[pair]=on?2u:1u;
            }
        }
    }
    *mask=0; /* Original clears even bits newly supplied by its callback. */
    return NBA97_CHANNEL_COMPLETE;
}
int nba97_voice_channels_service(Nba97VoiceChannels* s,Nba97VoiceChannelInvoke call,void* ctx) {
    unsigned i;int r;uint32_t value=0;
    if(!s||!s->stop||!s->voices||!s->finished||!call)return NBA97_CHANNEL_ARGUMENT;
    r=invoke(call,ctx,NBA97_CHANNEL_SAMPLE_7AEE4,0,0,&value);if(r!=1)return r;
    s->busy=1;
    if(s->stream_maintenance&&!s->stream_pending&&!s->transfer_pending) {
        r=invoke(call,ctx,NBA97_CHANNEL_STREAM_72954,0,0,&value);if(r!=1)return r;
    }
    if(s->stop->changing)return NBA97_CHANNEL_COMPLETE; /* Original leaves busy1. */
    for(i=0;i<24;++i) {
        uint32_t state=s->state[i];
        if(state==4) {
            if(i==s->stop->excluded_voice)continue;
            r=invoke(call,ctx,NBA97_CHANNEL_STATUS_7BFA0,1u<<i,0,&value);if(r!=1)return r;
            s->hardware_result=value;
            if(s->hardware_result!=1) {
                s->state[i]=1;s->stop->keyoff_mask|=1u<<i;
                if(s->stop->channel[i].kind>=2) {
                    unsigned pair=s->stop->channel[i].paired_voice;
                    if(pair>=24)return NBA97_CHANNEL_UNOWNED_PAIR;
                    s->state[pair]=1;
                    s->stop->keyoff_mask|=1u<<(s->stop->channel[i].paired_voice&31u);
                }
            }
        }else if(state==2) {
            r=invoke(call,ctx,NBA97_CHANNEL_STATUS_7BFA0,1u<<i,0,&value);if(r!=1)return r;
            s->hardware_result=value;if(s->hardware_result==1)s->state[i]=4;
        }else if(state==1) {
            r=invoke(call,ctx,NBA97_CHANNEL_STATUS_7BFA0,1u<<i,0,&value);if(r!=1)return r;
            s->hardware_result=value;
            if(!s->hardware_result) {
                uint32_t tracked=s->stop->tracked_stream<128?s->stop->tracked_stream:(uint32_t)s->stop->tracked_stream-256u;
                if(i==tracked){s->stop->tracked_stream=255;*s->finished=1;}
                if(s->transient[i])s->transient[i]=0;
                s->state[i]=0;s->voices[i].active=0; /*916AC retains handle. */
            }
        }
    }
    if(s->auxiliary_enabled&&s->auxiliary_on) {
        r=invoke(call,ctx,NBA97_CHANNEL_AUXILIARY_7E684,1,s->auxiliary_on,&value);if(r!=1)return r;
        s->auxiliary_on=0;
    }
    if(s->auxiliary_off) {
        r=invoke(call,ctx,NBA97_CHANNEL_AUXILIARY_7E684,0,s->auxiliary_off,&value);if(r!=1)return r;
        s->auxiliary_off=0;
    }
    r=keys(s,call,ctx,1);if(r!=1)return r;
    r=keys(s,call,ctx,0);if(r!=1)return r;
    s->busy=0;return NBA97_CHANNEL_COMPLETE;
}
