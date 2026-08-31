#include "audio_startup.h"
#include <string.h>

typedef struct Run {
    Nba97AudioStartup* owner;Nba97AudioStartupProgress* out;
    Nba97AudioStartupEvent* journal;size_t capacity;
    uint8_t parameter[40],parameter_known[40];
} Run;
static Nba97SpuTransferValue known(uint32_t word) {
    Nba97SpuTransferValue v;v.word=word;v.known=1;return v;
}
static void stop(Run* r,uint32_t pc,uint32_t at,int local) {
    r->out->stopped_pc=pc;r->out->stopped_address=at;r->out->stopped_local=(uint8_t)local;
}
static int access(Run* r,uint32_t pc,uint32_t at,int local) {
    stop(r,pc,at,local);
    if(r->out->accesses>=r->owner->access_budget)return NBA97_AUDIO_STARTUP_LIMIT;
    ++r->out->accesses;return 1;
}
static uint32_t truncate(uint32_t value,uint32_t width) {
    return width==1?value&0xffu:width==2?value&0xffffu:value;
}
static int read_ram(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t* value) {
    int rc=access(r,pc,at,0);
    return rc==1?nba97_voice_patl_read(&r->owner->memory,at,width,value):rc;
}
static int parameter_read(Run* r,int local,uint32_t base,uint32_t pc,
    uint32_t offset,uint32_t width,uint32_t* value) {
    uint32_t i,v=0;int rc;
    if(!local)return read_ram(r,pc,base+offset,width,value);
    rc=access(r,pc,offset,1);if(rc!=1)return rc;
    if(offset>sizeof r->parameter||width>sizeof r->parameter-offset||
        (offset&(width-1u)))return NBA97_PATL_RESOURCE;
    for(i=0;i<width;++i) {
        if(r->parameter_known[offset+i]!=1)return NBA97_PATL_RESOURCE;
        v|=(uint32_t)r->parameter[offset+i]<<(i*8u);
    }
    *value=v;return 1;
}
static int store(Run* r,int local,uint32_t pc,uint32_t at,uint32_t width,uint32_t value) {
    int rc;uint32_t i;Nba97AudioStartupEvent* e;
    stop(r,pc,at,local);if(r->out->events>=r->capacity)return NBA97_AUDIO_STARTUP_LIMIT;
    rc=access(r,pc,at,local);if(rc!=1)return rc;
    if(local) {
        if(at>sizeof r->parameter||width>sizeof r->parameter-at||
            (at&(width-1u)))return NBA97_PATL_RESOURCE;
        for(i=0;i<width;++i) {
            r->parameter[at+i]=(uint8_t)(value>>(i*8u));r->parameter_known[at+i]=1;
        }
    } else {
        rc=nba97_voice_patl_write(&r->owner->memory,at,width,value);if(rc!=1)return rc;
    }
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=local?NBA97_AUDIO_STARTUP_PARAMETER_STORE:NBA97_AUDIO_STARTUP_RAM_STORE;
    e->pc=pc;e->address=at;e->width=width;e->value=truncate(value,width);
    e->completed=1;++r->out->stores;return 1;
}
static int external(Run* r,enum Nba97AudioStartupKind kind,uint32_t pc,uint32_t at,
    uint32_t width,uint32_t value,uint32_t a0,uint32_t a1,Nba97SpuTransferValue* result) {
    Nba97AudioStartupEvent* e;Nba97SpuTransferValue returned={0,0};int rc;
    stop(r,pc,at,0);if(r->out->events>=r->capacity)return NBA97_AUDIO_STARTUP_LIMIT;
    rc=access(r,pc,at,0);if(rc!=1)return rc;
    if(width&&(at&(width-1u)))return NBA97_PATL_RESOURCE;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=kind;e->pc=pc;e->address=at;e->width=width;e->value=truncate(value,width);
    e->argument[0]=a0;e->argument[1]=a1;
    rc=r->owner->io?r->owner->io(r->owner->user,&r->owner->memory,e,&returned):0;
    if(rc!=1&&rc!=NBA97_AUDIO_STARTUP_TRANSFERRED)return NBA97_PATL_IO_REFUSED;
    e->returned=returned;e->completed=1;e->transferred=(uint8_t)(rc==2);
    ++r->out->callbacks_completed;
    if(returned.known>1)return NBA97_PATL_METADATA;
    if(rc==2) {
        if(kind!=NBA97_AUDIO_STARTUP_INITIALIZE)return NBA97_PATL_METADATA;
        r->out->transferred=1;return NBA97_AUDIO_STARTUP_TRANSFERRED;
    }
    *result=returned;return 1;
}
static int device_read(Run* r,uint32_t pc,uint32_t at,uint32_t* value) {
    Nba97SpuTransferValue result;int rc=external(r,NBA97_AUDIO_STARTUP_DEVICE_READ,pc,at,2,0,0,0,&result);
    if(rc!=1)return rc;if(!result.known)return NBA97_PATL_RESOURCE;
    *value=result.word&0xffffu;return 1;
}
static int device_write(Run* r,uint32_t pc,uint32_t at,uint32_t value) {
    Nba97SpuTransferValue unused;
    return external(r,NBA97_AUDIO_STARTUP_DEVICE_WRITE,pc,at,2,value,0,0,&unused);
}
#define TRY(expr) do { int rc_=(expr);if(rc_!=1)return rc_; } while(0)
#define READ(pc,at,w,v) TRY(read_ram(r,(pc),(at),(w),&(v)))
#define PARAM(pc,off,w,v) TRY(parameter_read(r,local,parameter,(pc),(off),(w),&(v)))
#define WRITE(pc,at,w,v) TRY(store(r,0,(pc),(at),(w),(v)))
#define LOCAL(pc,at,w,v) TRY(store(r,1,(pc),(at),(w),(v)))

static int master_volume(Run* r,int local,uint32_t parameter,int right,int use_mode) {
    uint32_t mode=0,target,flags=0,value,base;
    uint32_t delta=right?0xc8u:0,offset=right?6u:4u;
    if(use_mode) {
        PARAM(0x8007ded8u+delta,offset+4u,2,mode);
        /* Original LH then unsigned comparison rejects negative modes. */
        if(mode<8) {
            READ(right?0x8007dfbcu:0x8007def4u,
                (right?0x80027e38u:0x80027e18u)+mode*4u,4,target);
            if(target!=0x8007df3cu+delta) {
                if(target<0x8007df04u+delta||target>0x8007df34u+delta||
                    ((target-(0x8007df04u+delta))&7u)) {
                    stop(r,right?0x8007dfc4u:0x8007defcu,target,0);
                    return NBA97_PATL_IO_REFUSED;
                }
                flags=0x8000u+((target-(0x8007df04u+delta))/8u)*0x1000u;
            }
        }
    }
    if(flags) {
        PARAM(0x8007df4cu+delta,offset,2,value);
        value=(value&0x8000u)?0u:value>=128u?127u:value;
    } else PARAM(0x8007df3cu+delta,offset,2,value);
    READ(0x8007df7cu+delta,0x800c75c8u,4,base);
    return device_write(r,0x8007df84u+delta,base+(right?0x182u:0x180u),(value&0x7fffu)|flags);
}
static int common_volume(Run* r,int local,uint32_t parameter,uint32_t load_pc,
    uint32_t offset,uint32_t device_offset) {
    uint32_t base,value;
    /* Source caches device base BEFORE reading this parameter. */
    READ(load_pc,0x800c75c8u,4,base);PARAM(load_pc+4u,offset,2,value);
    return device_write(r,load_pc+12u,base+device_offset,value);
}
static int common_control(Run* r,int local,uint32_t parameter,uint32_t load_pc,
    uint32_t offset,uint32_t bit,uint32_t* returned_base) {
    uint32_t enabled,base,value;
    PARAM(load_pc,offset,4,enabled);
    READ(load_pc+(enabled?44u:20u),0x800c75c8u,4,base);
    TRY(device_read(r,load_pc+(enabled?52u:28u),base+0x1aau,&value));
    /* An effect may mutate the live pointer; this matching store uses cached base. */
    TRY(device_write(r,load_pc+64u,base+0x1aau,enabled?value|bit:value&~bit));
    *returned_base=base;return 1;
}
static int common(Run* r,int local,uint32_t parameter,Nba97SpuTransferValue* returned) {
    uint32_t mask,base=0;int all;
    PARAM(0x8007deb0u,0,4,mask);all=mask==0;
    if(all||(mask&1u))TRY(master_volume(r,local,parameter,0,all||(mask&4u)));
    if(all||(mask&2u))TRY(master_volume(r,local,parameter,1,all||(mask&8u)));
    if(all||(mask&0x40u))TRY(common_volume(r,local,parameter,0x8007e064u,0x10,0x1b0));
    if(all||(mask&0x80u))TRY(common_volume(r,local,parameter,0x8007e088u,0x12,0x1b2));
    if(all||(mask&0x100u))TRY(common_control(r,local,parameter,0x8007e0a8u,0x14,4,&base));
    if(all||(mask&0x200u))TRY(common_control(r,local,parameter,0x8007e0fcu,0x18,1,&base));
    if(all||(mask&0x400u))TRY(common_volume(r,local,parameter,0x8007e154u,0x1c,0x1b4));
    if(all||(mask&0x800u))TRY(common_volume(r,local,parameter,0x8007e178u,0x1e,0x1b6));
    if(all||(mask&0x1000u))TRY(common_control(r,local,parameter,0x8007e198u,0x20,8,&base));
    if(all||(mask&0x2000u)) {
        TRY(common_control(r,local,parameter,0x8007e1ecu,0x24,2,&base));
        *returned=known(base);
    } else *returned=known(0); /* Original final AND overwrites earlier v0. */
    return 1;
}
static int reset_music(Run* r,Nba97SpuTransferValue* returned) {
    static const uint32_t writes[][4]={
        {0x80073a78u,0x800c6d29u,1,0},{0x80073a80u,0x800c6d4cu,4,0},
        {0x80073a8cu,0x800e45e8u,1,2},{0x80073a94u,0x800e45f2u,2,0},
        {0x80073aa0u,0x800e45f4u,2,2},{0x80073aacu,0x800e45e4u,1,0xff},
        {0x80073ab8u,0x800e45e5u,1,0xff},{0x80073ac4u,0x800e45e7u,1,1},
        {0x80073accu,0x800e45eau,1,0},{0x80073ad4u,0x800e45e9u,1,0},
        {0x80073adcu,0x800e4600u,2,0},{0x80073ae8u,0x800e45fcu,2,0xffff},
        {0x80073af4u,0x800e45fau,2,0xffff},{0x80073b00u,0x800e45feu,2,0xffff},
        {0x80073b08u,0x800e45e6u,1,0},{0x80073b10u,0x800e45ecu,2,0},
        {0x80073b18u,0x800e45eeu,2,0},{0x80073b20u,0x800e45f0u,2,0},
        {0x80073b28u,0x800e4610u,4,0},{0x80073b30u,0x800e4614u,4,0},
        {0x80073b3cu,0x800e460cu,4,0xffffffffu},{0x80073b44u,0x800e4624u,4,0},
        {0x80073b4cu,0x800e462cu,4,0},{0x80073b54u,0x800e45f6u,2,0},
        {0x80073b5cu,0x800e45ebu,1,0},{0x80073b64u,0x800e45f8u,2,0},
        {0x80073b6cu,0x800e4604u,4,0},{0x80073b74u,0x800e4628u,4,0},
        {0x80073b7cu,0x800e4618u,4,0},{0x80073b84u,0x800e461cu,4,0}
    };
    size_t i;
    /* Original writes selected fields, leaving gaps and other state untouched. */
    for(i=0;i<sizeof writes/sizeof writes[0];++i)
        WRITE(writes[i][0],writes[i][1],writes[i][2],writes[i][3]);
    *returned=known(0xffffffffu);return 1;
}
static int register_callback(Run* r,uint32_t callback,Nba97SpuTransferValue* returned) {
    uint32_t i,slot;
    for(i=0;i<8;++i) {
        READ(0x8008e0ecu,0x800d96e8u+i*4u,4,slot);
        if(!slot) { WRITE(0x8008e100u,0x800d96e8u+i*4u,4,callback);break; }
    }
    /* Original bug: a full table silently returns zero, just like success.
     * A NULL callback also stores zero and leaves the slot available. */
    *returned=known(0);return 1;
}
static int startup(Run* r,Nba97SpuTransferValue* returned) {
    Nba97SpuTransferValue unused;uint32_t registered;
    TRY(external(r,NBA97_AUDIO_STARTUP_INITIALIZE,0x800700c8u,0x8007e6ecu,0,0,0,0,&unused));
    TRY(external(r,NBA97_AUDIO_STARTUP_HEAP,0x800700dcu,0x8007e940u,0,0,128,0x800fee50u,&unused));
    /* Ignored lower raw returns are original behavior; native refusals still stop. */
    LOCAL(0x800700e8u,0,4,0x2c3);LOCAL(0x800700ecu,4,2,0);LOCAL(0x800700f0u,6,2,0);
    LOCAL(0x800700f4u,0x10,2,0);LOCAL(0x800700f8u,0x12,2,0);LOCAL(0x80070100u,0x18,4,1);
    TRY(common(r,1,0,&unused));
    LOCAL(0x80070114u,0,4,0x2c3);LOCAL(0x8007011cu,4,2,0x3fff);LOCAL(0x80070124u,6,2,0x3fff);
    LOCAL(0x8007012cu,0x10,2,0x3fff);LOCAL(0x80070134u,0x12,2,0x3fff);LOCAL(0x8007013cu,0x18,4,1);
    TRY(common(r,1,0,&unused));TRY(reset_music(r,&unused));
    READ(0x80070158u,0x800c6d28u,1,registered);
    if(!registered) {
        /* Original bug: publish guard before registration; a full table will
         * permanently suppress later registration attempts. Preserve the prefix. */
        WRITE(0x80070170u,0x800c6d28u,1,1);
        TRY(register_callback(r,0x8007a6a8u,&unused));
    }
    *returned=known(0);return 1;
}
int nba97_audio_startup(Nba97AudioStartup* owner,enum Nba97AudioStartupOperation operation,
    uint32_t a0,Nba97AudioStartupEvent* journal,size_t capacity,Nba97AudioStartupProgress* out) {
    Run run;Nba97SpuTransferValue returned={0,0};int rc;
    if(!owner||!out||(!journal&&capacity)||(!owner->memory.spans&&owner->memory.count)||
        operation<NBA97_AUDIO_STARTUP_700B0||operation>NBA97_AUDIO_STARTUP_REGISTER_8E0E0)return NBA97_PATL_ARGUMENT;
    memset(out,0,sizeof *out);run.owner=owner;run.out=out;run.journal=journal;run.capacity=capacity;
    memset(run.parameter,0xcd,sizeof run.parameter);memset(run.parameter_known,0,sizeof run.parameter_known);
    switch(operation) {
    case NBA97_AUDIO_STARTUP_700B0:rc=startup(&run,&returned);break;
    case NBA97_AUDIO_STARTUP_COMMON_7DEA8:rc=common(&run,0,a0,&returned);break;
    case NBA97_AUDIO_STARTUP_RESET_73A68:rc=reset_music(&run,&returned);break;
    default:rc=register_callback(&run,a0,&returned);break;
    }
    if(rc==1) { out->completed=1;out->returned=returned;stop(&run,0,0,0); }
    else if(rc==NBA97_AUDIO_STARTUP_TRANSFERRED)out->completed=1;
    return rc;
}
