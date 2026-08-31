#include "interrupt_controller.h"
#include <string.h>
typedef struct Run {
    Nba97InterruptController* owner;Nba97InterruptProgress* out;
    Nba97InterruptEvent* journal;size_t capacity,depth;
} Run;
static Nba97SpuTransferValue known(uint32_t word) {
    Nba97SpuTransferValue v;v.word=word;v.known=1;return v;
}
static int access(Run* r,uint32_t pc,uint32_t at) {
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->accesses>=r->owner->access_budget)return NBA97_INTERRUPT_LIMIT;
    ++r->out->accesses;return 1;
}
static int read_ram(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t* value) {
    int rc=access(r,pc,at);return rc==1?nba97_voice_patl_read(&r->owner->memory,at,width,value):rc;
}
static int write_ram(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t value) {
    int rc;Nba97InterruptEvent* e;r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->events>=r->capacity)return NBA97_INTERRUPT_LIMIT;
    rc=access(r,pc,at);if(rc!=1)return rc;
    rc=nba97_voice_patl_write(&r->owner->memory,at,width,value);if(rc!=1)return rc;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=NBA97_INTERRUPT_RAM_STORE;e->pc=pc;e->address=at;e->width=width;
    e->value=width==2?value&0xffffu:value;e->completed=1;++r->out->stores;return 1;
}
static int external(Run* r,enum Nba97InterruptKind kind,uint32_t pc,uint32_t at,
    uint32_t width,uint32_t value,uint32_t a0,uint32_t a1,uint32_t a2,Nba97SpuTransferValue* out) {
    int rc;Nba97InterruptEvent* e;Nba97SpuTransferValue returned={0,0};
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->events>=r->capacity)return NBA97_INTERRUPT_LIMIT;
    rc=access(r,pc,at);if(rc!=1)return rc;
    if(width&&(at&(width-1u)))return NBA97_PATL_RESOURCE;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=kind;e->pc=pc;e->address=at;e->width=width;e->value=width==2?value&0xffffu:value;
    e->argument[0]=a0;e->argument[1]=a1;e->argument[2]=a2;
    rc=r->owner->io?r->owner->io(r->owner->user,&r->owner->memory,e,&returned):0;
    if(rc!=1&&rc!=NBA97_INTERRUPT_TRANSFERRED)return NBA97_PATL_IO_REFUSED;
    e->returned=returned;e->completed=1;e->transferred=(uint8_t)(rc==NBA97_INTERRUPT_TRANSFERRED);
    ++r->out->callbacks_completed;
    if(returned.known>1)return NBA97_PATL_METADATA;
    if(rc==NBA97_INTERRUPT_TRANSFERRED&&kind!=NBA97_INTERRUPT_RETURN_EXCEPTION&&kind!=NBA97_INTERRUPT_CALLBACK)
        return NBA97_PATL_METADATA;
    *out=returned;return rc;
}
static int read_device(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t* value) {
    Nba97SpuTransferValue v;int rc=external(r,NBA97_INTERRUPT_DEVICE_READ,pc,at,width,0,0,0,0,&v);
    if(rc!=1)return rc;if(!v.known)return NBA97_PATL_RESOURCE;
    *value=width==2?v.word&0xffffu:v.word;return 1;
}
static int write_device(Run* r,uint32_t pc,uint32_t at,uint32_t width,uint32_t value) {
    Nba97SpuTransferValue unused;
    return external(r,NBA97_INTERRUPT_DEVICE_WRITE,pc,at,width,value,0,0,0,&unused);
}
#define TRY(expr) do { int rc_=(expr);if(rc_!=1)return rc_; } while(0)
#define READ(pc,at,w,v) TRY(read_ram(r,(pc),(at),(w),&(v)))
#define WRITE(pc,at,w,v) TRY(write_ram(r,(pc),(at),(w),(v)))
#define DEVICE_READ(pc,at,w,v) TRY(read_device(r,(pc),(at),(w),&(v)))
#define DEVICE_WRITE(pc,at,w,v) TRY(write_device(r,(pc),(at),(w),(v)))
static int bios(Run* r,enum Nba97InterruptKind kind,uint32_t a0,uint32_t a1,Nba97SpuTransferValue* returned) {
    uint32_t pc;
    switch(kind) {
    case NBA97_INTERRUPT_ENTER_CRITICAL:pc=0x8007f30cu;a0=1;a1=0;break;
    case NBA97_INTERRUPT_EXIT_CRITICAL:pc=0x8007f57cu;a0=2;a1=0;break;
    case NBA97_INTERRUPT_CAPTURE_CONTEXT:pc=0x80083b34u;a1=0;break;
    case NBA97_INTERRUPT_REMOVE_CDROM_DRIVER:pc=0x8007f59cu;a0=0;a1=0;break;
    case NBA97_INTERRUPT_HOOK_CONTEXT:pc=0x8007fb7cu;a1=0;break;
    case NBA97_INTERRUPT_CHANGE_CLEAR_PAD:pc=0x8007f51cu;a1=0;break;
    case NBA97_INTERRUPT_CHANGE_CLEAR_COUNTER:pc=0x8007fb8cu;break;
    default:pc=0x8007fb9cu;a0=0;a1=0;break;
    }
    return external(r,kind,pc,a0,0,0,a0,a1,0,returned);
}
static int diagnostic(Run* r,uint32_t format,uint32_t a1,uint32_t a2,Nba97SpuTransferValue* returned) {
    return external(r,NBA97_INTERRUPT_DIAGNOSTIC,0x80083b24u,format,0,0,format,a1,a2,returned);
}
static int clear_words(Run* r,uint32_t pc,uint32_t at,uint32_t count,Nba97SpuTransferValue* returned) {
    /* Original word counts are unsigned loop state, not a byte count or a
     * bounded SDK array size. Huge/wrapped requests retain every reached store. */
    while(count) { WRITE(pc,at,4,0);at+=4u;--count; }
    *returned=known(0xffffffffu);return 1;
}
static int register_callback(Run* r,uint32_t channel,uint32_t callback,Nba97SpuTransferValue* returned) {
    uint32_t slot=0x800c7dccu+(channel<<2),old,ready,pointer,mask,cache,bit=1u<<(channel&31u);
    Nba97SpuTransferValue unused;
    READ(0x8007f9e4u,slot,4,old);*returned=known(old);if(old==callback)return 1;
    READ(0x8007f9f4u,0x800c7dc8u,2,ready);if(!ready)return 1;
    READ(0x8007fa08u,0x800c7e30u,4,pointer);
    DEVICE_READ(0x8007fa10u,pointer,2,mask);DEVICE_WRITE(0x8007fa14u,pointer,2,0);
    if(callback) {
        WRITE(0x8007fa28u,slot,4,callback);READ(0x8007fa2cu,0x800c7df8u,4,cache);
        mask|=bit;WRITE(0x8007fa38u,0x800c7df8u,4,cache|bit);
    } else {
        WRITE(0x8007fa50u,slot,4,0);READ(0x8007fa58u,0x800c7df8u,4,cache);
        mask&=~bit;WRITE(0x8007fa68u,0x800c7df8u,4,cache&~bit);
    }
    /* Unchecked source indices can alias the cached mask or this pointer.
     * Read after the callback store exactly as the original does. */
    READ(0x8007fa70u,0x800c7e30u,4,pointer);DEVICE_WRITE(0x8007fa78u,pointer,2,mask);
    if(channel>=4u&&channel<=6u)
        TRY(bios(r,NBA97_INTERRUPT_CHANGE_CLEAR_COUNTER,channel-4u,callback==0,&unused));
    *returned=known(old);return 1;
}
static int dispatch(Run* r,uint32_t channel,uint32_t callback,Nba97SpuTransferValue* returned) {
    uint32_t pointer,target;READ(0x8007f604u,0x800c7dc4u,4,pointer);
    READ(0x8007f610u,pointer+8u,4,target);
    if(target==0x8007f9bcu)return register_callback(r,channel,callback,returned);
    return external(r,NBA97_INTERRUPT_OTHER_DISPATCH,0x8007f618u,target,0,0,channel,callback,0,returned);
}
static int dma_initialize(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t pointer;Nba97SpuTransferValue unused;
    TRY(clear_words(r,0x8007fed0u,0x800c7e38u,9,&unused));
    READ(0x8007fbd0u,0x800c7e5cu,4,pointer);DEVICE_WRITE(0x8007fbdcu,pointer,4,0);
    TRY(dispatch(r,3,0x8007fc54u,&unused));WRITE(0x8007fbf4u,0x800c7e38u,2,1);
    *returned=known(0x8007fdb8u);return 1;
}
static int dma_shutdown(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t pointer;Nba97SpuTransferValue unused;WRITE(0x8007fc1cu,0x800c7e38u,2,0);
    TRY(clear_words(r,0x8007fed0u,0x800c7e38u,9,&unused));
    READ(0x8007fc30u,0x800c7e5cu,4,pointer);DEVICE_WRITE(0x8007fc38u,pointer,4,0);
    return dispatch(r,3,0,returned);
}
static int vblank_initialize(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t pointer;Nba97SpuTransferValue unused;READ(0x8007fef4u,0x800c7e64u,4,pointer);
    DEVICE_WRITE(0x8007ff04u,pointer,4,0x107);
    TRY(bios(r,NBA97_INTERRUPT_CHANGE_CLEAR_PAD,0,0,&unused));
    TRY(bios(r,NBA97_INTERRUPT_CHANGE_CLEAR_COUNTER,3,0,&unused));
    TRY(clear_words(r,0x80080030u,0x800c7e68u,2,&unused));
    TRY(dispatch(r,0,0x8007ff9cu,&unused));WRITE(0x8007ff4cu,0x800c7e68u,2,1);
    *returned=known(0x8007ffe8u);return 1;
}
static int vblank_shutdown(Run* r,Nba97SpuTransferValue* returned) {
    Nba97SpuTransferValue unused;WRITE(0x8007ff7cu,0x800c7e68u,2,0);
    TRY(clear_words(r,0x80080030u,0x800c7e68u,2,&unused));return dispatch(r,0,0,returned);
}
static int vblank_set(Run* r,uint32_t callback,Nba97SpuTransferValue incoming_v1,Nba97SpuTransferValue* returned) {
    uint32_t ready,old;READ(0x8007ffecu,0x800c7e68u,2,ready);
    /* Original disabled branch returns its untouched incoming v1. */
    if(!ready) { if(incoming_v1.known>1)return NBA97_PATL_METADATA;*returned=incoming_v1;return 1; }
    READ(0x80080000u,0x800c7e6cu,4,old);
    if(callback!=old)WRITE(0x80080014u,0x800c7e6cu,4,callback);
    *returned=known(old);return 1;
}
static int dma_handle(Run*,Nba97SpuTransferValue*);
static int vblank_handle(Run*,Nba97SpuTransferValue*);
static int callback(Run* r,uint32_t pc,uint32_t target,Nba97SpuTransferValue* returned) {
    int rc;
    if(target==0x8007fc54u||target==0x8007ff9cu) {
        r->out->stopped_pc=pc;r->out->stopped_address=target;
        if(r->depth>=64)return NBA97_INTERRUPT_LIMIT;
        ++r->depth;rc=target==0x8007fc54u?dma_handle(r,returned):vblank_handle(r,returned);
        --r->depth;return rc;
    }
    return external(r,NBA97_INTERRUPT_CALLBACK,pc,target,0,0,0,0,0,returned);
}
static int vblank_handle(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t count,target;READ(0x8007ffa0u,0x800c7e70u,4,count);
    READ(0x8007ffa8u,0x800c7e6cu,4,target);WRITE(0x8007ffbcu,0x800c7e70u,4,count+1u);
    /* Callback is cached before increment, and the source rereads the live
     * counter even when an ensuing callback makes that value unused. */
    READ(0x8007ffc4u,0x800c7e70u,4,count);
    if(target)return callback(r,0x8007ffd0u,target,returned);
    *returned=known(count);return 1;
}
static int dma_handle(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t pointer,value,pending,channel,target;Nba97SpuTransferValue unused;
    READ(0x8007fc58u,0x800c7e5cu,4,pointer);DEVICE_READ(0x8007fc78u,pointer,4,value);
    pending=(value>>24)&0x7fu;
    while(pending) {
        channel=0;
        while(pending&&channel<7u) {
            if(pending&1u) {
                READ(0x8007fcc8u,0x800c7e5cu,4,pointer);DEVICE_READ(0x8007fcd0u,pointer,4,value);
                DEVICE_WRITE(0x8007fcdcu,pointer,4,value&(0x00ffffffu|(1u<<(channel+24u))));
                READ(0x8007fce4u,0x800c7e5cu,4,pointer);DEVICE_READ(0x8007fcecu,pointer,4,value);
                READ(0x8007fcf4u,0x800c7e3cu+channel*4u,4,target);
                if(target) { READ(0x8007fd04u,0x800c7e3cu+channel*4u,4,target);
                    TRY(callback(r,0x8007fd0cu,target,&unused)); }
            }
            pending>>=1;++channel;
        }
        READ(0x8007fd28u,0x800c7e5cu,4,pointer);DEVICE_READ(0x8007fd30u,pointer,4,value);
        pending=(value>>24)&0x7fu;
    }
    READ(0x8007fd4cu,0x800c7e5cu,4,pointer);DEVICE_READ(0x8007fd54u,pointer,4,value);
    if((value&0xff000000u)!=0x80000000u) {
        DEVICE_READ(0x8007fd6cu,pointer,4,value);value&=0x8000u;
        if(!value) { *returned=known(0);return 1; }
    }
    DEVICE_READ(0x8007fd80u,pointer,4,value);return diagnostic(r,0x80027ee8u,value,0,returned);
}
static int pending_irq(Run* r,int again,uint32_t* pending) {
    uint32_t cache,status_pointer,mask_pointer,status,mask;
    READ(again?0x8007f8acu:0x8007f7f8u,0x800c7df8u,4,cache);
    READ(again?0x8007f8b4u:0x8007f800u,0x800c7e2cu,4,status_pointer);
    READ(again?0x8007f8bcu:0x8007f808u,0x800c7e30u,4,mask_pointer);
    DEVICE_READ(again?0x8007f8c0u:0x8007f80cu,status_pointer,2,status);
    DEVICE_READ(again?0x8007f8c4u:0x8007f810u,mask_pointer,2,mask);
    *pending=cache&status&mask;return 1;
}
static int handle_irq(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t pending,channel,pointer,target,value,mask_pointer,status,mask,count;
    Nba97SpuTransferValue unused;WRITE(0x8007f7f0u,0x800c7dcau,2,1);
    TRY(pending_irq(r,0,&pending));
    while(pending) {
        channel=0;
        while(pending&&channel<11u) {
            if(pending&1u) {
                READ(0x8007f850u,0x800c7e2cu,4,pointer);
                DEVICE_WRITE(0x8007f858u,pointer,2,~(1u<<channel));
                READ(0x8007f860u,0x800c7e2cu,4,pointer);DEVICE_READ(0x8007f868u,pointer,2,value);
                READ(0x8007f870u,0x800c7dccu+channel*4u,4,target);
                if(target) { READ(0x8007f880u,0x800c7dccu+channel*4u,4,target);
                    TRY(callback(r,0x8007f888u,target,&unused)); }
            }
            pending>>=1;++channel;
        }
        TRY(pending_irq(r,1,&pending));
    }
    READ(0x8007f8dcu,0x800c7e2cu,4,pointer);READ(0x8007f8e4u,0x800c7e30u,4,mask_pointer);
    DEVICE_READ(0x8007f8e8u,pointer,2,status);DEVICE_READ(0x8007f8ecu,mask_pointer,2,mask);
    if(status&mask) {
        READ(0x8007f904u,0x800c7e34u,4,count);WRITE(0x8007f918u,0x800c7e34u,4,count+1u);
        /* Signed test uses the OLD counter. Negative/wrapped counts also
         * defer the original diagnostic and interrupt masking. */
        if(!(count&0x80000000u)&&count>=0x101u) {
            DEVICE_READ(0x8007f92cu,pointer,2,status);DEVICE_READ(0x8007f930u,mask_pointer,2,mask);
            TRY(diagnostic(r,0x80027eccu,status,mask,&unused));
            READ(0x8007f940u,0x800c7e2cu,4,pointer);READ(0x8007f948u,0x800c7e30u,4,mask_pointer);
            DEVICE_READ(0x8007f94cu,pointer,2,status);DEVICE_READ(0x8007f950u,mask_pointer,2,mask);
            DEVICE_WRITE(0x8007f95cu,mask_pointer,2,mask&~status);
            READ(0x8007f964u,0x800c7e2cu,4,pointer);WRITE(0x8007f96cu,0x800c7e34u,4,0);
            DEVICE_WRITE(0x8007f970u,pointer,2,0);
        }
    } else WRITE(0x8007f980u,0x800c7e34u,4,0);
    WRITE(0x8007f98cu,0x800c7dcau,2,0);return bios(r,NBA97_INTERRUPT_RETURN_EXCEPTION,0,0,returned);
}
static int initialize(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t ready,pointer;Nba97SpuTransferValue value,unused;
    READ(0x8007f71cu,0x800c7dc8u,2,ready);if(ready) { *returned=known(ready);return 1; }
    TRY(bios(r,NBA97_INTERRUPT_ENTER_CRITICAL,1,0,&unused));
    TRY(clear_words(r,0x8007fb5cu,0x800c7dc8u,25,&unused));
    TRY(bios(r,NBA97_INTERRUPT_CAPTURE_CONTEXT,0x800c7dfcu,0,&value));
    if(!value.known)return NBA97_PATL_RESOURCE;
    if(value.word)TRY(handle_irq(r,&unused));
    /* A real ReturnFromException transfers control out of this entire run.
     * Only an explicitly returning platform operation reaches the source's
     * fallthrough after that call; no invented successful return is supplied. */
    WRITE(0x8007f764u,0x800c7dc8u,2,1);TRY(vblank_initialize(r,&value));
    READ(0x8007f774u,0x800c7dc4u,4,pointer);WRITE(0x8007f77cu,pointer+0x14u,4,value.word);
    TRY(dma_initialize(r,&value));READ(0x8007f784u,0x800c7dc4u,4,pointer);
    WRITE(0x8007f78cu,pointer+4u,4,value.word);TRY(bios(r,NBA97_INTERRUPT_REMOVE_CDROM_DRIVER,0,0,&unused));
    WRITE(0x8007f7a0u,0x800c7e00u,4,0x800dad08u);
    TRY(bios(r,NBA97_INTERRUPT_HOOK_CONTEXT,0x800c7dfcu,0,&unused));
    return bios(r,NBA97_INTERRUPT_EXIT_CRITICAL,2,0,returned);
}
static int shutdown(Run* r,Nba97SpuTransferValue* returned) {
    uint32_t status_pointer,mask_pointer,status;Nba97SpuTransferValue unused;
    TRY(bios(r,NBA97_INTERRUPT_ENTER_CRITICAL,1,0,&unused));
    TRY(clear_words(r,0x8007fb5cu,0x800c7dc8u,25,&unused));
    READ(0x8007fb08u,0x800c7e2cu,4,status_pointer);READ(0x8007fb10u,0x800c7e30u,4,mask_pointer);
    DEVICE_WRITE(0x8007fb14u,status_pointer,2,0);DEVICE_READ(0x8007fb18u,status_pointer,2,status);
    DEVICE_WRITE(0x8007fb20u,mask_pointer,2,status);
    TRY(vblank_shutdown(r,&unused));TRY(dma_shutdown(r,&unused));
    return bios(r,NBA97_INTERRUPT_EXIT_CRITICAL,2,0,returned);
}
int nba97_interrupt_controller(Nba97InterruptController* owner,enum Nba97InterruptOperation op,
    uint32_t a0,uint32_t a1,Nba97SpuTransferValue incoming_v1,Nba97InterruptEvent* journal,size_t capacity,Nba97InterruptProgress* out) {
    Run run;int rc;Nba97SpuTransferValue returned={0,0};
    if(!owner||!out||(!journal&&capacity)||(!owner->memory.spans&&owner->memory.count)||
        op<NBA97_INTERRUPT_INITIALIZE_7F708||op>NBA97_INTERRUPT_RETURN_7FB98)return NBA97_PATL_ARGUMENT;
    memset(out,0,sizeof *out);run.owner=owner;run.out=out;run.journal=journal;run.capacity=capacity;run.depth=0;
    switch(op) {
    case NBA97_INTERRUPT_INITIALIZE_7F708:rc=initialize(&run,&returned);break;
    case NBA97_INTERRUPT_SHUTDOWN_7FAE4:rc=shutdown(&run,&returned);break;
    case NBA97_INTERRUPT_DISPATCH_7F600:rc=dispatch(&run,a0,a1,&returned);break;
    case NBA97_INTERRUPT_REGISTER_7F9BC:rc=register_callback(&run,a0,a1,&returned);break;
    case NBA97_INTERRUPT_HANDLE_7F7C8:rc=handle_irq(&run,&returned);break;
    case NBA97_INTERRUPT_DMA_INITIALIZE_7FBA8:rc=dma_initialize(&run,&returned);break;
    case NBA97_INTERRUPT_DMA_SHUTDOWN_7FC0C:rc=dma_shutdown(&run,&returned);break;
    case NBA97_INTERRUPT_DMA_HANDLE_7FC54:rc=dma_handle(&run,&returned);break;
    case NBA97_INTERRUPT_VBLANK_INITIALIZE_7FEEC:rc=vblank_initialize(&run,&returned);break;
    case NBA97_INTERRUPT_VBLANK_SHUTDOWN_7FF64:rc=vblank_shutdown(&run,&returned);break;
    case NBA97_INTERRUPT_VBLANK_HANDLE_7FF9C:rc=vblank_handle(&run,&returned);break;
    case NBA97_INTERRUPT_VBLANK_SET_7FFE8:rc=vblank_set(&run,a0,incoming_v1,&returned);break;
    case NBA97_INTERRUPT_CLEAR_7FB4C:rc=clear_words(&run,0x8007fb5cu,a0,a1,&returned);break;
    case NBA97_INTERRUPT_CLEAR_7FEC0:rc=clear_words(&run,0x8007fed0u,a0,a1,&returned);break;
    case NBA97_INTERRUPT_CLEAR_80020:rc=clear_words(&run,0x80080030u,a0,a1,&returned);break;
    default:rc=bios(&run,(enum Nba97InterruptKind)(NBA97_INTERRUPT_ENTER_CRITICAL+(op-NBA97_INTERRUPT_ENTER_7F308)),a0,a1,&returned);break;
    }
    if(rc==1) { out->returned=returned;out->completed=1;out->stopped_pc=0;out->stopped_address=0; }
    else if(rc==NBA97_INTERRUPT_TRANSFERRED) { out->completed=1;out->transferred=1; }
    return rc;
}
