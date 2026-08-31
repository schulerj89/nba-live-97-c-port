#include "spu_events.h"
#include <string.h>
typedef struct Run {
    Nba97SpuEvents* owner;Nba97SpuEventsProgress* out;
    Nba97SpuEventsEvent* journal;size_t capacity;
} Run;
static Nba97SpuTransferValue known(uint32_t word) {
    Nba97SpuTransferValue v;v.word=word;v.known=1;return v;
}
static int access(Run* r,uint32_t pc,uint32_t at) {
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->accesses>=r->owner->access_budget)return NBA97_SPU_EVENTS_LIMIT;
    ++r->out->accesses;return 1;
}
static int read_ram(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t* value) {
    int rc=access(r,pc,at);return rc==1?nba97_voice_patl_read(&r->owner->memory,at,width,value):rc;
}
static int write_ram(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t value) {
    int rc;Nba97SpuEventsEvent* e;r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->events>=r->capacity)return NBA97_SPU_EVENTS_LIMIT;
    rc=access(r,pc,at);if(rc!=1)return rc;
    rc=nba97_voice_patl_write(&r->owner->memory,at,width,value);if(rc!=1)return rc;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=NBA97_SPU_EVENTS_RAM_STORE;e->pc=pc;e->address=at;e->width=width;
    e->value=width==2?value&0xffffu:value;e->completed=1;++r->out->stores;return 1;
}
static int external(Run* r,enum Nba97SpuEventsKind kind,uint32_t pc,uint32_t at,
    uint32_t width,uint32_t value,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,Nba97SpuTransferValue* out) {
    int rc;Nba97SpuEventsEvent* e;Nba97SpuTransferValue returned={0,0};
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->events>=r->capacity)return NBA97_SPU_EVENTS_LIMIT;
    rc=access(r,pc,at);if(rc!=1)return rc;
    if(width&&(at&(width-1u)))return NBA97_PATL_RESOURCE;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=kind;e->pc=pc;e->address=at;e->width=width;e->value=width==2?value&0xffffu:value;
    e->argument[0]=a0;e->argument[1]=a1;e->argument[2]=a2;e->argument[3]=a3;
    if(!r->owner->io||r->owner->io(r->owner->user,&r->owner->memory,e,&returned)!=1)return NBA97_PATL_IO_REFUSED;
    e->returned=returned;e->completed=1;++r->out->callbacks_completed;
    if(returned.known>1)return NBA97_PATL_METADATA;
    *out=returned;return 1;
}
static int read_device(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t* value) {
    Nba97SpuTransferValue v;int rc=external(r,NBA97_SPU_EVENTS_DEVICE_READ,pc,at,width,0,0,0,0,0,&v);
    if(rc!=1)return rc;if(!v.known)return NBA97_PATL_RESOURCE;
    *value=width==2?v.word&0xffffu:v.word;return 1;
}
static int write_device(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t value) {
    Nba97SpuTransferValue unused;
    return external(r,NBA97_SPU_EVENTS_DEVICE_WRITE,pc,at,width,value,0,0,0,0,&unused);
}
#define TRY(expr) do { int rc_=(expr);if(rc_!=1)return rc_; } while(0)
#define READ(pc,at,w,v) TRY(read_ram(r,(pc),(at),(w),&(v)))
#define WRITE(pc,at,w,v) TRY(write_ram(r,(pc),(at),(w),(v)))
#define DEVICE_READ(pc,at,w,v) TRY(read_device(r,(pc),(at),(w),&(v)))
#define DEVICE_WRITE(pc,at,w,v) TRY(write_device(r,(pc),(at),(w),(v)))
static int irq_mask(Run* r,uint32_t mask,Nba97SpuTransferValue* returned) {
    uint32_t pointer,old;READ(0x8007f6f0u,0x800c7e30u,4,pointer);
    DEVICE_READ(0x8007f6f8u,pointer,2,old);DEVICE_WRITE(0x8007f6fcu,pointer,2,mask);
    *returned=known(old);return 1;
}
static int set_callback(Run* r,uint32_t channel,uint32_t callback,Nba97SpuTransferValue* returned) {
    uint32_t slot=0x800c7e3cu+(channel<<2),old,enabled,pointer,value;
    Nba97SpuTransferValue saved_mask,unused;
    READ(0x8007fde4u,slot,4,old);*returned=known(old);
    if(old==callback)return 1;
    READ(0x8007fdf8u,0x800c7e38u,2,enabled);if(!enabled)return 1;
    TRY(irq_mask(r,0,&saved_mask));
    if(callback) {
        READ(0x8007fe20u,0x800c7e5cu,4,pointer);
        WRITE(0x8007fe28u,slot,4,callback);DEVICE_READ(0x8007fe2cu,pointer,4,value);
        value=(value&0x00ffffffu)|(1u<<((channel+16u)&31u))|0x00800000u;
        DEVICE_WRITE(0x8007fe4cu,pointer,4,value);
    } else {
        READ(0x8007fe60u,0x800c7e5cu,4,pointer);
        WRITE(0x8007fe68u,slot,4,0);DEVICE_READ(0x8007fe6cu,pointer,4,value);
        value=((value&0x00ffffffu)|0x00800000u)&~(1u<<((channel+16u)&31u));
        DEVICE_WRITE(0x8007fe90u,pointer,4,value);
    }
    /* Original restores through a FRESH C7E30 pointer read. Callback-table
     * or register aliases can redirect it; retain the source store order. */
    TRY(irq_mask(r,saved_mask.word,&unused));*returned=known(old);return 1;
}
static int dispatch(Run* r,uint32_t channel,uint32_t callback,Nba97SpuTransferValue* returned) {
    uint32_t pointer,target;READ(0x8007f634u,0x800c7dc4u,4,pointer);
    READ(0x8007f640u,pointer+4u,4,target);
    if(target==0x8007fdb8u)return set_callback(r,channel,callback,returned);
    return external(r,NBA97_SPU_EVENTS_OTHER_DISPATCH,0x8007f648u,target,0,0,channel,callback,0,0,returned);
}
static int bios(Run* r,enum Nba97SpuEventsKind kind,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,Nba97SpuTransferValue* returned) {
    uint32_t pc;
    switch(kind) {
    case NBA97_SPU_EVENTS_ENTER_CRITICAL:pc=0x8007f30cu;a0=1;break;
    case NBA97_SPU_EVENTS_EXIT_CRITICAL:pc=0x8007f57cu;a0=2;break;
    case NBA97_SPU_EVENTS_OPEN_EVENT:pc=0x8007f53cu;break;
    case NBA97_SPU_EVENTS_ENABLE_EVENT:pc=0x8007f35cu;break;
    case NBA97_SPU_EVENTS_CLOSE_EVENT:pc=0x8007f2fcu;break;
    default:pc=0x8007f4fcu;break;
    }
    if(kind!=NBA97_SPU_EVENTS_OPEN_EVENT) { a1=0;a2=0;a3=0; }
    return external(r,kind,pc,a0,0,0,a0,a1,a2,a3,returned);
}
static int initialize(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t guard;Nba97SpuTransferValue unused,handle;
    READ(0x8007e4c8u,0x800c7a80u,4,guard);if(guard) { *returned=known(guard);return 1; }
    /* Original publishes the guard BEFORE operations that can fail. No
     * rollback or manufactured retry is added when a later native call refuses. */
    WRITE(0x8007e4e0u,0x800c7a80u,4,1);
    TRY(bios(r,NBA97_SPU_EVENTS_ENTER_CRITICAL,1,0,0,0,&unused));
    WRITE(0x8007e4f8u,0x800c7620u,4,0);TRY(dispatch(r,4,0x8007d668u,&unused));
    TRY(bios(r,NBA97_SPU_EVENTS_OPEN_EVENT,0xf0000009u,0x20,0x2000,0,&handle));
    if(!handle.known)return NBA97_PATL_RESOURCE;
    WRITE(0x8007e524u,0x800c7678u,4,handle.word);
    /* The raw OpenEvent result is stored/enabled without a success check. */
    TRY(bios(r,NBA97_SPU_EVENTS_ENABLE_EVENT,handle.word,0,0,0,&unused));
    return bios(r,NBA97_SPU_EVENTS_EXIT_CRITICAL,2,0,0,0,returned);
}
static int shutdown(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t guard,handle;Nba97SpuTransferValue unused;
    READ(0x8007e824u,0x800c7a80u,4,guard);if(guard!=1) { *returned=known(1);return 1; }
    WRITE(0x8007e838u,0x800c7a80u,4,0);
    TRY(bios(r,NBA97_SPU_EVENTS_ENTER_CRITICAL,1,0,0,0,&unused));
    WRITE(0x8007e84cu,0x800c75fcu,4,0);WRITE(0x8007e854u,0x800c7600u,4,0);
    TRY(dispatch(r,4,0,&unused));READ(0x8007e864u,0x800c7678u,4,handle);
    TRY(bios(r,NBA97_SPU_EVENTS_CLOSE_EVENT,handle,0,0,0,&unused));
    /* Close precedes Disable, its return is ignored, and the live handle is
     * reread. Original leaves the retained handle word unchanged afterward. */
    READ(0x8007e874u,0x800c7678u,4,handle);
    TRY(bios(r,NBA97_SPU_EVENTS_DISABLE_EVENT,handle,0,0,0,&unused));
    return bios(r,NBA97_SPU_EVENTS_EXIT_CRITICAL,2,0,0,0,returned);
}
int nba97_spu_events(Nba97SpuEvents* owner,enum Nba97SpuEventsOperation op,
    uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,Nba97SpuEventsEvent* journal,size_t capacity,Nba97SpuEventsProgress* out) {
    Run run;int rc;Nba97SpuTransferValue returned={0,0};
    if(!owner||!out||(!journal&&capacity)||(!owner->memory.spans&&owner->memory.count)||
        op<NBA97_SPU_EVENTS_INITIALIZE_7E4C4||op>NBA97_SPU_EVENTS_DISABLE_7F4F8)return NBA97_PATL_ARGUMENT;
    memset(out,0,sizeof *out);run.owner=owner;run.out=out;run.journal=journal;run.capacity=capacity;
    switch(op) {
    case NBA97_SPU_EVENTS_INITIALIZE_7E4C4:rc=initialize(&run,&returned);break;
    case NBA97_SPU_EVENTS_SHUTDOWN_7E81C:rc=shutdown(&run,&returned);break;
    case NBA97_SPU_EVENTS_REGISTER_7E548:rc=dispatch(&run,4,a0,&returned);break;
    case NBA97_SPU_EVENTS_DISPATCH_7F630:rc=dispatch(&run,a0,a1,&returned);break;
    case NBA97_SPU_EVENTS_SET_CALLBACK_7FDB8:rc=set_callback(&run,a0,a1,&returned);break;
    case NBA97_SPU_EVENTS_IRQ_MASK_7F6EC:rc=irq_mask(&run,a0,&returned);break;
    default:rc=bios(&run,(enum Nba97SpuEventsKind)(NBA97_SPU_EVENTS_ENTER_CRITICAL+(op-NBA97_SPU_EVENTS_ENTER_7F308)),a0,a1,a2,a3,&returned);break;
    }
    if(rc==1) { out->returned=returned;out->completed=1;out->stopped_pc=0;out->stopped_address=0; }
    return rc;
}
