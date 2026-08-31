#include "game_animation_advance.h"
#include "game_animation_internal.h"

unsigned nba97_game_animation_extra_offset(unsigned field) {return field<23?ai_extra_offset[field]:0;}
static int32_t signed16(uint32_t v) {return v<32768?(int32_t)v:(int32_t)v-65536;}
static void copy_field(Nba97GameAnimationEffects* e,unsigned to,unsigned from) {ai_put(e,to,ai_get(&e->state,from));}
static void repeated(Nba97GameAnimationEffects* e,unsigned offset,uint32_t value,uint64_t count) {
    if(count) {ai_assign(e,offset,value);e->store_count+=count-1;}
}
static int global_half(Nba97GamePeriodValue value,int32_t* out) {
    if(!value.known)return NBA97_ANIMATION_UNRESOLVED;
    *out=signed16(value.word);return NBA97_ANIMATION_OK;
}
static int clip(const Nba97GameAnimationResources* r,unsigned channel,uint32_t request,
                const Nba97GameAnimationClipView** out) {
    const uint32_t index=request&0x3fffffffu;
    if(!r || index>=84)return NBA97_ANIMATION_REFERENCE;
    *out=&r->clip[channel][index];
    if((*out)->header.available==2)return NBA97_ANIMATION_UNRESOLVED;
    return (*out)->header.available==1?NBA97_ANIMATION_OK:NBA97_ANIMATION_REFERENCE;
}
static int lookup(const Nba97GameAnimationResources* r,unsigned table,int32_t index,int32_t* value) {
    const Nba97GameAnimationMapView* m;
    int64_t position;
    if(!r)return NBA97_ANIMATION_REFERENCE;
    m=&r->map[table];position=(int64_t)index-m->first_index;
    if(position<0 || (uint64_t)position>=m->count || !m->words)return NBA97_ANIMATION_REFERENCE;
    *value=signed16(m->words[(size_t)position]);return NBA97_ANIMATION_OK;
}
static int reset_frame(Nba97GameAnimationEffects* e,unsigned frame,unsigned time,uint8_t count) {
    uint32_t f=0;
    if(count) AI_READ(e,frame,f);
    if(!count || f>=count) {ai_assign(e,time,0);ai_assign(e,frame,0);}
    return NBA97_ANIMATION_OK;
}
static int remap(Nba97GameAnimationEffects* e,const Nba97GameAnimationAdvanceInput* source) {
    uint32_t v,old_default,live,height,id;int32_t request,controlled;
    const Nba97GameAnimationClipView* h;unsigned field;
    AI_READ(e,0xa0,v);AI_READ(e,0x4e,old_default);
    AI_TRY(lookup(source->resources,signed16(v)<6?NBA97_ANIMATION_MAP_B8564:NBA97_ANIMATION_MAP_B8590,(int32_t)old_default,&request));
    AI_READ(e,0xe4,v);AI_TRY(lookup(source->resources,v?NBA97_ANIMATION_MAP_B85E8:NBA97_ANIMATION_MAP_B8614,request,&request));
    AI_READ(e,0,id);AI_TRY(global_half(source->controlled_fdbcc,&controlled));
    AI_TRY(lookup(source->resources,id==(uint32_t)controlled?NBA97_ANIMATION_MAP_B850C:NBA97_ANIMATION_MAP_B8538,request,&request));
    AI_READ(e,0x10,height);
    if(height) AI_TRY(lookup(source->resources,NBA97_ANIMATION_MAP_B85BC,request,&request));
    /* Source keeps default11 when mapped result0, even while live clips change. */
    if(old_default!=11 || request)ai_assign(e,0x4e,(uint16_t)request);
    for(field=0;field<4;++field) {
        static const uint8_t offsets[4]={0x46,0x4a,0x48,0x4c};
        static const uint8_t flags[4]={0x60,0x64,0x62,0x66};
        static const uint8_t frames[4]={0x50,0x54,0x52,0x56};
        static const uint8_t times[4]={0x58,0x5c,0x5a,0x5e};
        AI_READ(e,offsets[field],live);
        if((live<21 && live!=(uint32_t)request) || (field<2 && live==65535)) {
            ai_assign(e,offsets[field],(uint16_t)request);
            AI_TRY(clip(source->resources,field&1,(uint32_t)request,&h));
            ai_assign(e,flags[field],h->header.flags);
            AI_TRY(reset_frame(e,frames[field],times[field],h->header.count7));
        }
    }
    return NBA97_ANIMATION_OK;
}
static int force(Nba97GameAnimationEffects* e,const Nba97GameAnimationAdvanceInput* source,unsigned channel,uint32_t request,uint32_t blend) {
    const Nba97GameAnimationClipView* h;uint32_t frame=0,flags;
    const unsigned lock=channel?0x4c:0x48,normalframe=channel?0x54:0x50;
    const unsigned forcedframe=channel?0x56:0x52,forcedtime=channel?0x5e:0x5a;
    AI_TRY(clip(source->resources,channel,request,&h));
    ai_assign(e,lock,request&65535);
    if(!(h->header.flags&1) && !channel && h->header.mode2==2) {
        /* Forced primary mode2 copies secondary FORCED state unconditionally,
         * without requiring matching secondary lock or clamping frame count. */
        copy_field(e,0x52,0x56);copy_field(e,0x5a,0x5e);
    } else {
        int retain=0;
        if(!(h->header.flags&1) && h->header.count7) {
            AI_READ(e,normalframe,frame);
            if(frame<h->header.count7) {AI_READ(e,channel?0x64:0x60,flags);retain=(flags&3)==0;}
        }
        if(retain) {ai_assign(e,forcedframe,frame);copy_field(e,forcedtime,channel?0x5c:0x58);}
        else {ai_assign(e,forcedtime,0);ai_assign(e,forcedframe,0);}
    }
    ai_assign(e,0x80,blend&65535);ai_assign(e,channel?0x78:0x70,65535);
    ai_assign(e,channel?0x66:0x62,h->header.flags);return NBA97_ANIMATION_OK;
}
static int add_time(Nba97GameAnimationEffects* e,const Nba97GameAnimationAdvanceInput* source,
                    unsigned mode,unsigned channel,int forced,int32_t* time) {
    uint32_t value;int32_t tick;
    if(mode==0 || (mode==3 && (forced || !channel))) {
        AI_TRY(global_half(source->tick_fdb6c,&tick));*time+=tick*256;
    } else if(mode==1 || (mode==2 && !forced && !channel)) {
        AI_READ(e,0x9c,value);*time+=(int32_t)value;
    } else if(mode==5) {
        AI_READ(e,0x9e,value);AI_TRY(global_half(source->tick_fdb6c,&tick));
        *time=(int32_t)((int64_t)*time+(int64_t)value*tick);
    }
    return NBA97_ANIMATION_OK;
}
static uint32_t increment_frame(uint32_t frame,uint8_t count,uint64_t steps) {
    if(!count)return 0;
    if(frame>=count)return (uint32_t)((steps-1)%count);
    return (uint32_t)((frame+steps)%count);
}
static int angular(Nba97GameAnimationEffects* e,const Nba97GameAnimationClipView* h,int32_t* time) {
    uint32_t turn,frame;uint64_t steps;const uint32_t step=h->step3,count=h->header.count7;
    AI_READ(e,0xea,turn);*time+=signed16(turn);
    if(!step) return *time?NBA97_ANIMATION_SOURCE_NONTERMINATING:NBA97_ANIMATION_OK;
    if(*time<-(int32_t)step) {
        AI_READ(e,0x54,frame);steps=(uint32_t)(-*time-1)/step;
        if(steps<=frame || !count)frame=(uint16_t)(frame-(uint32_t)steps);
        else frame=(uint32_t)(count-1-((steps-frame-1)%count));
        repeated(e,0x54,frame,steps);*time+=(int32_t)(steps*step);
    }
    if(*time>(int32_t)step) {
        AI_READ(e,0x54,frame);steps=(uint32_t)(*time-1)/step;
        repeated(e,0x54,increment_frame(frame,(uint8_t)count,steps),steps);*time-=(int32_t)(steps*step);
    }
    return NBA97_ANIMATION_OK;
}
static int forced_frames(Nba97GameAnimationEffects* e,const Nba97GameAnimationAdvanceInput* source,unsigned channel,uint32_t request) {
    const Nba97GameAnimationClipView* h;uint32_t value,other,blend;int32_t time;
    const unsigned frame=channel?0x56:0x52,timing=channel?0x5e:0x5a,counter=channel?0x96:0x94;
    AI_TRY(clip(source->resources,channel,request,&h));
    if(!channel && h->header.mode2==2) {
        AI_READ(e,0x4c,other);
        if(other==request) {copy_field(e,0x52,0x56);copy_field(e,0x5a,0x5e);goto blend;}
    }
    AI_READ(e,timing,value);time=signed16(value);
    AI_TRY(add_time(e,source,h->header.mode2,channel,1,&time));
    if(time>=(int32_t)h->step3*16) {
        uint64_t steps;
        if(!h->step3)return NBA97_ANIMATION_SOURCE_NONTERMINATING;
        steps=(uint32_t)time/((uint32_t)h->step3*16);
        value=0;if(h->header.count7) AI_READ(e,frame,value);
        repeated(e,frame,increment_frame(value,h->header.count7,steps),steps);
        time-=(int32_t)(steps*h->step3*16);
    }
    ai_assign(e,timing,(uint16_t)time);
blend:
    AI_READ(e,counter,value);AI_READ(e,0x80,blend);value=(value+blend)&65535;
    ai_assign(e,counter,value);
    if(value>=256) {
        Nba97GamePeriodValue status=ai_get(&e->state,0x9a);
        ai_assign(e,channel?0x4c:0x48,65535);ai_assign(e,channel?0x4a:0x46,request);
        copy_field(e,channel?0x54:0x50,frame);copy_field(e,channel?0x5c:0x58,timing);
        copy_field(e,channel?0x64:0x60,channel?0x66:0x62);
        status.word=((status.word>>2)&(channel?2:1))+(status.word&(channel?5:10));
        /* Original promotion discards ALL other status bits, not just blend bits. */
        ai_put(e,0x9a,status);
    }
    return NBA97_ANIMATION_OK;
}
static int normal_frames(Nba97GameAnimationEffects* e,const Nba97GameAnimationAdvanceInput* source,unsigned channel) {
    const Nba97GameAnimationClipView *h,*next;
    const unsigned current=channel?0x4a:0x46,frame=channel?0x54:0x50,timing=channel?0x5c:0x58;
    const unsigned flags=channel?0x64:0x60,queue=channel?0x78:0x70,aux=channel?0x6c:0x68;
    uint32_t request,other,value,f=0,queued,blend;int32_t time;uint64_t steps,before_wrap;
    AI_READ(e,current,request);AI_TRY(clip(source->resources,channel,request,&h));
    if(!channel && h->header.mode2==2) {
        AI_READ(e,0x4a,other);
        if(other==request) {copy_field(e,0x50,0x54);copy_field(e,0x58,0x5c);return NBA97_ANIMATION_OK;}
    }
    AI_READ(e,timing,value);time=signed16(value);
    if(channel && h->header.mode2==3) AI_TRY(angular(e,h,&time));
    else AI_TRY(add_time(e,source,h->header.mode2,channel,0,&time));
    if(time<(int32_t)h->step3*16) {ai_assign(e,timing,(uint16_t)time);return NBA97_ANIMATION_OK;}
    if(h->header.count7) AI_READ(e,frame,f);
    before_wrap=h->header.count7>f?h->header.count7-f:1;
    steps=h->step3?(uint32_t)time/((uint32_t)h->step3*16):before_wrap;
    if(steps<before_wrap) {
        repeated(e,frame,f+(uint32_t)steps,steps);
        ai_assign(e,timing,(uint16_t)(time-(int32_t)(steps*h->step3*16)));return NBA97_ANIMATION_OK;
    }
    AI_READ(e,queue,queued);
    if(queued>=32768) {
        AI_READ(e,flags,value);
        if(!(value&1)) {
            if(!h->step3)return NBA97_ANIMATION_SOURCE_NONTERMINATING;
            repeated(e,frame,increment_frame(f,h->header.count7,steps),steps);
            ai_assign(e,timing,(uint16_t)(time-(int32_t)(steps*h->step3*16)));return NBA97_ANIMATION_OK;
        }
        AI_READ(e,0x4e,request);
    } else request=queued;
    /* Advance only to the clip boundary; source discards leftover time after
     * a queued/default transition. Blend transitions leave normal time stale. */
    repeated(e,frame,f+(uint32_t)before_wrap-1,before_wrap-1);
    if(queued<32768) {
        unsigned i;AI_READ(e,aux,blend);
        for(i=0;i<3;++i) {copy_field(e,queue+i*2,queue+(i+1)*2);copy_field(e,aux+i,aux+i+1);}
        ai_assign(e,queue+6,65535); /* Last aux byte is deliberately not cleared. */
        if(blend) {
            ai_assign(e,channel?0x96:0x94,blend);ai_assign(e,0x80,blend);
            ai_assign(e,channel?0x4c:0x48,request);ai_assign(e,channel?0x5e:0x5a,0);ai_assign(e,channel?0x56:0x52,0);
            AI_TRY(clip(source->resources,channel,request,&next));
            ai_assign(e,channel?0x66:0x62,next->header.flags);return NBA97_ANIMATION_OK;
        }
    }
    ai_assign(e,current,request);AI_TRY(clip(source->resources,channel,request,&next));
    if(channel) {
        ai_assign(e,flags,next->header.flags);
        if(next->header.flags&0x100) {
            AI_READ(e,0x18,value);
            if(!value) {AI_READ(e,0xc4,value);ai_assign(e,0x18,value);}
        }
        ai_assign(e,frame,0);ai_assign(e,timing,0);
    } else {
        ai_assign(e,frame,0);ai_assign(e,flags,next->header.flags);
        /* Source checks OLD header mode2 after changing to the new clip. */
        if(h->header.mode2==2) {
            AI_READ(e,0x4a,other);
            if(other==request) {copy_field(e,0x50,0x54);copy_field(e,0x58,0x5c);return NBA97_ANIMATION_OK;}
        }
        ai_assign(e,timing,0);
    }
    return NBA97_ANIMATION_OK;
}
static int frames(Nba97GameAnimationEffects* e,const Nba97GameAnimationAdvanceInput* source) {
    uint32_t lock;
    AI_READ(e,0x4c,lock);
    if(lock<32768) AI_TRY(forced_frames(e,source,1,lock));else AI_TRY(normal_frames(e,source,1));
    AI_READ(e,0x48,lock);
    if(lock<32768) AI_TRY(forced_frames(e,source,0,lock));else AI_TRY(normal_frames(e,source,0));
    return NBA97_ANIMATION_OK;
}
static int advance(Nba97GameAnimationEffects* e,const Nba97GameAnimationAdvanceInput* source) {
    uint32_t value,other,clip_index,id;int32_t controlled;
    AI_READ(e,0x48,value);
    if(value>=32768) {AI_READ(e,0x4c,value);if(value>=32768) AI_TRY(remap(e,source));}
    AI_READ(e,0x10,value);
    if(!value) {
        AI_READ(e,0x2c,value);
        if(value) {
            AI_READ(e,0x1a,value);
            if(value!=20) {
                AI_READ(e,0x4a,value);ai_assign(e,0x16,0);ai_assign(e,0x14,0);ai_assign(e,0xe6,10);
                if(value<44) {AI_READ(e,0x4c,other);if(other>=32768) AI_TRY(force(e,source,1,38,40));}
                AI_READ(e,0x46,clip_index);
                if(clip_index<44) {
                    AI_TRY(global_half(source->controlled_fdbcc,&controlled));AI_READ(e,0,id);
                    value=id==(uint32_t)controlled;
                    if(!value) {AI_READ(e,0x60,other);value=(other&2)!=0;}
                    if(!value || clip_index==37) {
                        AI_READ(e,0x48,other);if(other>=32768) AI_TRY(force(e,source,0,38,40));
                    }
                }
            }
        }
    }
    return frames(e,source);
}
static int execute(Nba97GameAnimationEffects* out,const Nba97GameAnimationAdvanceInput* input,unsigned operation) {
    Nba97GameAnimationAdvanceInput source;Nba97GameAnimationEffects next;int result;unsigned ch,i;
    if(!out || !input)return NBA97_ANIMATION_ARGUMENT;
    memcpy(&source,input,sizeof(source));
    if(!ai_valid(&source.previous) || !ai_value_valid(source.tick_fdb6c,2) || !ai_value_valid(source.controlled_fdbcc,2))return NBA97_ANIMATION_ARGUMENT;
    if(source.resources) for(ch=0;ch<2;++ch)for(i=0;i<84;++i)
        if(source.resources->clip[ch][i].header.available>2)return NBA97_ANIMATION_ARGUMENT;
    memset(&next,0,sizeof(next));next.state=source.previous;
    result=operation==0?advance(&next,&source):operation==1?frames(&next,&source):remap(&next,&source);
    if(result==NBA97_ANIMATION_OK)memcpy(out,&next,sizeof(next));return result;
}
int nba97_game_animation_advance(Nba97GameAnimationEffects* out,const Nba97GameAnimationAdvanceInput* input) {return execute(out,input,0);}
int nba97_game_animation_advance_frames(Nba97GameAnimationEffects* out,const Nba97GameAnimationAdvanceInput* input) {return execute(out,input,1);}
int nba97_game_animation_remap(Nba97GameAnimationEffects* out,const Nba97GameAnimationAdvanceInput* input) {return execute(out,input,2);}
