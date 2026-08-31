#include "voice_mapping.h"
#include <string.h>

typedef struct Run {Nba97VoiceMapping* owner;Nba97VoiceMappingProgress* progress;} Run;
static int32_t s32(uint32_t v){return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)~v;}
static Nba97VoiceApiResult result(int status,uint32_t value){Nba97VoiceApiResult r;r.completion=status;r.value=s32(value);return r;}
static int step(Run* r,uint32_t address,int table){
    r->progress->stopped_address=address;r->progress->stopped_in_table=(uint8_t)table;
    if(r->progress->steps>=r->owner->step_budget)return NBA97_MAPPING_LIMIT;
    ++r->progress->steps;return 1;
}
static int read_word(Run* r,uint32_t at,uint32_t width,uint32_t* value){
    int rc=step(r,at,0);return rc==1?nba97_voice_patl_read(&r->owner->memory,at,width,value):rc;
}
static int write_word(Run* r,uint32_t at,uint32_t width,uint32_t value){
    int rc=step(r,at,0);return rc==1?nba97_voice_patl_write(&r->owner->memory,at,width,value):rc;
}
static int table_word(Run* r,Nba97VoiceMappingTable* table,uint32_t at,int write,uint32_t* value){
    int rc=step(r,at,1);if(rc!=1)return rc;
    return write?nba97_voice_mapping_table_write(table,at,*value):nba97_voice_mapping_table_read(table,at,value);
}
static int call(Run* r,enum Nba97VoiceMappingCall op,uint32_t a0,uint32_t a1,uint32_t* value){
    Nba97VoiceMappingEvent e;int rc=step(r,a0,0);if(rc!=1)return rc;
    e.call=op;e.a0=a0;e.a1=a1;
    if(!r->owner->call||r->owner->call(r->owner->context,&r->owner->memory,&e,value)!=1)return NBA97_PATL_IO_REFUSED;
    ++r->progress->callbacks_completed;return 1;
}
#define TRY(x) do{int rc=(x);if(rc!=1)return rc;}while(0)
#define READ(a,w,v) TRY(read_word(r,(a),(w),&(v)))
#define WRITE(a,w,v) TRY(write_word(r,(a),(w),(v)))
#define CALL(op,a,b,v) TRY(call(r,(op),(a),(b),&(v)))
#define TABLE(at,v) TRY(table_word(r,table,(at),0,&(v)))
static int table_store(Run* r,Nba97VoiceMappingTable* table,uint32_t at,uint32_t value){return table_word(r,table,at,1,&value);}

/*7E994(0), the only argument reached by70884. Both source stores execute.*/
static int transfer_mode(Run* r){WRITE(0x800c7624u,4,0);WRITE(0x800c75e0u,4,0);return 1;}
/*7E9C8 returns its incoming address even when lower7DDC8 returns something
 * else. Its two mismatch branches in70884 are unreachable under this owner.*/
static int transfer_address(Run* r,uint32_t address){
    uint32_t enabled,alignment,mask,shift,value=address;
    READ(0x800c75e8u,4,enabled);
    if(enabled){
        READ(0x800c75f0u,4,alignment);if(!alignment)return NBA97_MAPPING_TRAP;
        if(value%alignment){READ(0x800c75f4u,4,mask);value=(value+alignment)&~mask;}
    }
    READ(0x800c75ecu,4,shift);value>>=(shift&31u);
    WRITE(0x800c75c4u,2,value);return 1;
}
static int transfer(Run* r,uint32_t source,uint32_t size,uint32_t* written){
    uint32_t v;READ(0x800f9600u,4,v);WRITE(0x800f9600u,4,v+1u);
    if(size>0x7f000u)size=0x7f000u;
    CALL(NBA97_MAPPING_TRANSFER_7DC90,source,size,v);
    /*7EA04 ignores the lower transfer's return and reports the clamped size.
     * Callback refusal is different: it may not masquerade as this success.*/
    READ(0x800c75fcu,4,v);if(!v)WRITE(0x800c75f8u,4,0);
    *written=size;return 1;
}
/*7E898(1): wait for the actual SDK event, preserving initial shortcuts and
 * its source pending-word update. The step budget bounds original waits.*/
static int transfer_completed(Run* r,uint32_t* completed){
    uint32_t v,event;READ(0x800c7624u,4,v);if(v==1){*completed=1;return 1;}
    READ(0x800c75f8u,4,v);if(v==1){*completed=1;return 1;}
    do{READ(0x800c7678u,4,event);CALL(NBA97_MAPPING_TEST_EVENT_7F568,event,0,v);}while(!v);
    WRITE(0x800c75f8u,4,1);*completed=1;return 1;
}
static int upload(Run* r,uint32_t mapping,uint32_t body,Nba97VoiceMappingTable* table,uint32_t* answer){
    uint32_t index=0,word,offset,value,address,size,written,done,unused=0;
    if(!mapping){*answer=0xfffffff8u;return 1;}
    WRITE(0x800c6d2du,1,1);
    for(;;){READ(0x800c6d2cu,1,value);if(!value)break;CALL(NBA97_MAPPING_WAIT_CHANNEL,0,0,unused);}
    if(body){
        for(;;){
            TABLE(index*12u,word);if(word==0xffffffffu)break;
            /* Source rereads this word before EACH comparison. Aliased
             * mapping writes can change the table before the next record. */
            TABLE(index*12u,word);READ(mapping+0x1cu,4,offset);
            if(word==offset){TABLE(index*12u+4u,value);WRITE(mapping+0x2cu,4,value);WRITE(mapping+0x1cu,4,0xffffffffu);}
            else{TABLE(index*12u,word);READ(mapping+0x20u,4,offset);
                if(word==offset){TABLE(index*12u+4u,value);WRITE(mapping+0x30u,4,value);WRITE(mapping+0x20u,4,0xffffffffu);}}
            ++index;
        }
        READ(mapping+0x2cu,4,value);
        if(value==0xffffffffu){
            READ(mapping+0x24u,4,size);CALL(NBA97_MAPPING_ALLOCATE_7EC2C,size,0,address);
            if(address==0xffffffffu)goto failed;
            TRY(transfer_mode(r));TRY(transfer_address(r,address));
            READ(mapping+0x1cu,4,offset);READ(mapping+0x24u,4,size);
            TRY(transfer(r,body+offset,size,&written));TRY(transfer_completed(r,&done));
            if(!done)goto failed;READ(mapping+0x24u,4,size);if(written!=size)goto failed;
            WRITE(mapping+0x2cu,4,address);READ(mapping+0x1cu,4,offset);
            TRY(table_store(r,table,index*12u,offset));TRY(table_store(r,table,index*12u+4u,address));++index;
            /* Original overwrites the terminator and DOES NOT write a new
             * one at the next record. Incoming table bytes remain live. */
        }
        READ(mapping+0x30u,4,value);
        if(value==0xffffffffu){
            READ(mapping+6u,1,value);
            if(value==2){
                READ(mapping+0x28u,4,size);CALL(NBA97_MAPPING_ALLOCATE_7EC2C,size,0,address);
                if(address==0xffffffffu)goto failed;
                TRY(transfer_mode(r));TRY(transfer_address(r,address));
                READ(mapping+0x20u,4,offset);READ(mapping+0x28u,4,size);
                TRY(transfer(r,body+offset,size,&written));TRY(transfer_completed(r,&done));
                if(!done)goto failed;
                /* Confirmed original bug70CAC: compares the right transfer
                 * with LEFT+24 size, although the requested size was+28. */
                READ(mapping+0x24u,4,size);if(written!=size)goto failed;
                WRITE(mapping+0x30u,4,address);READ(mapping+0x20u,4,offset);
                TRY(table_store(r,table,index*12u,offset));TRY(table_store(r,table,index*12u+4u,address));
            }
        }
    }else{
        READ(0x800e45e4u,1,value);
        if(value==255u){
            READ(mapping+6u,1,value);WRITE(0x800e45e9u,1,value);
            CALL(NBA97_MAPPING_STREAM_RESET_7390C,0,0,unused);
            READ(0x800e460cu,4,value);WRITE(mapping+0x2cu,4,value);
            READ(0x800e45e9u,1,value);
            if(value==2){READ(0x800e460cu,4,value);READ(0x800e4624u,4,offset);WRITE(mapping+0x30u,4,value+offset);}
            CALL(NBA97_MAPPING_STREAM_PRIME_73580,0,0,unused);
        }
    }
    WRITE(0x800c6d2du,1,0);*answer=0;return 1;
failed:
    /* No source free, rollback, table repair, or restored offsets. */
    WRITE(0x800c6d2du,1,0);*answer=0xffffffffu;return 1;
}
static int unload(Run* r,uint32_t mapping,uint32_t* answer){
    uint32_t value,address,unused=0;READ(mapping+0x1cu,4,value);
    if(value!=0xffffffffu){READ(mapping+0x2cu,4,address);CALL(NBA97_MAPPING_FREE_7E56C,address,0,unused);}
    READ(mapping+0x20u,4,value);
    if(value!=0xffffffffu){READ(mapping+6u,1,value);if(value==2){READ(mapping+0x30u,4,address);CALL(NBA97_MAPPING_FREE_7E56C,address,0,unused);}}
    /* Original ignores free results and never clears address/offset fields.
     * Offsets marked FFFFFFFF by deduplication intentionally skip freeing. */
    *answer=0;return 1;
}
Nba97VoiceApiResult nba97_voice_mapping_upload(Nba97VoiceMapping* owner,uint32_t mapping,uint32_t body,Nba97VoiceMappingTable* table,Nba97VoiceMappingProgress* progress){
    Run r;uint32_t value=0;int status;if(!owner||!progress)return result(NBA97_PATL_ARGUMENT,0);
    memset(progress,0,sizeof *progress);r.owner=owner;r.progress=progress;status=upload(&r,mapping,body,table,&value);return result(status,value);
}
Nba97VoiceApiResult nba97_voice_mapping_unload(Nba97VoiceMapping* owner,uint32_t mapping,Nba97VoiceMappingProgress* progress){
    Run r;uint32_t value=0;int status;if(!owner||!progress)return result(NBA97_PATL_ARGUMENT,0);
    memset(progress,0,sizeof *progress);r.owner=owner;r.progress=progress;status=unload(&r,mapping,&value);return result(status,value);
}
