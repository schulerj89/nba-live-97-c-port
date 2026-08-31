#include "voice_allocation.h"

static int32_t signed32(uint32_t u){return u<=0x7fffffffu?(int32_t)u:-1-(int32_t)~u;}
static int32_t signed8(uint8_t u){return u<128?(int32_t)u:(int32_t)u-256;}
static Nba97VoiceApiResult result(int completion,int32_t value){Nba97VoiceApiResult r;r.completion=completion;r.value=value;return r;}
static int valid(const Nba97VoiceAllocation* s){
    return s&&s->shared&&s->records&&s->generation&&s->membership_count&&s->selected;
}
static int scratch(uint32_t index){return index<8;}
/* Complete912E8: signed count and signed selected bytes. The count is read
 * once per call, independently of requested_count in91338. Original never
 * synchronizes it here; a short/zero count can permit duplicate selection. */
static Nba97VoiceApiResult contains(Nba97VoiceAllocation* s,uint32_t index){
    uint32_t i=0,count=*s->membership_count;
    while(signed32(i)<signed32(count)){
        if(!scratch(i))return result(NBA97_ALLOCATION_UNOWNED_SCRATCH,0);
        if(signed8(s->selected[i])==(int32_t)index)return result(NBA97_ALLOCATION_COMPLETE,1);
        ++i;
    }
    return result(NBA97_ALLOCATION_COMPLETE,0);
}
static Nba97VoiceApiResult finish(Nba97VoiceAllocation* s,int32_t value){
    Nba97VoiceApiResult r=nba97_voice_handle_unlock(s->shared);r.value=value;return r;
}
Nba97VoiceApiResult nba97_voice_allocate(Nba97VoiceAllocation* s,uint32_t mask,
    uint32_t requested,uint32_t priority,uint32_t* output_handle){
    uint32_t i,found=0,attempt,least,best,age,first;
    int32_t answer=-9;Nba97VoiceApiResult r;Nba97MusicVoice* voice;
    if(!valid(s)||!output_handle)return result(NBA97_ALLOCATION_ARGUMENT,0);
    r=nba97_voice_handle_lock(s->shared);if(r.completion!=1)return r;
    for(i=0;signed32(i)<signed32(requested);++i){
        if(!scratch(i))return result(NBA97_ALLOCATION_UNOWNED_SCRATCH,0);
        s->selected[i]=255;
    }
    /* Original generation advances even on failed allocation. Signed-negative
     * wrap resets the whole word to0; it is not WinMM's native64-bit ID. */
    *s->generation+=32u;
    if(*s->generation&0x80000000u)*s->generation=0;
    for(i=0;i<24;++i){
        if((mask&(1u<<i))&&s->shared->voices[i].active==0){
            r=contains(s,i);if(r.completion!=1)return r;
            if(!r.value){
                if(!scratch(found))return result(NBA97_ALLOCATION_UNOWNED_SCRATCH,0);
                s->selected[found++]=(uint8_t)i;
                if(signed32(found)>=signed32(requested))break;
            }
        }
    }
    if(signed32(found)<signed32(requested)){
        attempt=found;
        do{
            least=102;best=0;age=0x7fffffffu;
            for(i=0;i<24;++i){
                if(mask&(1u<<i)){
                    r=contains(s,i);if(r.completion!=1)return r;
                    if(!r.value){
                        if(s->records[i].priority<least){
                            least=s->records[i].priority;age=s->records[i].age;best=i;
                        }else if(s->records[i].priority==least&&signed32(s->records[i].age)<signed32(age)){
                            age=s->records[i].age;best=i;
                        }
                    }
                }
            }
            /* Original fallback remains voice0/priority102 if no candidate
             * improved the sentinel. A large request priority can select it
             * even outside mask; do not "fix" this source behavior. */
            if(signed32(priority)>=signed32(least)){
                if(!scratch(found))return result(NBA97_ALLOCATION_UNOWNED_SCRATCH,0);
                s->selected[found++]=(uint8_t)best;
                if(signed32(found)>=signed32(requested))break;
            }
            ++attempt;
        }while(signed32(attempt)<signed32(requested));
    }
    if(found!=requested)return finish(s,answer);
    /* Original zero-count quirk: if no free candidate was added, stale
     * selected[0] still publishes/replaces a handle; no active transition is
     * made because the activation loop has zero iterations. */
    first=(uint32_t)signed8(s->selected[0]);
    *output_handle=*s->generation|first; /* Published BEFORE stop/status. */
    if(first>=24)return result(NBA97_ALLOCATION_UNOWNED_SLOT,0);
    s->shared->voices[first].handle=*s->generation|first;
    answer=(int32_t)first;
    for(i=0;i<found;++i){
        uint32_t index=(uint32_t)signed8(s->selected[i]);
        if(index>=24)return result(NBA97_ALLOCATION_UNOWNED_SLOT,0);
        voice=&s->shared->voices[index];
        if(voice->active==1){
            r=nba97_voice_handle_stop(s->shared,voice->handle);if(r.completion!=1)return r;
        }
        r=nba97_voice_handle_status(s->shared,voice->handle);if(r.completion!=1)return r;
        /* A queued keyoff does not retire an active voice under the outer
         * lock. Original failure leaves the new handle and masks written. */
        if(r.value!=1)return finish(s,-9);
        voice->active=1;s->records[index].age=s->shared->clock->services;
        s->records[index].priority=(uint8_t)priority;
    }
    for(i=1;i<found;++i){
        first=(uint32_t)signed8(s->selected[0]);
        if(first>=24)return result(NBA97_ALLOCATION_UNOWNED_SLOT,0);
        s->records[first].linked[i-1]=s->selected[i];
        first=(uint32_t)signed8(s->selected[i]);
        if(first>=24)return result(NBA97_ALLOCATION_UNOWNED_SLOT,0);
        s->shared->voices[first].handle=0xffffffffu;
    }
    return finish(s,answer);
}
