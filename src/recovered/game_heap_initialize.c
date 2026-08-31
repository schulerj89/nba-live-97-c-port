#include "game_heap_initialize.h"
#include <string.h>

typedef struct Run {
    Nba97GameHeapInitializeContext* context;
    Nba97GameHeapInitializeEvent* journal;
    size_t capacity;
    Nba97GameHeapInitializeProgress* out;
} Run;
#define TRY(expr) do {int rc_=(expr);if(rc_!=NBA97_TEXT_COMPLETE)return rc_;}while(0)
#define READ(at,pc,v) TRY(read_word(r,(at),(pc),&(v)))
#define WRITE(at,pc,v) TRY(write_word(r,(at),(pc),(v)))
static void stop(Run* r,uint32_t pc,uint32_t address){r->out->stopped_pc=pc;r->out->stopped_address=address;}
static int access(Run* r,uint32_t address,uint32_t pc,uint8_t** bytes,uint8_t** known){
    size_t i,j;stop(r,pc,address);
    if(r->out->accesses>=r->context->access_budget)return NBA97_TEXT_LIMIT;
    ++r->out->accesses;
    if(address&3u)return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){
        Nba97GameTextRegion* region=&r->context->memory.region[i];uint64_t offset=(uint64_t)address-region->base;
        if(address<region->base||offset>region->size||4>region->size-(size_t)offset)continue;
        *bytes=region->data+(size_t)offset;*known=region->known?region->known+(size_t)offset:NULL;
        if(*known)for(j=0;j<4;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read_word(Run* r,uint32_t address,uint32_t pc,uint32_t* value){
    uint8_t *bytes,*known;unsigned i;uint32_t v=0;TRY(access(r,address,pc,&bytes,&known));
    if(known)for(i=0;i<4;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<4;++i)v|=(uint32_t)bytes[i]<<(i*8);*value=v;return NBA97_TEXT_COMPLETE;
}
static int reserve(Run* r,uint32_t pc,uint32_t address){
    stop(r,pc,address);return r->out->events<r->capacity?NBA97_TEXT_COMPLETE:NBA97_TEXT_LIMIT;
}
static int write_word(Run* r,uint32_t address,uint32_t pc,uint32_t value){
    uint8_t *bytes,*known;unsigned i;Nba97GameHeapInitializeEvent* e;
    TRY(reserve(r,pc,address));TRY(access(r,address,pc,&bytes,&known));
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->pc=pc;e->address=address;e->value=value;e->width=4;e->completed=1;
    for(i=0;i<4;++i){bytes[i]=(uint8_t)(value>>(i*8));if(known)known[i]=1;}
    ++r->out->stores;return NBA97_TEXT_COMPLETE;
}
static int format(Run* r,uint32_t pc,uint32_t destination,uint32_t pattern,uint32_t name){
    Nba97GameHeapInitializeEvent* e;TRY(reserve(r,pc,0));
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->kind=NBA97_HEAP_INITIALIZE_FORMAT_9CB7C;e->pc=pc;
    e->argument[0]=destination;e->argument[1]=pattern;e->argument[2]=name;
    if(!r->context->io||r->context->io(r->context->user,&r->context->memory,e)!=1)return NBA97_TEXT_IO_REFUSED;
    e->completed=1;++r->out->callbacks_completed;return NBA97_TEXT_COMPLETE;
}
static int pop(Run* r,uint32_t* descriptor){
    uint32_t next;READ(0x800eb688u,0x80090d44u,*descriptor);
    /*90D40 has no empty-list check. Preserve the dereference and prefix.*/
    READ(*descriptor+0x20u,0x80090d4cu,next);WRITE(0x800eb688u,0x80090d54u,next);return 1;
}
static int bank(Run* r,const Nba97GameHeapBankArguments* a){
    uint32_t heap=0x80103d50u+((a->flags>>8)&15u)*24u,descriptor,value;
    r->out->heap_bank=heap;
    WRITE(heap+8u,0x8008fbb4u,a->alignment-1u);
    WRITE(heap+0xcu,0x8008fbb8u,a->alternate_alignment-1u);
    WRITE(heap+0x10u,0x8008fbc0u,a->reclaim);
    if(a->guard){WRITE(heap+0x14u,0x8008fbccu,4);}
    else {WRITE(heap+0x14u,0x8008fbd0u,0);}
    TRY(pop(r,&descriptor));
    TRY(format(r,0x8008fbecu,descriptor+4u,0x80028034u,a->name));
    WRITE(descriptor,0x8008fbf8u,a->begin);
    WRITE(descriptor+0x14u,0x8008fbfcu,0);WRITE(descriptor+0x10u,0x8008fc00u,0);
    WRITE(descriptor+0x24u,0x8008fc04u,0);WRITE(descriptor+0x18u,0x8008fc08u,a->flags|0x8000u);
    WRITE(heap,0x8008fc10u,descriptor);
    TRY(pop(r,&descriptor));
    TRY(format(r,0x8008fc24u,descriptor+4u,0x80028040u,a->name));
    WRITE(descriptor,0x8008fc30u,a->end);
    WRITE(descriptor+0x14u,0x8008fc34u,0);WRITE(descriptor+0x10u,0x8008fc38u,0);
    WRITE(descriptor+0x20u,0x8008fc3cu,0);WRITE(descriptor+0x18u,0x8008fc40u,a->flags|0x8020u);
    /* The first pointer is loaded before publishing the second pointer;
     * source aliases may change the subsequent reload. Do not cache both. */
    READ(heap,0x8008fc44u,value);WRITE(heap+4u,0x8008fc48u,descriptor);
    WRITE(value+0x20u,0x8008fc4cu,descriptor);
    READ(heap,0x8008fc50u,value);WRITE(descriptor+0x24u,0x8008fc58u,value);
    r->out->return_v0=heap;return 1;
}
static int begin(Nba97GameHeapInitializeContext* c,Nba97GameHeapInitializeEvent* journal,
    size_t capacity,Nba97GameHeapInitializeProgress* out,Run* r){
    size_t i,j;
    if(!c||!out||(!journal&&capacity)||(!c->memory.region&&c->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<c->memory.count;++i){const Nba97GameTextRegion* a=&c->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&c->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}
    }
    memset(out,0,sizeof *out);r->context=c;r->journal=journal;r->capacity=capacity;r->out=out;return 1;
}
static int finish(Run* r){r->out->completed=1;r->out->stopped_pc=0;r->out->stopped_address=0;return 1;}
int nba97_game_heap_initialize_bank(Nba97GameHeapInitializeContext* c,const Nba97GameHeapBankArguments* arguments,
    Nba97GameHeapInitializeEvent* journal,size_t capacity,Nba97GameHeapInitializeProgress* out){
    Run run,*r=&run;Nba97GameHeapBankArguments args;
    if(!arguments)return NBA97_TEXT_ARGUMENT;args=*arguments;
    TRY(begin(c,journal,capacity,out,r));TRY(bank(r,&args));return finish(r);
}
int nba97_game_heap_initialize(Nba97GameHeapInitializeContext* c,const Nba97GameHeapInitializeArguments* arguments,
    Nba97GameHeapInitializeEvent* journal,size_t capacity,Nba97GameHeapInitializeProgress* out){
    Run run,*r=&run;Nba97GameHeapInitializeArguments args;Nba97GameHeapBankArguments b;
    uint32_t cursor,remaining,index=0,base,end,lock;
    if(!arguments)return NBA97_TEXT_ARGUMENT;args=*arguments;TRY(begin(c,journal,capacity,out,r));
    WRITE(args.gp+0x274u,0x800a4048u,0);
    WRITE(0x801029c0u,0x8008fa98u,0x800d7c3cu);
    WRITE(0x800d7c3cu,0x800a4064u,1);
    WRITE(0x800c4a84u,0x8008fab8u,args.arena);
    WRITE(0x800eb688u,0x80090cecu,args.arena);
    cursor=args.arena;remaining=args.descriptor_count-1u;
    /*90CE4 always writes the terminal link, even for zero/negative count.*/
    while(remaining&&remaining<0x80000000u&&index<remaining){
        WRITE(cursor+0x20u,0x80090d00u,cursor+40u);cursor+=40u;++index;
    }
    WRITE(cursor+0x20u,0x80090d18u,0);
    READ(0x800c4a84u,0x8008fac8u,base);end=args.arena+args.arena_size;
    WRITE(0x800c4a80u,0x8008fae8u,end);
    b.name=0x8002802cu;b.flags=0;b.begin=base+args.descriptor_count*40u;b.end=end;
    b.alignment=16;b.alternate_alignment=16;b.reclaim=0;b.guard=0;
    TRY(bank(r,&b));
    READ(0x800c4a80u,0x8008fb08u,end);READ(0x800c4a84u,0x8008fb10u,base);
    READ(0x801029c0u,0x8008fb18u,lock);
    WRITE(0x80109b8cu,0x8008fb24u,end-base);
    /* Unlike901EC, this entry calls the unlock setter even for a NULL target.*/
    WRITE(lock,0x800a408cu,0);
    out->return_v0=end-base;return finish(r);
}
