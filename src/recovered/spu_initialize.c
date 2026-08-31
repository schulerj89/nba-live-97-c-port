#include "spu_initialize.h"
#include <string.h>

typedef struct Run {
    Nba97SpuInitialize* owner;
    Nba97SpuInitializeProgress* out;
    Nba97SpuInitializeEvent* journal;
    size_t capacity;
} Run;
static int32_t s32(uint32_t v) { return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)~v; }
static Nba97SpuTransferValue known(uint32_t word) {
    Nba97SpuTransferValue value;value.word=word;value.known=1;return value;
}
static int access(Run* r,uint32_t pc,uint32_t at) {
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->accesses>=r->owner->access_budget)return NBA97_SPU_INITIALIZE_LIMIT;
    ++r->out->accesses;return 1;
}
static int read_ram(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t* value) {
    int rc=access(r,pc,at);
    return rc==1?nba97_voice_patl_read(&r->owner->memory,at,width,value):rc;
}
static int write_ram(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t value) {
    int rc;Nba97SpuInitializeEvent* e;
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->events>=r->capacity)return NBA97_SPU_INITIALIZE_LIMIT;
    rc=access(r,pc,at);if(rc!=1)return rc;
    rc=nba97_voice_patl_write(&r->owner->memory,at,width,value);if(rc!=1)return rc;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=NBA97_SPU_INITIALIZE_RAM_STORE;e->pc=pc;e->address=at;e->width=width;
    e->value=width==2?value&0xffffu:value;e->completed=1;++r->out->stores;return 1;
}
static int external(Run* r,enum Nba97SpuInitializeKind kind,uint32_t pc,
    uint32_t address,uint32_t width,uint32_t value,uint32_t a0,uint32_t a1,Nba97SpuTransferValue* result) {
    Nba97SpuInitializeEvent* e;Nba97SpuTransferValue returned={0,0};int rc;
    r->out->stopped_pc=pc;r->out->stopped_address=address;
    if(r->out->events>=r->capacity)return NBA97_SPU_INITIALIZE_LIMIT;
    rc=access(r,pc,address);if(rc!=1)return rc;
    if(width&&(address&(width-1u)))return NBA97_PATL_RESOURCE;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=kind;e->pc=pc;e->address=address;e->width=width;e->value=width==2?value&0xffffu:value;
    e->argument[0]=a0;e->argument[1]=a1;
    rc=r->owner->io?r->owner->io(r->owner->user,&r->owner->memory,e,&returned):0;
    if(rc!=1&&rc!=NBA97_SPU_INITIALIZE_TRANSFERRED)return NBA97_PATL_IO_REFUSED;
    e->returned=returned;e->completed=1;e->transferred=(uint8_t)(rc==NBA97_SPU_INITIALIZE_TRANSFERRED);
    ++r->out->callbacks_completed;
    if(returned.known>1)return NBA97_PATL_METADATA;
    if(rc==2) {
        if(kind!=NBA97_SPU_INITIALIZE_CONTROLLER)return NBA97_PATL_METADATA;
        r->out->transferred=1;return NBA97_SPU_INITIALIZE_TRANSFERRED;
    }
    *result=returned;return 1;
}
static int read_device(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t* value) {
    Nba97SpuTransferValue result;int rc=external(r,NBA97_SPU_INITIALIZE_DEVICE_READ,pc,at,width,0,0,0,&result);
    if(rc!=1)return rc;if(!result.known)return NBA97_PATL_RESOURCE;
    *value=width==2?result.word&0xffffu:result.word;return 1;
}
static int write_device(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t value) {
    Nba97SpuTransferValue unused;
    return external(r,NBA97_SPU_INITIALIZE_DEVICE_WRITE,pc,at,width,value,0,0,&unused);
}
#define TRY(expr) do { int rc_=(expr);if(rc_!=1)return rc_; } while(0)
#define READ(pc,at,w,v) TRY(read_ram(r,(pc),(at),(w),&(v)))
#define WRITE(pc,at,w,v) TRY(write_ram(r,(pc),(at),(w),(v)))
#define DEVICE_READ(pc,at,w,v) TRY(read_device(r,(pc),(at),(w),&(v)))
#define DEVICE_WRITE(pc,at,w,v) TRY(write_device(r,(pc),(at),(w),(v)))

static int controller(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t table,target;
    READ(0x8007f5d4u,0x800c7dc4u,4,table);READ(0x8007f5e0u,table+12u,4,target);
    return external(r,NBA97_SPU_INITIALIZE_CONTROLLER,0x8007f5e8u,target,0,0,0,0,returned);
}
static int set_register(Run* r,uint32_t index,uint32_t value,uint32_t shifted,
    Nba97SpuTransferValue* returned) {
    uint32_t base,shift,address;
    if(!shifted) {
        READ(0x8007dd8cu,0x800c75c8u,4,base);address=base+(index<<1);
        DEVICE_WRITE(0x8007dd98u,address,2,value);
    } else {
        READ(0x8007dda8u,0x800c75c8u,4,base);READ(0x8007ddb0u,0x800c75ecu,4,shift);
        address=base+(index<<1);DEVICE_WRITE(0x8007ddbcu,address,2,value>>(shift&31u));
    }
    /* Original index is unchecked and the return is the address, not value. */
    *returned=known(address);return 1;
}
static int hardware(Run* r,uint32_t mode,Nba97SpuTransferValue* returned) {
    uint32_t base,pointer,value,count,i;
    Nba97SpuTransferValue unused;
    READ(0x8007ce28u,0x800c75d8u,4,pointer);DEVICE_READ(0x8007ce30u,pointer,4,value);
    DEVICE_WRITE(0x8007ce3cu,pointer,4,value|0x000b0000u);
    READ(0x8007ce44u,0x800c75c8u,4,base);
    DEVICE_WRITE(0x8007ce4cu,base+0x180u,2,0);DEVICE_WRITE(0x8007ce50u,base+0x182u,2,0);
    DEVICE_WRITE(0x8007ce54u,base+0x1aau,2,0);
    WRITE(0x8007ce6cu,0x800c75e0u,4,0);WRITE(0x8007ce74u,0x800c75e4u,4,0);
    WRITE(0x8007ce7cu,0x800c75c4u,2,0);
    /* The source's nine private stack delay loops are not a native clock. */
    READ(0x8007ceccu,0x800c75c8u,4,base);
    DEVICE_WRITE(0x8007ced4u,base+0x180u,2,0);DEVICE_WRITE(0x8007ced8u,base+0x182u,2,0);
    DEVICE_READ(0x8007cedcu,base+0x1aeu,2,value);WRITE(0x8007cee4u,0x800c75c0u,4,0);
    while(value&0x7ffu) {
        READ(0x8007cef8u,0x800c75c0u,4,count);++count;
        WRITE(0x8007cf08u,0x800c75c0u,4,count);
        if(s32(count)>=0x1389) {
            /* Original timeout prints and CONTINUES initialization. */
            TRY(external(r,NBA97_SPU_INITIALIZE_DIAGNOSTIC,0x8007cf28u,0x80027dd0u,
                0,0,0x80027dd0u,0x80027de0u,&unused));break;
        }
        READ(0x8007cf3cu,0x800c75c8u,4,base);DEVICE_READ(0x8007cf44u,base+0x1aeu,2,value);
    }
    /* Base is cached BEFORE these RAM writes, including physical aliases. */
    READ(0x8007cf5cu,0x800c75c8u,4,base);
    WRITE(0x8007cf68u,0x800c75e8u,4,2);WRITE(0x8007cf74u,0x800c75ecu,4,3);
    WRITE(0x8007cf80u,0x800c75f0u,4,8);WRITE(0x8007cf8cu,0x800c75f4u,4,7);
    DEVICE_WRITE(0x8007cf94u,base+0x1acu,2,4);
    DEVICE_WRITE(0x8007cf9cu,base+0x184u,2,0);DEVICE_WRITE(0x8007cfa0u,base+0x186u,2,0);
    DEVICE_WRITE(0x8007cfa4u,base+0x18cu,2,0xffffu);DEVICE_WRITE(0x8007cfa8u,base+0x18eu,2,0xffffu);
    DEVICE_WRITE(0x8007cfacu,base+0x198u,2,0);DEVICE_WRITE(0x8007cfb0u,base+0x19au,2,0);
    if(!mode) {
        DEVICE_WRITE(0x8007cfccu,base+0x190u,2,0);DEVICE_WRITE(0x8007cfd0u,base+0x192u,2,0);
        DEVICE_WRITE(0x8007cfd4u,base+0x194u,2,0);DEVICE_WRITE(0x8007cfd8u,base+0x196u,2,0);
        DEVICE_WRITE(0x8007cfdcu,base+0x1b0u,2,0);DEVICE_WRITE(0x8007cfe0u,base+0x1b2u,2,0);
        DEVICE_WRITE(0x8007cfe4u,base+0x1b4u,2,0);DEVICE_WRITE(0x8007cfe8u,base+0x1b6u,2,0);
        WRITE(0x8007cff0u,0x800c75c4u,2,0x200u);
        TRY(external(r,NBA97_SPU_INITIALIZE_PIO,0x8007cff4u,0x8007d334u,
            0,0,0x800c7604u,16,&unused));
        READ(0x8007d00cu,0x800c75c8u,4,base);
        for(i=0;i<24;++i,base+=16u) {
            DEVICE_WRITE(0x8007d014u,base,2,0);DEVICE_WRITE(0x8007d018u,base+2u,2,0);
            DEVICE_WRITE(0x8007d01cu,base+4u,2,0x3fffu);DEVICE_WRITE(0x8007d020u,base+6u,2,0x200u);
            DEVICE_WRITE(0x8007d024u,base+8u,2,0);DEVICE_WRITE(0x8007d028u,base+10u,2,0);
        }
        READ(0x8007d040u,0x800c75c8u,4,base);
        /* These discarded reads really occur before the constant writes. */
        DEVICE_READ(0x8007d048u,base+0x188u,2,value);DEVICE_WRITE(0x8007d050u,base+0x188u,2,0xffffu);
        DEVICE_READ(0x8007d054u,base+0x18au,2,value);DEVICE_WRITE(0x8007d060u,base+0x18au,2,value|0xffu);
        READ(0x8007d19cu,0x800c75c8u,4,base);
        DEVICE_READ(0x8007d1a4u,base+0x18cu,2,value);DEVICE_WRITE(0x8007d1acu,base+0x18cu,2,0xffffu);
        DEVICE_READ(0x8007d1b0u,base+0x18eu,2,value);DEVICE_WRITE(0x8007d1bcu,base+0x18eu,2,value|0xffu);
    }
    READ(0x8007d2f8u,0x800c75c8u,4,base);WRITE(0x8007d304u,0x800c75f8u,4,1);
    DEVICE_WRITE(0x8007d30cu,base+0x1aau,2,0xc000u);
    WRITE(0x8007d314u,0x800c75fcu,4,0);WRITE(0x8007d31cu,0x800c7600u,4,0);
    *returned=known(0);return 1;
}
static int initialize(Run* r,uint32_t mode,Nba97SpuTransferValue* returned) {
    Nba97SpuTransferValue unused;uint32_t i,value;
    TRY(controller(r,&unused));TRY(hardware(r,mode,&unused));
    if(!mode)for(i=0;i<24;++i)WRITE(0x8007e42cu,0x800c7676u-i*2u,2,0xc000u);
    TRY(external(r,NBA97_SPU_INITIALIZE_EVENTS,0x8007e43cu,0x8007e4c4u,0,0,0,0,&unused));
    READ(0x8007e44cu,0x800c7a90u,4,value);
    WRITE(0x8007e454u,0x800c7628u,4,0);WRITE(0x8007e45cu,0x800c762cu,4,0);
    WRITE(0x8007e464u,0x800c7638u,4,0);WRITE(0x8007e46cu,0x800c763cu,2,0);
    WRITE(0x8007e474u,0x800c763eu,2,0);WRITE(0x8007e47cu,0x800c7640u,4,0);
    WRITE(0x8007e484u,0x800c7644u,4,0);WRITE(0x8007e48cu,0x800c7630u,4,value);
    TRY(set_register(r,0xd1u,value,0,returned));
    WRITE(0x8007e49cu,0x800c7624u,4,0);WRITE(0x8007e4a4u,0x800c75e0u,4,0);
    WRITE(0x8007e4acu,0x800c767cu,4,0);return 1;
}
int nba97_spu_initialize(Nba97SpuInitialize* owner,enum Nba97SpuInitializeOperation operation,
    uint32_t a0,uint32_t a1,uint32_t a2,Nba97SpuInitializeEvent* journal,size_t capacity,
    Nba97SpuInitializeProgress* out) {
    Run run;int rc;Nba97SpuTransferValue returned={0,0};
    if(!owner||!out||(!journal&&capacity)||(!owner->memory.spans&&owner->memory.count)||
        operation<NBA97_SPU_INITIALIZE_7E6EC||operation>NBA97_SPU_INITIALIZE_REGISTER_7DD80)return NBA97_PATL_ARGUMENT;
    memset(out,0,sizeof *out);run.owner=owner;run.out=out;run.journal=journal;run.capacity=capacity;
    switch(operation) {
    case NBA97_SPU_INITIALIZE_7E6EC:rc=initialize(&run,0,&returned);break;
    case NBA97_SPU_INITIALIZE_MODE_7E3FC:rc=initialize(&run,a0,&returned);break;
    case NBA97_SPU_INITIALIZE_HARDWARE_7CE18:rc=hardware(&run,a0,&returned);break;
    case NBA97_SPU_INITIALIZE_CONTROLLER_7F5D0:rc=controller(&run,&returned);break;
    default:rc=set_register(&run,a0,a1,a2,&returned);break;
    }
    if(rc==1) { out->completed=1;out->returned=returned;out->stopped_pc=0;out->stopped_address=0; }
    else if(rc==NBA97_SPU_INITIALIZE_TRANSFERRED)out->completed=1;
    return rc;
}
