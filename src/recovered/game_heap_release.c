#include "game_heap_release.h"
#include <string.h>

typedef struct Run {
    Nba97GameHeapReleaseContext* context;Nba97GameHeapReleaseStore* journal;
    size_t capacity;Nba97GameHeapReleaseProgress* out;
} Run;
#define TRY(expr) do {int rc_=(expr);if(rc_!=1)return rc_;}while(0)
#define READ(at,pc,v) TRY(read_word(r,(at),(pc),&(v)))
#define WRITE(at,pc,v) TRY(write_word(r,(at),(pc),(v)))
static void stop(Run* r,uint32_t pc,uint32_t at){r->out->stopped_pc=pc;r->out->stopped_address=at;}
static int access(Run* r,uint32_t at,uint32_t pc,uint8_t** data,uint8_t** known){
    size_t i,j;stop(r,pc,at);
    if(r->out->accesses>=r->context->access_budget)return NBA97_TEXT_LIMIT;
    ++r->out->accesses;if(at&3u)return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){Nba97GameTextRegion* b=&r->context->memory.region[i];
        uint64_t offset=(uint64_t)at-b->base;
        if(at<b->base||offset>b->size||4>b->size-(size_t)offset)continue;
        *data=b->data+(size_t)offset;*known=b->known?b->known+(size_t)offset:NULL;
        if(*known)for(j=0;j<4;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return 1;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read_word(Run* r,uint32_t at,uint32_t pc,uint32_t* value){
    uint8_t *data,*known;unsigned i;uint32_t v=0;TRY(access(r,at,pc,&data,&known));
    if(known)for(i=0;i<4;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<4;++i)v|=(uint32_t)data[i]<<(8*i);*value=v;return 1;
}
static int write_word(Run* r,uint32_t at,uint32_t pc,uint32_t value){
    uint8_t *data,*known;unsigned i;Nba97GameHeapReleaseStore* e;stop(r,pc,at);
    if(r->out->stores>=r->capacity)return NBA97_TEXT_LIMIT;
    TRY(access(r,at,pc,&data,&known));e=&r->journal[r->out->stores++];e->pc=pc;e->address=at;e->value=value;
    for(i=0;i<4;++i){data[i]=(uint8_t)(value>>(8*i));if(known)known[i]=1;}return 1;
}
static int find(Run* r,uint32_t payload,uint32_t* result){
    uint32_t bank,high,node,value,flags;
    for(bank=0;bank<16;++bank){
        READ(0x80103d54u+bank*24u,0x80090628u,high);if(!high)continue;
        READ(0x80103d50u+bank*24u,0x80090640u,node);
        do {READ(node+0x20u,0x80090648u,node);READ(node,0x80090650u,value);
            if(value==payload)break;
        }while(node!=high);
        /* The source checks flags even after an unmatched high sentinel.
         * A malformed sentinel without8000 is returned, not repaired. */
        READ(node+0x18u,0x80090668u,flags);
        if(!(flags&0x8000u)){*result=node;return 1;}
    }
    *result=0;return 1;
}
static int unlink_descriptor(Run* r,uint32_t descriptor,uint32_t* returned){
    uint32_t previous,next,head;
    READ(descriptor+0x24u,0x8009071cu,previous);READ(descriptor+0x20u,0x80090720u,next);
    WRITE(previous+0x20u,0x80090728u,next);
    /* Reread both links after the first store; aliases can redirect the next
     * destination. The original does not clear all descriptor fields. */
    READ(descriptor+0x20u,0x8009072cu,next);READ(descriptor+0x24u,0x80090730u,previous);
    WRITE(next+0x24u,0x80090738u,previous);WRITE(descriptor,0x80090740u,0);
    READ(0x800eb688u,0x80090d2cu,head);WRITE(0x800eb688u,0x80090d34u,descriptor);
    WRITE(descriptor+0x20u,0x80090d3cu,head);*returned=head;return 1;
}
int nba97_game_heap_release(Nba97GameHeapReleaseContext* context,enum Nba97GameHeapReleaseOperation operation,
    uint32_t address,Nba97GameHeapReleaseValue incomingv0,Nba97GameHeapReleaseStore* journal,
    size_t capacity,Nba97GameHeapReleaseProgress* out){
    Run run,*r=&run;uint32_t descriptor=address,lock,value;size_t i,j;
    if(!context||!out||(!journal&&capacity)||(!context->memory.region&&context->memory.count)||
        operation<NBA97_HEAP_FIND_90618||operation>NBA97_HEAP_UNLINK_90714||incomingv0.known>1)return NBA97_TEXT_ARGUMENT;
    for(i=0;i<context->memory.count;++i){const Nba97GameTextRegion* a=&context->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&context->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}
    }
    memset(out,0,sizeof *out);r->context=context;r->journal=journal;r->capacity=capacity;r->out=out;out->returned=incomingv0;
    if(operation==NBA97_HEAP_FIND_90618||(operation==NBA97_HEAP_RELEASE_PAYLOAD_90698&&address)){
        TRY(find(r,address,&descriptor));out->returned.word=descriptor;out->returned.known=1;
    }
    out->descriptor=descriptor;
    if(operation==NBA97_HEAP_UNLINK_90714){
        TRY(unlink_descriptor(r,descriptor,&value));out->returned.word=value;out->returned.known=1;
    }else if(operation!=NBA97_HEAP_FIND_90618&&descriptor){
        READ(0x801029c0u,0x800906dcu,lock);
        /*906C4 calls these lock setters unconditionally, including NULL.
         * Unlink aliases may replace1029C0 before its second load. */
        WRITE(lock,0x800a4064u,1);TRY(unlink_descriptor(r,descriptor,&value));
        READ(0x801029c0u,0x800906f4u,lock);WRITE(lock,0x800a408cu,0);
        out->returned.word=value;out->returned.known=1;
    }
    out->completed=1;out->stopped_pc=0;out->stopped_address=0;return 1;
}
