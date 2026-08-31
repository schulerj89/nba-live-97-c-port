#include "game_text_pools.h"
#include <string.h>
typedef struct Run {Nba97GameTextPoolContext* context;Nba97GameTextPoolEvent* journal;size_t capacity;Nba97GameTextPoolProgress* out;} Run;
#define TRY(expr) do{int result_=(expr);if(result_!=NBA97_TEXT_COMPLETE)return result_;}while(0)
static int32_t s16(uint32_t v){v&=65535u;return v<32768u?(int32_t)v:(int32_t)v-65536;}
static void stop(Run* r,uint32_t pc,uint32_t address){r->out->stopped_pc=pc;r->out->stopped_address=address;}
static int access(Run* r,uint32_t address,unsigned width,uint32_t pc,uint8_t** bytes,uint8_t** known){
    size_t i,j;stop(r,pc,address);
    if(address&(width-1u))return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){
        Nba97GameTextRegion* region=&r->context->memory.region[i];uint64_t offset=(uint64_t)address-region->base;
        if(address<region->base||offset>region->size||width>region->size-(size_t)offset)continue;
        *bytes=region->data+(size_t)offset;*known=region->known?region->known+(size_t)offset:NULL;
        if(*known)for(j=0;j<width;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read32(Run* r,uint32_t address,uint32_t pc,uint32_t* value){
    uint8_t *bytes,*known;unsigned i;uint32_t v=0;
    TRY(access(r,address,4,pc,&bytes,&known));
    if(known)for(i=0;i<4;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<4;++i)v|=(uint32_t)bytes[i]<<(i*8);
    *value=v;return NBA97_TEXT_COMPLETE;
}
static int reserve(Run* r,uint32_t pc,uint32_t address){
    stop(r,pc,address);return r->out->events<r->capacity?NBA97_TEXT_COMPLETE:NBA97_TEXT_LIMIT;
}
static int store(Run* r,uint32_t address,unsigned width,uint32_t value,uint32_t pc){
    uint8_t *bytes,*known;unsigned i;Nba97GameTextPoolEvent* e;
    TRY(reserve(r,pc,address));TRY(access(r,address,width,pc,&bytes,&known));
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);e->kind=NBA97_TEXT_POOL_STORE;
    e->pc=pc;e->address=address;e->width=(uint8_t)width;e->completed=1;
    e->value=width==1?value&255u:width==2?value&65535u:value;
    for(i=0;i<width;++i){bytes[i]=(uint8_t)(value>>(i*8));if(known)known[i]=1;}
    ++r->out->stores;return NBA97_TEXT_COMPLETE;
}
static int allocation(Run* r,uint32_t name,uint32_t size,uint32_t flags,uint32_t* style){
    uint32_t lock;Nba97GameTextPoolValue returned={0,0};Nba97GameTextPoolEvent* e;
    /*901EC rereads this global after the allocation. The second lock target
     * need not be the first target, and may alias the returned descriptor. */
    TRY(read32(r,0x801029c0u,0x800901fcu,&lock));
    if(lock)TRY(store(r,lock,4,1,0x800a4064u));
    TRY(reserve(r,0x80090234u,0));
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);e->kind=NBA97_TEXT_POOL_ALLOCATE_9027C;e->pc=0x80090234u;
    e->argument[0]=name;e->argument[1]=size;e->argument[2]=flags;e->argument[3]=1;
    if(!r->context->io||r->context->io(r->context->user,&r->context->memory,e,&returned)!=1)return NBA97_TEXT_IO_REFUSED;
    if(returned.known>1)return NBA97_TEXT_ARGUMENT;
    e->completed=1;e->returned=returned;r->out->allocation_descriptor=returned;++r->out->callbacks_completed;
    TRY(read32(r,0x801029c0u,0x80090240u,&lock));
    if(lock)TRY(store(r,lock,4,0,0x800a408cu));
    stop(r,0x80090170u,returned.known?returned.word:0);
    if(!returned.known)return NBA97_TEXT_UNKNOWN;
    /* Original90160 does LW0 from the returned descriptor WITHOUT a NULL
     * check.2E200 likewise stores through a NULL style if word0 was zero.
     * Retained-memory bounds can refuse, but no successful fallback/repair. */
    TRY(read32(r,returned.word,0x80090170u,style));
    r->out->style.word=*style;r->out->style.known=1;return NBA97_TEXT_COMPLETE;
}
static int validate(Nba97GameTextPoolContext* context,Nba97GameTextPoolEvent* journal,size_t capacity,Nba97GameTextPoolProgress* out){
    size_t i,j;
    if(!context||!out||(!journal&&capacity)||(!context->memory.region&&context->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<context->memory.count;++i){const Nba97GameTextRegion* a=&context->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&context->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}
    }
    return NBA97_TEXT_COMPLETE;
}
int nba97_game_allocate_payload_90160(Nba97GameTextPoolContext* context,uint32_t name,uint32_t size,uint32_t flags,
    Nba97GameTextPoolEvent* journal,size_t capacity,Nba97GameTextPoolProgress* out){
    Run r;uint32_t payload;
    TRY(validate(context,journal,capacity,out));
    memset(out,0,sizeof *out);r.context=context;r.journal=journal;r.capacity=capacity;r.out=out;out->requested_size=size;
    TRY(allocation(&r,name,size,flags,&payload));out->return_v0=payload;out->completed=1;return NBA97_TEXT_COMPLETE;
}
int nba97_game_text_pools(Nba97GameTextPoolContext* context,const Nba97GameTextPoolArguments* arguments,
    Nba97GameTextPoolEvent* journal,size_t capacity,Nba97GameTextPoolProgress* out){
    Run run,*r=&run;Nba97GameTextPoolArguments args;uint32_t glyph_span,font_span,text_span,id_span,packet_span,size,style,base,index,font_slots,text_slots;
    int32_t ids,packets;size_t i;
    static const uint8_t minus_one_offsets[]={0x2c,0x2e,0x3c,0x38,0x34,0x30,0x3e,0x3a,0x36,0x32};
    static const uint32_t minus_one_pcs[]={0x8002e2ecu,0x8002e2f0u,0x8002e2f4u,0x8002e2f8u,0x8002e2fcu,0x8002e300u,0x8002e304u,0x8002e308u,0x8002e30cu,0x8002e310u};
    if(!arguments)return NBA97_TEXT_ARGUMENT;
    TRY(validate(context,journal,capacity,out));
    memset(out,0,sizeof *out);r->context=context;r->journal=journal;r->capacity=capacity;r->out=out;args=*arguments;
    /* Raw argument6 is never used by the original. Mixed signed16/unsigned8
     * sizes are intentional, including negative sizes and modular sums. */
    ids=s16(args.id_capacity);packets=s16(args.packet_capacity);
    glyph_span=(uint32_t)s16(args.glyph_capacity)*20u;font_span=(args.font_count&255u)<<9;
    text_slots=args.text_capacity&255u;text_span=text_slots<<6;
    id_span=(uint32_t)ids<<1;packet_span=(uint32_t)packets*160u;
    size=0x58u+glyph_span+font_span+text_span+id_span+packet_span+(uint32_t)packets;out->requested_size=size;
    TRY(allocation(r,args.name,size,0x20,&style));base=style+0x58u;
    TRY(store(r,style+0x18u,4,base,0x8002e2b4u));base+=packet_span;
    TRY(store(r,style+8u,4,base,0x8002e2bcu));base+=glyph_span;
    TRY(store(r,style+0x10u,4,base,0x8002e2c4u));base+=text_span;
    TRY(store(r,style+0xcu,4,base,0x8002e2ccu));base+=font_span;
    TRY(store(r,style+0x14u,4,base,0x8002e2d4u));base+=id_span;
    TRY(store(r,style+0x1cu,4,base,0x8002e2dcu));
    TRY(store(r,style+0x28u,2,0x7fff,0x8002e2e4u));
    for(i=0;i<10;++i)TRY(store(r,style+minus_one_offsets[i],2,0xffff,minus_one_pcs[i]));
    TRY(store(r,style+0x26u,2,0,0x8002e31cu));TRY(store(r,style+0x2au,2,0,0x8002e320u));
    TRY(store(r,style+0x24u,2,0,0x8002e324u));TRY(store(r,style+0x52u,1,args.mode,0x8002e328u));
    TRY(store(r,style+0x22u,2,text_slots,0x8002e32cu));TRY(store(r,style+0x53u,1,0,0x8002e330u));
    TRY(store(r,style+0x40u,2,0,0x8002e338u));
    font_slots=(args.font_count&255u)<<8;
    /* These pointer words are live each iteration. Aliasing writes can change
     * a later destination; do not replace the loops with detached memset. */
    for(index=0;index<font_slots;++index){TRY(read32(r,style+0xcu,0x8002e340u,&base));TRY(store(r,base+(index<<1),2,0xffff,0x8002e358u));}
    for(index=0;ids>0&&index<(uint32_t)ids;++index){TRY(read32(r,style+0x14u,0x8002e37cu,&base));TRY(store(r,base+(index<<1),2,0xffff,0x8002e394u));}
    for(index=0;index<text_slots;++index){TRY(read32(r,style+0x10u,0x8002e3a8u,&base));TRY(store(r,base+(index<<6)+0x12u,2,0xffff,0x8002e3c0u));}
    /* Original global publication precedes the remaining capacity/bitmap
     * writes. A later refusal preserves this partially initialized style. */
    TRY(store(r,0x800b2048u,4,style,0x8002e3d4u));TRY(store(r,style+0x20u,2,args.packet_capacity,0x8002e3dcu));
    for(index=0;packets>0&&index<(uint32_t)packets;++index){TRY(read32(r,style+0x1cu,0x8002e3e4u,&base));TRY(store(r,base+index,1,0,0x8002e3f4u));}
    out->return_v0=packets>0?0:(uint32_t)packets;out->completed=1;out->stopped_pc=0;out->stopped_address=0;return NBA97_TEXT_COMPLETE;
}
