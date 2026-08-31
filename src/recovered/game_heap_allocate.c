#include "game_heap_allocate.h"
#include <string.h>

typedef struct Run {Nba97GameHeapContext* context;Nba97GameHeapEvent* journal;size_t capacity;Nba97GameHeapProgress* out;} Run;
#define TRY(expr) do{int result_=(expr);if(result_!=NBA97_TEXT_COMPLETE)return result_;}while(0)
#define READ(at,pc,to) TRY(read_value(r,(at),4,(pc),&(to)))
#define WRITE(at,pc,val) TRY(write_value(r,(at),4,(val),(pc)))
static int signed_less(uint32_t a,uint32_t b){return (a^0x80000000u)<(b^0x80000000u);}
static void stop(Run* r,uint32_t pc,uint32_t at){r->out->stopped_pc=pc;r->out->stopped_address=at;}
static int access(Run* r,uint32_t at,unsigned width,uint32_t pc,uint8_t** data,uint8_t** known){
    size_t i,j;stop(r,pc,at);
    if(r->out->accesses>=r->context->access_budget)return NBA97_TEXT_LIMIT;
    ++r->out->accesses;
    if(at&(width-1u))return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){Nba97GameTextRegion* region=&r->context->memory.region[i];
        uint64_t offset=(uint64_t)at-region->base;
        if(at<region->base||offset>region->size||width>region->size-(size_t)offset)continue;
        *data=region->data+(size_t)offset;*known=region->known?region->known+(size_t)offset:NULL;
        if(*known)for(j=0;j<width;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read_value(Run* r,uint32_t at,unsigned width,uint32_t pc,uint32_t* value){
    uint8_t *data,*known;unsigned i;uint32_t v=0;TRY(access(r,at,width,pc,&data,&known));
    if(known)for(i=0;i<width;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<width;++i)v|=(uint32_t)data[i]<<(8*i);
    *value=v;return NBA97_TEXT_COMPLETE;
}
static int reserve(Run* r,uint32_t pc,uint32_t at){stop(r,pc,at);return r->out->events<r->capacity?NBA97_TEXT_COMPLETE:NBA97_TEXT_LIMIT;}
static int write_value(Run* r,uint32_t at,unsigned width,uint32_t value,uint32_t pc){
    uint8_t *data,*known;unsigned i;Nba97GameHeapEvent* e;
    TRY(reserve(r,pc,at));TRY(access(r,at,width,pc,&data,&known));
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);e->pc=pc;e->address=at;e->value=width==1?value&255u:value;
    e->kind=NBA97_HEAP_STORE;e->width=(uint8_t)width;e->completed=1;
    for(i=0;i<width;++i){data[i]=(uint8_t)(value>>(8*i));if(known)known[i]=1;}
    ++r->out->stores;return NBA97_TEXT_COMPLETE;
}
static int callback(Run* r,uint8_t kind,uint32_t pc,uint32_t a0,uint32_t a1,uint32_t a2,Nba97GameHeapValue* returned){
    Nba97GameHeapEvent* e;TRY(reserve(r,pc,0));
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);e->kind=kind;e->pc=pc;e->argument[0]=a0;e->argument[1]=a1;e->argument[2]=a2;
    returned->word=0;returned->known=0;
    if(!r->context->io||r->context->io(r->context->user,&r->context->memory,e,returned)!=1)return NBA97_TEXT_IO_REFUSED;
    if(returned->known>1)return NBA97_TEXT_ARGUMENT;
    e->returned=*returned;e->completed=1;++r->out->callbacks_completed;return NBA97_TEXT_COMPLETE;
}
static int basename(Run* r,uint32_t name,uint32_t* result){
    uint32_t cursor=name,value;*result=name;
    TRY(read_value(r,cursor,1,0x800a7098u,&value));if(!value)return NBA97_TEXT_COMPLETE;
    /* Original firstbyte is loaded twice. Subsequent iterations reuse the
     * load at70DC; do not add a detached string scan or whole-span preflight. */
    TRY(read_value(r,cursor,1,0x800a70b4u,&value));
    do {if(value==92||value==58||value==47)*result=cursor+1u;
        ++cursor;TRY(read_value(r,cursor,1,0x800a70dcu,&value));
    }while(value);
    return NBA97_TEXT_COMPLETE;
}
static int pop_descriptor(Run* r,uint32_t* descriptor){
    uint32_t next;READ(0x800eb688u,0x80090d44u,*descriptor);
    /* Original empty-free-list case dereferences NULL+20 without a check. */
    READ(*descriptor+0x20u,0x80090d4cu,next);WRITE(0x800eb688u,0x80090d54u,next);return NBA97_TEXT_COMPLETE;
}
static int guard(Run* r,uint32_t descriptor){
    uint32_t flags,base,size,index;READ(descriptor+0x18u,0x800a54c4u,flags);
    READ(descriptor,0x800a54c8u,base);READ(descriptor+0x14u,0x800a54ccu,size);
    WRITE(descriptor+0x18u,0x800a54dcu,flags|0x4000u);
    /* AA06C writes the big-endian BEND marker from the last byte backwards.
     * It does not clear the allocated payload or preserve overlappingfields. */
    for(index=0;index<4;++index)TRY(write_value(r,base+size+3u-index,1,0x42454e44u>>(8*index),0x800aa078u));
    return NBA97_TEXT_COMPLETE;
}
int nba97_game_heap_allocate(Nba97GameHeapContext* context,const Nba97GameHeapArguments* arguments,
    Nba97GameHeapEvent* journal,size_t capacity,Nba97GameHeapProgress* out){
    Run run,*r=&run;Nba97GameHeapArguments args;Nba97GameHeapValue returned;
    uint32_t heap,name,padding,mask,aligned,position,node,descriptor,serial,value,other,gap=0,align_flag;size_t i,j;
    if(!context||!arguments||!out||(!journal&&capacity)||(!context->memory.region&&context->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<context->memory.count;++i){const Nba97GameTextRegion* a=&context->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&context->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}
    }
    memset(out,0,sizeof *out);r->context=context;r->journal=journal;r->capacity=capacity;r->out=out;args=*arguments;
    heap=0x80103d50u+((args.flags>>8)&15u)*24u;out->heap_context=heap;
    TRY(basename(r,args.name,&name));align_flag=args.flags&0x40u;
    if(align_flag){READ(heap+0x14u,0x800902e0u,padding);READ(heap+0xcu,0x800902e4u,mask);}
    else {READ(heap+0x14u,0x800902f0u,padding);READ(heap+8u,0x800902f4u,mask);}
    aligned=(args.size+padding+mask)&~mask;out->aligned_size=aligned;
    READ(heap,0x80090308u,value);if(!value)goto null_return;
    if(args.flags&0x20u)goto reverse_start;

forward_start:
    READ(heap,0x80090320u,value);READ(value+0x20u,0x80090328u,node);READ(value,0x8009032cu,position);
    if(align_flag){READ(heap+0xcu,0x80090338u,mask);position=(position+mask)&~mask;}
forward_outer:
    READ(node,0x8009034cu,value);gap=0;goto forward_gap;
forward_skip:
    READ(node,0x80090358u,value);READ(node+0x10u,0x8009035cu,other);position=value+other;
    if(align_flag){READ(heap+0xcu,0x80090368u,mask);position=(position+mask)&~mask;}
    READ(heap+4u,0x8009037cu,value);if(node==value)goto forward_choose;
    READ(node+0x20u,0x8009038cu,node);READ(node,0x80090394u,value);
forward_gap:
    if(position>=value)goto forward_skip;
    gap=value-position;
forward_choose:
    /* Source compares the modular gap and requested aligned size SIGNED.
     * Wrapped/negative sizes can therefore allocate overlapping ranges. */
    if(signed_less(gap,aligned))goto forward_insufficient;
    TRY(pop_descriptor(r,&descriptor));READ(0x800c4a8cu,0x800903c8u,serial);
    WRITE(descriptor+0x18u,0x800903d8u,args.flags);WRITE(descriptor+0x14u,0x800903dcu,args.size);
    WRITE(descriptor+0x10u,0x800903e0u,aligned);WRITE(0x800c4a8cu,0x800903ecu,serial+1u);
    WRITE(descriptor+0x1cu,0x800903f4u,serial);
    TRY(callback(r,NBA97_HEAP_BIOS_A0_1A,0x8009d940u,descriptor+4u,name,12,&returned));
    WRITE(descriptor,0x800903f8u,position);READ(node+0x24u,0x800903fcu,value);
    WRITE(descriptor+0x20u,0x80090400u,node);WRITE(descriptor+0x24u,0x80090404u,value);
    READ(node+0x24u,0x80090408u,value);WRITE(value+0x20u,0x80090410u,descriptor);
    WRITE(node+0x24u,0x80090418u,descriptor);goto finish_descriptor;
forward_insufficient:
    READ(heap+4u,0x8009041cu,value);if(node==value)goto forward_reclaim;
    READ(node,0x8009042cu,value);READ(node+0x10u,0x80090430u,other);position=value+other;
    if(align_flag){READ(heap+0xcu,0x8009043cu,mask);position=(position+mask)&~mask;}
    READ(node+0x20u,0x80090450u,node);goto forward_outer;
forward_reclaim:
    TRY(callback(r,NBA97_HEAP_RECLAIM_A3074,0x8009045cu,args.flags,0,0,&returned));
    stop(r,0x80090464u,0);if(!returned.known)return NBA97_TEXT_UNKNOWN;
    if(returned.word)goto forward_start;
    goto null_return;

reverse_start:
    READ(heap+4u,0x80090474u,value);READ(value+0x24u,0x8009047cu,node);READ(value,0x80090480u,position);gap=0;
    if(align_flag){READ(heap+0xcu,0x8009048cu,mask);position&=~mask;}
reverse_scan:
    READ(node,0x8009049cu,value);READ(node+0x10u,0x800904a0u,other);other+=value;
    if(align_flag){READ(heap+0xcu,0x800904acu,mask);other=(other+mask)&~mask;}
    if(other<position){gap=position-other;goto reverse_choose;}
    position=value;if(align_flag){READ(heap+0xcu,0x800904d4u,mask);position&=~mask;}
    READ(heap,0x800904e4u,value);if(node==value)goto reverse_choose;
    READ(node+0x24u,0x800904f4u,node);goto reverse_scan;
reverse_choose:
    if(signed_less(gap,aligned))goto reverse_insufficient;
    TRY(pop_descriptor(r,&descriptor));READ(0x800c4a8cu,0x8009051cu,serial);
    WRITE(descriptor+0x18u,0x8009052cu,args.flags);WRITE(descriptor+0x14u,0x80090530u,args.size);
    WRITE(descriptor+0x10u,0x80090534u,aligned);WRITE(0x800c4a8cu,0x80090540u,serial+1u);
    WRITE(descriptor+0x1cu,0x80090548u,serial);
    TRY(callback(r,NBA97_HEAP_BIOS_A0_1A,0x8009d940u,descriptor+4u,name,12,&returned));
    WRITE(descriptor,0x80090550u,position-aligned);WRITE(descriptor+0x24u,0x80090554u,node);
    READ(node+0x20u,0x80090558u,value);WRITE(descriptor+0x20u,0x80090560u,value);
    READ(node+0x20u,0x80090564u,value);WRITE(value+0x24u,0x8009056cu,descriptor);
    WRITE(node+0x20u,0x80090570u,descriptor);goto finish_descriptor;
reverse_insufficient:
    READ(heap,0x8009059cu,value);if(node==value)goto reverse_reclaim;
    READ(node,0x800905acu,position);if(align_flag){READ(heap+0xcu,0x800905b8u,mask);position&=~mask;}
    READ(node+0x24u,0x800905c8u,node);gap=0;goto reverse_scan;
reverse_reclaim:
    TRY(callback(r,NBA97_HEAP_RECLAIM_A3074,0x800905d4u,args.flags,0,0,&returned));
    stop(r,0x800905dcu,0);if(!returned.known)return NBA97_TEXT_UNKNOWN;
    if(returned.word)goto reverse_start;
    goto null_return;
finish_descriptor:
    /* Reread padding AFTER namecallback and list insertion. A callback/alias
     * can select or suppress guard writes without changing cachedsize. */
    READ(heap+0x14u,0x80090574u,value);if(value)TRY(guard(r,descriptor));
    out->descriptor.word=descriptor;goto complete;
null_return:
    out->descriptor.word=0;
complete:
    out->descriptor.known=1;out->completed=1;out->stopped_pc=0;out->stopped_address=0;return NBA97_TEXT_COMPLETE;
}
