#include "spu_heap.h"
#include <string.h>

#define LAST 0x800c7a88u
#define TABLE 0x800c7a8cu
#define CAPACITY 0x800c7a84u
#define ADDRESS_MASK 0x0fffffffu
#define TAIL 0x40000000u
#define FREE 0x80000000u
#define DELETED 0x2fffffffu
typedef struct Run {
    Nba97SpuHeap* heap;
    Nba97SpuHeapProgress* out;
    Nba97SpuHeapStore* journal;
    size_t capacity;
} Run;
static int32_t signed32(uint32_t v) { return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)~v; }
static uint32_t arithmetic_shift(uint32_t v,uint32_t n) {
    n&=31u;
    if(!n)return v;
    return (v>>n)|((v&0x80000000u)?(~0u<<(32u-n)):0u);
}
static int access(Run* r,uint32_t pc,uint32_t at) {
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->accesses>=r->heap->access_budget)return NBA97_SPU_HEAP_LIMIT;
    ++r->out->accesses;return 1;
}
static int read_word(Run* r,uint32_t pc,uint32_t at,uint32_t* value) {
    int rc=access(r,pc,at);
    return rc==1?nba97_voice_patl_read(&r->heap->memory,at,4,value):rc;
}
static int write_word(Run* r,uint32_t pc,uint32_t at,uint32_t value) {
    int rc;Nba97SpuHeapStore* e;
    r->out->stopped_pc=pc;r->out->stopped_address=at;
    if(r->out->stores>=r->capacity)return NBA97_SPU_HEAP_LIMIT;
    rc=access(r,pc,at);if(rc!=1)return rc;
    rc=nba97_voice_patl_write(&r->heap->memory,at,4,value);if(rc!=1)return rc;
    e=&r->journal[r->out->stores++];e->pc=pc;e->address=at;e->value=value;return 1;
}
#define TRY(expr) do { int rc_=(expr);if(rc_!=1)return rc_; } while(0)
#define READ(pc,at,v) TRY(read_word(r,(pc),(at),&(v)))
#define WRITE(pc,at,v) TRY(write_word(r,(pc),(at),(v)))

static int maintain(Run* r,uint32_t* returned) {
    uint32_t last,index,base,slot,next_index,next,word,next_word,size,next_size;
    uint32_t limit,candidate,old_word,old_size,value;
    READ(0x8007ef48u,LAST,last);
    if(signed32(last)>=0) {
        index=0;READ(0x8007ef6cu,TABLE,base);slot=base;
        do {
            READ(0x8007ef78u,slot,word);
            if(word&FREE) {
                next_index=index+1u;next=base+next_index*8u;
                /* Original skips deleted entries without checking the last
                 * index. A malformed chain reaches its actual read boundary. */
                for(;;) {
                    READ(0x8007ef94u,next,next_word);next+=8u;
                    if(next_word!=DELETED)break;
                    ++next_index;
                }
                next=base+next_index*8u;READ(0x8007efb4u,next,next_word);
                if(next_word&FREE) {
                    READ(0x8007efc8u,slot,word);READ(0x8007efccu,slot+4u,size);
                    if((next_word&ADDRESS_MASK)==(word&ADDRESS_MASK)+size) {
                        WRITE(0x8007efe0u,next,DELETED);
                        READ(0x8007efe4u,slot+4u,size);READ(0x8007efe8u,next+4u,next_size);
                        WRITE(0x8007eff8u,slot+4u,size+next_size);
                        goto merge_test;
                    }
                }
            }
            slot+=8u;++index;
merge_test:
            READ(0x8007f008u,LAST,last);
        } while(signed32(last)>=signed32(index));
        READ(0x8007f020u,LAST,last);
    }
    /* Mark zero-size records, using the count captured before this loop. */
    if(signed32(last)>=0) {
        index=0;READ(0x8007f040u,TABLE,slot);
        do {
            READ(0x8007f048u,slot+4u,size);++index;
            if(!size)WRITE(0x8007f058u,slot,DELETED);
            slot+=8u;
        } while(signed32(last)>=signed32(index));
    }
    /* Sort by the masked address, preserving each source reread after a
     * store. The descriptor storage can alias the live SDK globals. */
    READ(0x8007f06cu,LAST,last);
    if(signed32(last)>=0) {
        index=0;READ(0x8007f088u,TABLE,base);slot=base;
        do {
            READ(0x8007f094u,slot,word);if(word&TAIL)break;
            next_index=index+1u;
            if(signed32(last)>=signed32(next_index)) {
                READ(0x8007f0c0u,LAST,limit);candidate=base+next_index*8u;
                do {
                    READ(0x8007f0c8u,candidate,next_word);if(next_word&TAIL)break;
                    READ(0x8007f0dcu,slot,old_word);++next_index;
                    if((next_word&ADDRESS_MASK)<(old_word&ADDRESS_MASK)) {
                        WRITE(0x8007f0f4u,slot,next_word);
                        READ(0x8007f0f8u,candidate+4u,next_size);READ(0x8007f0fcu,slot+4u,old_size);
                        WRITE(0x8007f100u,slot+4u,next_size);WRITE(0x8007f104u,candidate,old_word);
                        WRITE(0x8007f108u,candidate+4u,old_size);
                    }
                    candidate+=8u;
                } while(signed32(limit)>=signed32(next_index));
            }
            READ(0x8007f11cu,LAST,last);++index;slot+=8u;
        } while(signed32(last)>=signed32(index));
    }
    READ(0x8007f134u,LAST,last);
    if(signed32(last)>=0) {
        index=0;READ(0x8007f150u,TABLE,base);slot=base;
        do {
            READ(0x8007f15cu,slot,word);if(word&TAIL)break;
            if(word==DELETED) {
                next=base+last*8u;READ(0x8007f17cu,next,next_word);
                WRITE(0x8007f184u,slot,next_word);READ(0x8007f188u,next+4u,next_size);
                WRITE(0x8007f190u,LAST,index);WRITE(0x8007f198u,slot+4u,next_size);break;
            }
            READ(0x8007f1a0u,LAST,last);++index;slot+=8u;
        } while(signed32(last)>=signed32(index));
    }
    /* Collapse free records at the end. The source does not check address
     * contiguity here, and its incidental v0 return is kept verbatim. */
    READ(0x8007f1b8u,LAST,last);index=last-1u;value=index*8u;
    if(signed32(index)>=0) {
        READ(0x8007f1e0u,TABLE,base);slot=base+value;
        do {
            READ(0x8007f1ecu,slot,word);value=word&ADDRESS_MASK;
            if(!(word&FREE))break;
            READ(0x8007f204u,LAST,last);value|=TAIL;WRITE(0x8007f20cu,slot,value);
            READ(0x8007f210u,slot+4u,value);WRITE(0x8007f218u,LAST,index);
            READ(0x8007f224u,base+last*8u+4u,next_size);--index;
            value+=next_size;WRITE(0x8007f230u,slot+4u,value);slot-=8u;
        } while(signed32(index)>=0);
    }
    *returned=value;return 1;
}
static int initialize(Run* r,uint32_t count,uint32_t table,uint32_t* returned) {
    uint32_t shift;
    if(signed32(count)<=0) { *returned=0;return 1; }
    READ(0x8007e958u,0x800c75ecu,shift);
    WRITE(0x8007e960u,table,TAIL|0x1010u);WRITE(0x8007e96cu,TABLE,table);
    WRITE(0x8007e974u,LAST,0);WRITE(0x8007e97cu,CAPACITY,count);
    WRITE(0x8007e988u,table+4u,(0x10000u<<(shift&31u))-0x1010u);
    *returned=count;return 1;
}
static int allocate(Run* r,uint32_t requested,uint32_t* returned) {
    uint32_t enabled,reserved=0,units,shift,mask,rounded,base,word,index=0;
    uint32_t selected=0xffffffffu,capacity,slot,size,next,next_word,next_size,last,unused;
    READ(0x8007ec30u,0x800c762cu,enabled);
    if(enabled) {
        READ(0x8007ec6cu,0x800c7630u,units);READ(0x8007ec74u,0x800c75ecu,shift);
        reserved=(0x10000u-units)<<(shift&31u);
    }
    READ(0x8007ec84u,0x800c75f4u,mask);
    /* Original tests the complement of the mask. With shift3/mask7,
     * requests1..7 round DOWN to zero; later compaction can make the raw
     * returned descriptor word include TAIL. Do not repair either behavior. */
    rounded=(requested&~mask)?requested+mask:requested;
    READ(0x8007eca4u,0x800c75ecu,shift);READ(0x8007ecacu,TABLE,base);
    rounded=arithmetic_shift(rounded,shift)<<(shift&31u);
    READ(0x8007ecb8u,base,word);
    if(word&TAIL)selected=0;
    else {
        TRY(maintain(r,&unused));READ(0x8007ece0u,CAPACITY,capacity);
        if(signed32(capacity)>0) {
            READ(0x8007ed04u,TABLE,base);slot=base;
            do {
                READ(0x8007ed10u,slot,word);
                if(word&TAIL) { selected=index;break; }
                if(word&FREE) {
                    READ(0x8007ed2cu,slot+4u,size);
                    if(size>=rounded) { selected=index;break; }
                }
                ++index;slot+=8u;
            } while(signed32(index)<signed32(capacity));
        }
    }
    *returned=0xffffffffu;if(selected==0xffffffffu)return 1;
    READ(0x8007ed68u,TABLE,base);slot=base+selected*8u;
    READ(0x8007ed74u,slot,word);
    if(word&TAIL) {
        READ(0x8007ed8cu,CAPACITY,capacity);
        if(signed32(selected)>=signed32(capacity))return 1;
        READ(0x8007eda0u,slot+4u,size);
        /* The subtraction is unsigned and may wrap; no reserve clamp or
         * zero-size special case is inserted. Init's count is not reduced. */
        if(size-reserved<rounded)return 1;
        next=base+(selected+1u)*8u;READ(0x8007edc8u,slot,word);
        WRITE(0x8007eddcu,next,((word&ADDRESS_MASK)+rounded)|TAIL);
        READ(0x8007ede0u,slot+4u,size);WRITE(0x8007edecu,next+4u,size-rounded);
        READ(0x8007edf0u,slot,word);WRITE(0x8007edf8u,LAST,selected+1u);
        WRITE(0x8007edfcu,slot+4u,rounded);WRITE(0x8007ee08u,slot,word&ADDRESS_MASK);
        TRY(maintain(r,&unused));READ(0x8007ee10u,TABLE,base);
        READ(0x8007ee1cu,base+selected*8u,*returned);
    } else {
        READ(0x8007ee28u,slot+4u,size);
        if(rounded<size) {
            READ(0x8007ee40u,LAST,last);READ(0x8007ee48u,CAPACITY,capacity);
            if(signed32(last)<signed32(capacity)) {
                next=base+last*8u;
                READ(0x8007ee6cu,next,next_word);READ(0x8007ee70u,next+4u,next_size);
                WRITE(0x8007ee74u,next,(rounded+word)|FREE);WRITE(0x8007ee7cu,next+4u,size-rounded);
                WRITE(0x8007ee88u,LAST,last+1u);WRITE(0x8007ee8cu,next+8u,next_word);
                WRITE(0x8007ee90u,next+12u,next_size);
            }
            /* With no descriptor capacity, the original still shrinks the
             * selected block below and loses the unrecorded remainder. */
        }
        READ(0x8007ee9cu,TABLE,base);slot=base+selected*8u;
        READ(0x8007eea8u,slot,word);WRITE(0x8007eeb0u,slot+4u,rounded);
        WRITE(0x8007eebcu,slot,word&ADDRESS_MASK);
        TRY(maintain(r,&unused));READ(0x8007eec4u,TABLE,base);
        READ(0x8007eed0u,base+selected*8u,*returned);
    }
    return 1;
}
static int release(Run* r,uint32_t address,uint32_t* returned) {
    uint32_t capacity,index=0,slot,word;
    READ(0x8007e574u,CAPACITY,capacity);
    if(signed32(capacity)>0) {
        READ(0x8007e598u,TABLE,slot);
        do {
            READ(0x8007e5a0u,slot,word);if(word&TAIL)break;
            ++index;
            if(word==address) { WRITE(0x8007e5c0u,slot,address|FREE);break; }
            slot+=8u;
        } while(signed32(index)<signed32(capacity));
    }
    /* Maintenance executes even if the address was not found or count<=0. */
    return maintain(r,returned);
}
int nba97_spu_heap(Nba97SpuHeap* heap,enum Nba97SpuHeapOperation operation,uint32_t a0,uint32_t a1,
    Nba97SpuHeapStore* journal,size_t capacity,Nba97SpuHeapProgress* out) {
    Run run;int rc;uint32_t value=0;
    if(!heap||!out||(!journal&&capacity)||(!heap->memory.spans&&heap->memory.count)||
        operation<NBA97_SPU_HEAP_INITIALIZE_7E940||operation>NBA97_SPU_HEAP_MAINTAIN_7EF44)return NBA97_PATL_ARGUMENT;
    memset(out,0,sizeof *out);run.heap=heap;run.out=out;run.journal=journal;run.capacity=capacity;
    switch(operation) {
    case NBA97_SPU_HEAP_INITIALIZE_7E940:rc=initialize(&run,a0,a1,&value);break;
    case NBA97_SPU_HEAP_ALLOCATE_7EC2C:rc=allocate(&run,a0,&value);break;
    case NBA97_SPU_HEAP_FREE_7E56C:rc=release(&run,a0,&value);break;
    default:rc=maintain(&run,&value);break;
    }
    if(rc==1) { out->completed=1;out->return_v0=value;out->stopped_pc=0;out->stopped_address=0; }
    return rc;
}
