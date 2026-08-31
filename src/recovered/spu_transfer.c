#include "spu_transfer.h"
#include <string.h>

typedef struct Run {
    Nba97SpuTransfer* owner;
    Nba97SpuTransferProgress* out;
    Nba97SpuTransferEvent* journal;
    size_t capacity;
} Run;
static int32_t s32(uint32_t v) { return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)~v; }
static Nba97SpuTransferValue known(uint32_t word) {
    Nba97SpuTransferValue value;value.word=word;value.known=1;return value;
}
static int access(Run* r,uint32_t pc,uint32_t at) {
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->accesses>=r->owner->access_budget)return NBA97_SPU_TRANSFER_LIMIT;
    ++r->out->accesses;return 1;
}
static int read_ram(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t* value) {
    int rc=access(r,pc,at);
    return rc==1?nba97_voice_patl_read(&r->owner->memory,at,width,value):rc;
}
static int write_ram(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t value) {
    int rc;Nba97SpuTransferEvent* e;
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->events>=r->capacity)return NBA97_SPU_TRANSFER_LIMIT;
    rc=access(r,pc,at);if(rc!=1)return rc;
    rc=nba97_voice_patl_write(&r->owner->memory,at,width,value);if(rc!=1)return rc;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=NBA97_SPU_TRANSFER_RAM_STORE;e->pc=pc;e->address=at;e->width=width;
    e->value=width==2?value&0xffffu:value;e->completed=1;++r->out->stores;return 1;
}
static int external(Run* r,enum Nba97SpuTransferEventKind kind,uint32_t pc,
    uint32_t address,uint32_t width,uint32_t value,uint32_t a0,uint32_t a1,Nba97SpuTransferValue* result) {
    Nba97SpuTransferEvent* e;Nba97SpuTransferValue returned={0,0};int rc;
    r->out->stopped_pc=pc;r->out->stopped_address=address;
    if(r->out->events>=r->capacity)return NBA97_SPU_TRANSFER_LIMIT;
    rc=access(r,pc,address);if(rc!=1)return rc;
    if(width&&(address&(width-1u)))return NBA97_PATL_RESOURCE;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=kind;e->pc=pc;e->address=address;e->width=width;e->value=width==2?value&0xffffu:value;
    e->argument[0]=a0;e->argument[1]=a1;
    if(!r->owner->io||r->owner->io(r->owner->user,&r->owner->memory,e,&returned)!=1)return NBA97_PATL_IO_REFUSED;
    e->returned=returned;e->completed=1;++r->out->callbacks_completed;
    if(returned.known>1)return NBA97_PATL_METADATA;
    *result=returned;return 1;
}
static int read_device(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t* value) {
    Nba97SpuTransferValue result;int rc=external(r,NBA97_SPU_TRANSFER_DEVICE_READ,pc,at,width,0,0,0,&result);
    if(rc!=1)return rc;if(!result.known)return NBA97_PATL_RESOURCE;
    *value=width==2?result.word&0xffffu:result.word;return 1;
}
static int write_device(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t value) {
    Nba97SpuTransferValue unused;
    return external(r,NBA97_SPU_TRANSFER_DEVICE_WRITE,pc,at,width,value,0,0,&unused);
}
#define TRY(expr) do { int rc_=(expr);if(rc_!=1)return rc_; } while(0)
#define READ(pc,at,w,v) TRY(read_ram(r,(pc),(at),(w),&(v)))
#define WRITE(pc,at,w,v) TRY(write_ram(r,(pc),(at),(w),(v)))
#define DEVICE_READ(pc,at,w,v) TRY(read_device(r,(pc),(at),(w),&(v)))
#define DEVICE_WRITE(pc,at,w,v) TRY(write_device(r,(pc),(at),(w),(v)))

static int control(Run* r,uint32_t command,uint32_t a1,uint32_t a2,Nba97SpuTransferValue* returned) {
    uint32_t shift,base,value,desired,count,pointer,direction,source,blocks;
    *returned=known(0);
    if(command==2) {
        READ(0x8007da48u,0x800c75ecu,4,shift);READ(0x8007da50u,0x800c75c8u,4,base);
        value=a1>>(shift&31u);WRITE(0x8007da5cu,0x800c75c4u,2,value);
        DEVICE_WRITE(0x8007da60u,base+0x1a6u,2,value);return 1;
    }
    if(command==1||command==0) {
        const int reading=command==0;
        READ(reading?0x8007dae0u:0x8007da70u,0x800c75c8u,4,base);
        READ(reading?0x8007dae8u:0x8007da78u,0x800c75c4u,2,desired);
        DEVICE_READ(reading?0x8007daecu:0x8007da7cu,base+0x1a6u,2,value);
        WRITE(reading?0x8007daf4u:0x8007da84u,0x800c7614u,4,reading?1u:0u);
        if(value!=desired) {
            count=1;
            for(;;) {
                if(count>=0xf01u) { *returned=known(0xfffffffeu);return 1; }
                DEVICE_READ(reading?0x8007db14u:0x8007daa4u,base+0x1a6u,2,value);
                ++count;if(value==desired)break;
            }
        }
        READ(reading?0x8007db28u:0x8007dab8u,0x800c75c8u,4,base);
        DEVICE_READ(reading?0x8007db30u:0x8007dac0u,base+0x1aau,2,value);
        value=reading?value|0x30u:(value&0xffcfu)|0x20u;
        DEVICE_WRITE(reading?0x8007db3cu:0x8007dad0u,base+0x1aau,2,value);return 1;
    }
    if(command!=3)return 1;
    READ(0x8007db4cu,0x800c7614u,4,direction);desired=direction==1?0x30u:0x20u;
    READ(0x8007db64u,0x800c75c8u,4,base);
    DEVICE_READ(0x8007db6cu,base+0x1aau,2,value);count=1;
    while((value&0x30u)!=desired) {
        if(count>=0xf01u) { *returned=known(0xfffffffeu);return 1; }
        DEVICE_READ(0x8007db8cu,base+0x1aau,2,value);++count;
    }
    READ(0x8007dba4u,0x800c7614u,4,direction);
    READ(direction==1?0x8007dbb8u:0x8007dbd8u,0x800c75dcu,4,pointer);
    DEVICE_READ(direction==1?0x8007dbc0u:0x8007dbe0u,pointer,4,value);
    value=(value&0xf0ffffffu)|(direction==1?0x22000000u:0x20000000u);
    DEVICE_WRITE(0x8007dbf4u,pointer,4,value);
    READ(0x8007dc04u,0x800c75ccu,4,pointer);WRITE(0x8007dc0cu,0x800c7618u,4,a1);
    /* ceil(byte_count/64) is stored before shifting into the DMA block
     * register. The shift truncates high bits; do not replace it with a
     * clamped logical byte count or silently drop rounded source bytes. */
    blocks=(a2>>6)+(uint32_t)((a2&63u)!=0);
    READ(0x8007dc28u,0x800c7618u,4,source);WRITE(0x8007dc34u,0x800c761cu,4,blocks);
    DEVICE_WRITE(0x8007dc38u,pointer,4,source);
    READ(0x8007dc40u,0x800c761cu,4,blocks);READ(0x8007dc48u,0x800c75d0u,4,pointer);
    DEVICE_WRITE(0x8007dc54u,pointer,4,(blocks<<16)|0x10u);
    READ(0x8007dc5cu,0x800c7614u,4,direction);READ(0x8007dc78u,0x800c75d4u,4,pointer);
    DEVICE_WRITE(0x8007dc80u,pointer,4,direction==1?0x01000200u:0x01000201u);return 1;
}
static int diagnostic(Run* r,uint32_t pc,uint32_t message,Nba97SpuTransferValue* returned) {
    return external(r,NBA97_SPU_TRANSFER_DIAGNOSTIC_83B20,pc,0x80027dd0u,0,0,0x80027dd0u,message,returned);
}
static int pio(Run* r,uint32_t source,uint32_t remaining,Nba97SpuTransferValue* returned) {
    uint32_t base,address,value,initial,chunk,offset,count;
    Nba97SpuTransferValue unused;
    READ(0x8007d338u,0x800c75c8u,4,base);READ(0x8007d340u,0x800c75c4u,2,address);
    DEVICE_READ(0x8007d364u,base+0x1aeu,2,initial);
    DEVICE_WRITE(0x8007d36cu,base+0x1a6u,2,address);initial&=0x7ffu;
    /* Four source delay loops only modify private stack words. They do not
     * establish native elapsed time or authorize a made-up device result. */
    while(remaining) {
        chunk=remaining<65u?remaining:64u;offset=0;
        READ(0x8007d3f0u,0x800c75c8u,4,base);
        do {
            /* Original LHU reads the extra byte for an odd final count. */
            READ(0x8007d3f4u,source,2,value);source+=2u;offset+=2u;
            DEVICE_WRITE(0x8007d400u,base+0x1a8u,2,value);
        } while(s32(offset)<s32(chunk));
        READ(0x8007d414u,0x800c75c8u,4,base);
        DEVICE_READ(0x8007d41cu,base+0x1aau,2,value);
        DEVICE_WRITE(0x8007d42cu,base+0x1aau,2,(value&0xffcfu)|0x10u);
        READ(0x8007d480u,0x800c75c8u,4,base);
        DEVICE_READ(0x8007d488u,base+0x1aeu,2,value);WRITE(0x8007d490u,0x800c75c0u,4,0);
        while(value&0x400u) {
            READ(0x8007d4a4u,0x800c75c0u,4,count);++count;WRITE(0x8007d4b4u,0x800c75c0u,4,count);
            if(s32(count)>=0x1389) {
                /* The source prints and CONTINUES after this timeout. */
                TRY(diagnostic(r,0x8007d4d4u,0x80027df0u,&unused));break;
            }
            READ(0x8007d4e8u,0x800c75c8u,4,base);
            DEVICE_READ(0x8007d4f0u,base+0x1aeu,2,value);
        }
        remaining-=chunk;
    }
    READ(0x8007d5acu,0x800c75c8u,4,base);
    DEVICE_READ(0x8007d5b4u,base+0x1aau,2,value);
    DEVICE_WRITE(0x8007d5c0u,base+0x1aau,2,value&0xffcfu);
    DEVICE_READ(0x8007d5c4u,base+0x1aeu,2,value);WRITE(0x8007d5d0u,0x800c75c0u,4,0);
    while((value&0x7ffu)!=initial) {
        READ(0x8007d5e4u,0x800c75c0u,4,count);++count;WRITE(0x8007d5f4u,0x800c75c0u,4,count);
        if(s32(count)>=0x1389)return diagnostic(r,0x8007d614u,0x80027e04u,returned);
        READ(0x8007d628u,0x800c75c8u,4,base);
        DEVICE_READ(0x8007d630u,base+0x1aeu,2,value);
    }
    *returned=known(value&0x7ffu);return 1;
}
static int deliver(Run* r,uint32_t event_class,uint32_t spec,Nba97SpuTransferValue* returned) {
    return external(r,NBA97_SPU_TRANSFER_DELIVER_EVENT_B0_07,0x8007f50cu,event_class,
        0,0,event_class,spec,returned);
}
static int interrupt_complete(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t direction,base,value,count,callback;
    READ(0x8007d66cu,0x800c7614u,4,direction);
    /* Direction zero executes three private stack delay loops. Like PIO's
     * delays, these do not supply a portable elapsed-time/device model. */
    (void)direction;
    READ(0x8007d768u,0x800c75c8u,4,base);
    DEVICE_READ(0x8007d770u,base+0x1aau,2,value);
    DEVICE_WRITE(0x8007d77cu,base+0x1aau,2,value&0xffcfu);
    DEVICE_READ(0x8007d780u,base+0x1aau,2,value);count=1;
    while(value&0x30u) {
        if(count>=0xf01u)break;
        DEVICE_READ(0x8007d7a4u,base+0x1aau,2,value);++count;
    }
    /* Original timeout still reaches completion. A custom callback replaces
     * default event delivery; neither branch means that DMA start was enough. */
    READ(0x8007d7c0u,0x800c75fcu,4,callback);
    if(callback) {
        READ(0x8007d7d4u,0x800c75fcu,4,callback);
        return external(r,NBA97_SPU_TRANSFER_CALLBACK_7D668,0x8007d7dcu,callback,
            0,0,0xf0000000u,0,returned);
    }
    return deliver(r,0xf0000009u,0x20u,returned);
}
static int transfer(Run* r,uint32_t source,uint32_t size,Nba97SpuTransferValue* returned) {
    uint32_t mode,address,shift;Nba97SpuTransferValue unused;
    READ(0x8007dc94u,0x800c75e0u,4,mode);
    if(!mode) {
        READ(0x8007dcb8u,0x800c75c4u,2,address);READ(0x8007dcc0u,0x800c75ecu,4,shift);
        TRY(control(r,2,address<<(shift&31u),0,&unused));
        TRY(control(r,1,0,0,&unused));TRY(control(r,3,source,size,&unused));
    } else TRY(pio(r,source,size,&unused));
    /* All three command returns (including -2) and the PIO return are
     * ignored. A native refusal still stops; it is not a source timeout. */
    *returned=known(size);return 1;
}
int nba97_spu_transfer(Nba97SpuTransfer* owner,enum Nba97SpuTransferOperation operation,
    uint32_t a0,uint32_t a1,uint32_t a2,Nba97SpuTransferEvent* journal,size_t capacity,
    Nba97SpuTransferProgress* out) {
    Run run;int rc;Nba97SpuTransferValue returned={0,0};
    if(!owner||!out||(!journal&&capacity)||(!owner->memory.spans&&owner->memory.count)||
        operation<NBA97_SPU_TRANSFER_7DC90||operation>NBA97_SPU_DELIVER_EVENT_7F508)return NBA97_PATL_ARGUMENT;
    memset(out,0,sizeof *out);run.owner=owner;run.out=out;run.journal=journal;run.capacity=capacity;
    switch(operation) {
    case NBA97_SPU_TRANSFER_7DC90:rc=transfer(&run,a0,a1,&returned);break;
    case NBA97_SPU_CONTROL_7D9E8:rc=control(&run,a0,a1,a2,&returned);break;
    case NBA97_SPU_PIO_7D334:rc=pio(&run,a0,a1,&returned);break;
    case NBA97_SPU_ISR_7D668:rc=interrupt_complete(&run,&returned);break;
    case NBA97_SPU_DELIVER_EVENT_7F508:rc=deliver(&run,a0,a1,&returned);break;
    default:rc=external(&run,NBA97_SPU_TRANSFER_TEST_EVENT_B0_0B,0x8007f56cu,a0,0,0,a0,0,&returned);break;
    }
    if(rc==1) { out->returned=returned;out->completed=1;out->stopped_pc=0;out->stopped_address=0; }
    return rc;
}
