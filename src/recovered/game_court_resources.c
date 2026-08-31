#include "game_court_resources.h"
#include <string.h>
typedef struct Run {Nba97GameTextPoolContext* context;size_t budget,capacity;Nba97GameTextPoolEvent* journal;Nba97GameCourtResourceProgress* out;} Run;
#define TRY(x) do{int result_=(x);if(result_!=NBA97_TEXT_COMPLETE)return result_;}while(0)
#define READ(at,pc,to) TRY(read_value(r,(at),4,(pc),&(to)))
#define WRITE(at,pc,v) TRY(write_value(r,(at),4,(v),(pc)))
static int positive(uint32_t v){return v!=0&&v<0x80000000u;}
static int less(uint32_t a,uint32_t b){return(a^0x80000000u)<(b^0x80000000u);}
static void stop(Run* r,uint32_t pc,uint32_t at){r->out->stopped_pc=pc;r->out->stopped_address=at;}
static int access(Run* r,uint32_t at,unsigned width,uint32_t pc,uint8_t** data,uint8_t** known){
    size_t i,j;stop(r,pc,at);
    if(r->out->accesses>=r->budget)return NBA97_TEXT_LIMIT;
    ++r->out->accesses;if(at&(width-1u))return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){Nba97GameTextRegion* region=&r->context->memory.region[i];uint64_t offset=(uint64_t)at-region->base;
        if(at<region->base||offset>region->size||width>region->size-(size_t)offset)continue;
        *data=region->data+(size_t)offset;*known=region->known?region->known+(size_t)offset:0;
        if(*known)for(j=0;j<width;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read_value(Run* r,uint32_t at,unsigned width,uint32_t pc,uint32_t* value){
    uint8_t *data,*known;unsigned i;uint32_t v=0;TRY(access(r,at,width,pc,&data,&known));
    if(known)for(i=0;i<width;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<width;++i)v|=(uint32_t)data[i]<<(i*8);
    *value=v;return NBA97_TEXT_COMPLETE;
}
static int write_value(Run* r,uint32_t at,unsigned width,uint32_t value,uint32_t pc){
    uint8_t *data,*known;unsigned i;Nba97GameTextPoolEvent* e;stop(r,pc,at);
    if(r->out->events>=r->capacity)return NBA97_TEXT_LIMIT;
    TRY(access(r,at,width,pc,&data,&known));
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);e->pc=pc;e->address=at;e->width=(uint8_t)width;e->value=width==1?value&255u:value;e->completed=1;
    for(i=0;i<width;++i){data[i]=(uint8_t)(value>>(8*i));if(known)known[i]=1;}
    ++r->out->stores;return NBA97_TEXT_COMPLETE;
}
static int allocate(Run* r,uint32_t name,uint32_t size,uint32_t* payload){
    Nba97GameTextPoolProgress* p=&r->out->allocation;int result;
    Nba97GameTextPoolEvent* journal=r->journal?r->journal+r->out->events:0;
    result=nba97_game_allocate_payload_90160(r->context,name,size,0,journal,r->capacity-r->out->events,p);
    r->out->events+=p->events;r->out->stores+=p->stores;r->out->callbacks_completed+=p->callbacks_completed;
    stop(r,p->stopped_pc,p->stopped_address);
    if(result!=NBA97_TEXT_COMPLETE)return result;
    *payload=p->return_v0;++r->out->allocations_completed;return NBA97_TEXT_COMPLETE;
}
int nba97_game_court_resources(Nba97GameTextPoolContext* context,uint32_t loaded_court,size_t access_budget,
    Nba97GameTextPoolEvent* journal,size_t capacity,Nba97GameCourtResourceProgress* out){
    Run run,*r=&run;uint32_t cursor=loaded_court,resource,count,first,second,header,groups,index=0,other,payload,bank,list_slot,packet,source,destination,line,v;size_t i,j;
    if(!context||!out||(!journal&&capacity)||(!context->memory.region&&context->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<context->memory.count;++i){const Nba97GameTextRegion* a=&context->memory.region[i];
        if(!a->data||!a->size||a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&context->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}
    }
    memset(out,0,sizeof *out);r->context=context;r->budget=access_budget;r->capacity=capacity;r->journal=journal;r->out=out;
    WRITE(0x800febe4u,0x80048a54u,cursor);READ(0x800febe4u,0x80048a5cu,resource);
    READ(resource,0x80048a64u,count);cursor+=20;
    WRITE(resource+12,0x80048a6cu,cursor);READ(resource+12,0x80048a70u,first);READ(resource,0x80048a74u,groups);
    cursor+=count<<4;READ(resource+4,0x80048a80u,count);WRITE(resource+16,0x80048a84u,cursor);READ(resource+16,0x80048a88u,second);
    WRITE(0x80102c84u,0x80048a94u,first);WRITE(0x800fc964u,0x80048aa0u,second);cursor+=count<<4;
    if(positive(groups))do{
        READ(0x80102c84u,0x80048ab0u,header);READ(header+4,0x80048ab8u,first);READ(header+4,0x80048abcu,second);++index;
        WRITE(header+8,0x80048ac8u,cursor);WRITE(0x80102c84u,0x80048ad0u,header+16);
        cursor+=first*40u;WRITE(header+12,0x80048ae8u,cursor);READ(resource,0x80048aecu,groups);cursor+=second*40u;
    }while(less(index,groups));
    READ(0x800febe4u,0x80048b08u,resource);READ(resource+4,0x80048b10u,groups);index=0;
    if(positive(groups))do{
        READ(0x800fc964u,0x80048b24u,header);READ(header+4,0x80048b2cu,first);READ(header+4,0x80048b30u,second);++index;
        WRITE(header+8,0x80048b3cu,cursor);WRITE(0x800fc964u,0x80048b44u,header+16);
        cursor+=first*24u;WRITE(header+12,0x80048b5cu,cursor);READ(resource+4,0x80048b60u,groups);cursor+=second*24u;
    }while(less(index,groups));
    READ(0x800febe4u,0x80048b7cu,resource);READ(0x800dcf10u,0x80048b84u,other);
    READ(resource+4,0x80048b88u,count);READ(resource,0x80048b8cu,first);READ(resource+4,0x80048b90u,second);
    WRITE(resource+8,0x80048ba4u,cursor);READ(resource+16,0x80048ba8u,header);
    WRITE(0x8010b60cu,0x80048bbcu,first+second+other+1u);WRITE(0x800fc964u,0x80048bc4u,header);
    TRY(allocate(r,0x800260c0u,count<<2,&payload));READ(0x800febe4u,0x80048bd4u,resource);
    WRITE(0x800feda0u,0x80048be0u,payload);READ(resource+4,0x80048be4u,count);
    TRY(allocate(r,0x800260c0u,count<<2,&payload));READ(0x800febe4u,0x80048bfcu,resource);
    WRITE(0x800feda4u,0x80048c04u,payload);READ(resource+4,0x80048c08u,groups);index=0;
    if(positive(groups))do{
        list_slot=0x800feda0u;
        for(bank=0;bank<2;++bank){
            READ(0x800fc964u,0x80048c30u,header);READ(header+4,0x80048c38u,count);
            TRY(allocate(r,0x800260d0u,count<<4,&payload));line=0;
            READ(list_slot,0x80048c54u,first);READ(0x800fc964u,0x80048c5cu,header);
            WRITE(first+(index<<2),0x80048c6cu,payload);READ(header+4,0x80048c70u,count);READ(header+(bank<<2)+8,0x80048c7cu,packet);
            source=packet+6;destination=payload+6;
            if(positive(count))do{
                TRY(write_value(r,destination-3,1,3,0x80048c90u));TRY(write_value(r,destination+1,1,0x40,0x80048c94u));
                TRY(read_value(r,source-2,1,0x80048c98u,&v));TRY(write_value(r,destination-2,1,v,0x80048ca0u));
                TRY(read_value(r,source-1,1,0x80048ca4u,&v));TRY(write_value(r,destination-1,1,v,0x80048cacu));
                TRY(read_value(r,source,1,0x80048cb0u,&v));TRY(write_value(r,destination,1,v,0x80048cc0u));
                /* Actual9C274(a1=0) rereads the just-authored command byte,
                 * then clears only bit1 in its return delay slot. */
                TRY(read_value(r,payload+7,1,0x8009c288u,&v));TRY(write_value(r,payload+7,1,v&0xfdu,0x8009c298u));
                READ(0x800fc964u,0x80048cc8u,header);++line;READ(header+4,0x80048cd0u,count);
                payload+=16;source+=24;destination+=16;
            }while(less(line,count));
            list_slot+=4;
        }
        READ(0x800febe4u,0x80048cfcu,resource);READ(0x800fc964u,0x80048d04u,header);READ(resource+4,0x80048d08u,groups);++index;
        WRITE(0x800fc964u,0x80048d18u,header+16);
    }while(less(index,groups));
    out->completed=1;stop(r,0x80048d28u,0);return NBA97_TEXT_COMPLETE;
}
