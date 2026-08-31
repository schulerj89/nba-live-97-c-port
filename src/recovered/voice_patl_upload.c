#include "voice_patl_upload.h"

static int32_t s32(uint32_t u){return u<=0x7fffffffu?(int32_t)u:-1-(int32_t)~u;}
static Nba97VoiceApiResult result(int c,int32_t v){Nba97VoiceApiResult r;r.completion=c;r.value=v;return r;}
static int resolve(const Nba97VoicePatlMemory* m,uint32_t address,uint32_t width,size_t* offset,const Nba97VoicePatlSpan** output){
    const Nba97VoicePatlSpan* found=0;size_t i;
    if(!m||(!m->spans&&m->count)||(width!=1&&width!=2&&width!=4)||(address&(width-1)))return NBA97_PATL_RESOURCE;
    for(i=0;i<m->count;++i){
        const Nba97VoicePatlSpan* s=&m->spans[i];size_t at,j;
        if(!s->source_address_known||!s->data||
            (uint64_t)s->size>UINT64_C(0x100000000)-(uint64_t)s->source_address||address<s->source_address)continue;
        at=(size_t)(address-s->source_address);
        if(at>s->size||width>s->size-at)continue;
        /* Validate the entire requested width before testing individual read
         * knownness or modifying destination bytes. An unknown byte cannot
         * hide a later malformed marker in this access. Unvisited allocation
         * bytes are not interpreted; callbacks can change masks between calls. */
        if(s->source_address_known>1||s->writable>1||s->fully_known>1)return NBA97_PATL_METADATA;
        if(s->known){for(j=at;j<at+width;++j){
            if(s->known[j]>1||(s->fully_known&&s->known[j]!=1))return NBA97_PATL_METADATA;
        }}
        if(found)return NBA97_PATL_RESOURCE; /* No guessed winner for conflicting provenance. */
        found=s;*offset=at;
    }
    if(!found)return NBA97_PATL_RESOURCE;
    *output=found;return NBA97_PATL_COMPLETE;
}
int nba97_voice_patl_read(const Nba97VoicePatlMemory* m,uint32_t address,uint32_t width,uint32_t* value){
    size_t offset=0;uint32_t i,v=0;const Nba97VoicePatlSpan* s;int status;
    if(!value)return NBA97_PATL_ARGUMENT;
    status=resolve(m,address,width,&offset,&s);if(status!=1)return status;
    if(!s->fully_known){for(i=0;i<width;++i)if(!s->known||!s->known[offset+i])return NBA97_PATL_RESOURCE;}
    for(i=0;i<width;++i)v|=(uint32_t)s->data[offset+i]<<(8u*i);
    *value=v;return NBA97_PATL_COMPLETE;
}
int nba97_voice_patl_write(const Nba97VoicePatlMemory* m,uint32_t address,uint32_t width,uint32_t value){
    size_t offset=0;uint32_t i;const Nba97VoicePatlSpan* s;int status=resolve(m,address,width,&offset,&s);
    if(status!=1)return status;
    if(!s->writable||(!s->fully_known&&!s->known))return NBA97_PATL_RESOURCE;
    /* Do not inspect prior destination bytes: an actual source store makes
     * them known even if their previous contents were unavailable. */
    for(i=0;i<width;++i){s->data[offset+i]=(uint8_t)(value>>(8u*i));if(s->known)s->known[offset+i]=1;}
    return NBA97_PATL_COMPLETE;
}
static int valid(const Nba97VoicePatlUpload* s){return s&&s->call&&s->memory.spans&&s->memory.count;}
#define READ(address,width,target) do{int status=nba97_voice_patl_read(&s->memory,(address),(width),&(target));if(status!=1)return result(status,0);}while(0)
#define WRITE(address,width,value) do{int status=nba97_voice_patl_write(&s->memory,(address),(width),(value));if(status!=1)return result(status,0);}while(0)
Nba97VoiceApiResult nba97_voice_patl_upload(Nba97VoicePatlUpload* s,uint32_t header,uint32_t body,uint32_t* auxiliary){
    uint32_t loaded,count,pointer,index=0,offset=0,tone,value,last=0,cleanup=0xffffffffu,field;
    if(!valid(s)||!auxiliary)return result(NBA97_PATL_ARGUMENT,0);
    READ(header+5,1,loaded);if(loaded)return result(NBA97_PATL_COMPLETE,-1);
    READ(header+12,4,pointer);READ(header+7,1,count);
    /* Even zero tones relocate the program pointer and mark loaded. */
    WRITE(header+12,4,header+pointer+12);
    if(count){
        do{
            READ(header+12,4,pointer);tone=pointer+offset;
            for(field=24;field<=32;field+=4){
                READ(tone+field,4,value);
                if(value)WRITE(tone+field,4,tone+field+value);
            }
            /* Envelope+36 relocates unconditionally, including relative0. */
            READ(tone+36,4,value);WRITE(tone+36,4,tone+36+value);
            if(s->call(s->context,&s->memory,NBA97_PATL_UPLOAD_MAPPING_70884,tone+40,body,auxiliary,&last)!=1)
                return result(NBA97_PATL_IO_REFUSED,0);
            if(s32(last)<0){cleanup=index-1u;break;}
            READ(header+7,1,count);++index;offset+=92;
        }while(index<count);
    }
    if(s32(cleanup)>=0){
        index=0;offset=0;
        do{
            READ(header+12,4,pointer);++index;
            if(s->call(s->context,&s->memory,NBA97_PATL_UNLOAD_MAPPING_714B8,pointer+offset+40,0,0,&value)!=1)
                return result(NBA97_PATL_IO_REFUSED,0);
            offset+=92;
        }while(index<=cleanup);
        /* Original ignores cleanup return values and keeps relocations. */
        return result(NBA97_PATL_COMPLETE,s32(last));
    }
    /* Original bug: first-tone failure makes cleanup=-1, so it reaches this
     * same loaded=1 store as success, while retaining the negative return.
     * Do not clear/undo relocated pointers or modernize this to atomic load. */
    WRITE(header+5,1,1);return result(NBA97_PATL_COMPLETE,s32(last));
}
Nba97VoiceApiResult nba97_voice_patl_unload(Nba97VoicePatlUpload* s,uint32_t header){
    uint32_t count,index=0,offset=0,pointer,value;
    if(!valid(s))return result(NBA97_PATL_ARGUMENT,0);
    READ(header+7,1,count);
    while(index<count){
        READ(header+12,4,pointer);
        if(s->call(s->context,&s->memory,NBA97_PATL_UNLOAD_MAPPING_714B8,pointer+offset+40,0,0,&value)!=1)
            return result(NBA97_PATL_IO_REFUSED,0);
        READ(header+7,1,count);++index;offset+=92;
    }
    /* 91AB4 ignores unload returns and never clears loaded/relocated fields. */
    return result(NBA97_PATL_COMPLETE,0);
}
