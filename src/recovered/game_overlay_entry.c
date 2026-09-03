#include "game_overlay_entry.h"
#include <string.h>

static const uint32_t BSS_BEGIN=0x800d7bb8u;
static const uint32_t BSS_END=0x8010b61cu;
static const uint32_t MEMORY_TOP=0x800c4b3cu;
static const uint32_t STACK_RESERVE=0x800c4b38u;
static const uint32_t HEAP_BASE_GLOBAL=0x800c4b18u;
static const uint32_t HEAP_SIZE_GLOBAL=0x800c4b1cu;
static const uint32_t SOURCE_GP=0x800d79c8u;
typedef struct Run {
    Nba97GameOverlayEntryContext* context;
    Nba97GameOverlayEntryProgress* out;
} Run;
#define TRY(expr) do {int rc_=(expr);if(rc_!=NBA97_TEXT_COMPLETE)return rc_;}while(0)

static void stop(Run* r,uint32_t pc,uint32_t address,uint32_t entry){
    r->out->stopped_pc=pc;r->out->stopped_address=address;r->out->stopped_entry=entry;
}
static int locate(Run* r,uint32_t address,uint32_t pc,uint8_t** data,uint8_t** known){
    size_t i,j;stop(r,pc,address,0);
    if(r->out->accesses>=r->context->access_budget)return NBA97_TEXT_LIMIT;
    ++r->out->accesses;
    if(address&3u)return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){
        Nba97GameTextRegion* region=&r->context->memory.region[i];
        uint64_t offset=(uint64_t)address-region->base;
        if(address<region->base||offset>region->size||4u>region->size-(size_t)offset)continue;
        *data=region->data+(size_t)offset;
        *known=region->known?region->known+(size_t)offset:0;
        if(*known)for(j=0;j<4;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read_word(Run* r,uint32_t address,uint32_t pc,uint32_t* value){
    uint8_t *data,*known;unsigned i;uint32_t result=0;TRY(locate(r,address,pc,&data,&known));
    if(known)for(i=0;i<4;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<4;++i)result|=(uint32_t)data[i]<<(i*8);
    *value=result;
    return NBA97_TEXT_COMPLETE;
}
static int write_word(Run* r,uint32_t address,uint32_t pc,uint32_t value){
    uint8_t *data,*known;unsigned i;TRY(locate(r,address,pc,&data,&known));
    for(i=0;i<4;++i){data[i]=(uint8_t)(value>>(i*8));if(known)known[i]=1;}
    ++r->out->stores;return NBA97_TEXT_COMPLETE;
}
static int invoke(Run* r,const Nba97GameOverlayEntryEvent* event,
    enum Nba97GameOverlayEntryCalleeOutcome* outcome){
    int result;stop(r,event->pc,0,event->entry);
    if(!r->context->io)return NBA97_TEXT_IO_REFUSED;
    *outcome=NBA97_GAME_OVERLAY_CALLEE_UNSET;
    result=r->context->io(r->context->user,&r->context->memory,event,outcome);
    if(result!=1)return NBA97_TEXT_IO_REFUSED;
    if(*outcome!=NBA97_GAME_OVERLAY_CALLEE_RETURNED&&
        *outcome!=NBA97_GAME_OVERLAY_CALLEE_TRANSFERRED)return NBA97_TEXT_ARGUMENT;
    ++r->out->callbacks_completed;return NBA97_TEXT_COMPLETE;
}
static int validate(Nba97GameOverlayEntryContext* context,
    Nba97GameOverlayEntryProgress* out,Run* run){
    size_t i,j;if(!out)return NBA97_TEXT_ARGUMENT;memset(out,0,sizeof *out);
    if(!context||(!context->memory.region&&context->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<context->memory.count;++i){
        const Nba97GameTextRegion* a=&context->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||
            (uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&context->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&
                (uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}
    }
    run->context=context;run->out=out;return NBA97_TEXT_COMPLETE;
}

int nba97_game_overlay_entry(Nba97GameOverlayEntryContext* context,
    Nba97GameOverlayEntryProgress* out){
    Run run,*r=&run;Nba97GameOverlayEntryEvent event;enum Nba97GameOverlayEntryCalleeOutcome outcome;
    uint32_t cursor,memory_top,stack_reserve,top_minus_eight,heap_offset,restored_ra;
    TRY(validate(context,out,r));

    /* GAMEONLY 0x80094828: the store loop includes the saved-RA word and ends
     * immediately before 0x8010B61C. Its unsigned comparison is exact. */
    for(cursor=BSS_BEGIN;cursor<BSS_END;cursor+=4u){
        TRY(write_word(r,cursor,0x80094838u,0));++out->words_cleared;
    }
    TRY(read_word(r,MEMORY_TOP,0x80094850u,&memory_top));
    /* 0x80094858 is signed ADDI, not wrapping ADDIU. */
    if(memory_top>=0x80000000u&&memory_top<=0x80000007u){
        stop(r,0x80094858u,0,0);out->trapped=1;
        return NBA97_GAME_OVERLAY_ENTRY_ARITHMETIC_TRAP;
    }
    top_minus_eight=memory_top-8u;
    out->stack_pointer=top_minus_eight|0x80000000u;
    /* The source SLL/SRL pair strips the high three bits from 0x8010B61C. */
    heap_offset=0x0010b61cu;
    TRY(read_word(r,STACK_RESERVE,0x80094878u,&stack_reserve));
    out->heap_size=top_minus_eight-stack_reserve-heap_offset;
    out->heap_base=heap_offset|0x80000000u;
    TRY(write_word(r,HEAP_SIZE_GLOBAL,0x8009488cu,out->heap_size));
    TRY(write_word(r,HEAP_BASE_GLOBAL,0x80094898u,out->heap_base));
    out->saved_return_address=context->incoming_return_address;
    TRY(write_word(r,BSS_BEGIN,0x800948a0u,out->saved_return_address));
    out->global_pointer=SOURCE_GP;out->frame_pointer=out->stack_pointer;

    memset(&event,0,sizeof event);event.kind=NBA97_GAME_OVERLAY_BIOS_A0_39_INIT_HEAP;
    event.pc=0x800948b0u;event.entry=0x80098554u;event.argument_count=2;
    event.argument[0]=out->heap_base+4u;event.argument[1]=out->heap_size;
    event.stack_pointer=out->stack_pointer;event.frame_pointer=out->frame_pointer;
    event.global_pointer=out->global_pointer;event.return_address=0x800948b8u;
    TRY(invoke(r,&event,&outcome));
    if(outcome!=NBA97_GAME_OVERLAY_CALLEE_RETURNED)return NBA97_TEXT_ARGUMENT;

    /* 0x800948BC reloads the live saved word after BIOS InitHeap returns. */
    TRY(read_word(r,BSS_BEGIN,0x800948bcu,&restored_ra));
    out->restored_return_address=restored_ra;
    memset(&event,0,sizeof event);event.kind=NBA97_GAME_OVERLAY_MAIN_29994;
    event.pc=0x800948c4u;event.entry=0x80029994u;
    event.stack_pointer=out->stack_pointer;event.frame_pointer=out->frame_pointer;
    event.global_pointer=out->global_pointer;event.return_address=0x800948ccu;
    TRY(invoke(r,&event,&outcome));out->entered_main=1;
    if(outcome==NBA97_GAME_OVERLAY_CALLEE_RETURNED){
        stop(r,0x800948ccu,0,0);out->trapped=1;
        return NBA97_GAME_OVERLAY_ENTRY_BREAK_TRAP;
    }
    out->transferred=1;out->completed=1;stop(r,0,0,0);
    return NBA97_TEXT_COMPLETE;
}
