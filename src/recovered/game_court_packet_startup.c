#include "game_court_packet_startup.h"
#include <string.h>
typedef struct Run {Nba97CourtPacketStartupContext* context;Nba97CourtPacketStartupEvent* journal;size_t capacity;Nba97CourtPacketStartupProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_TEXT_COMPLETE)return status_;}while(0)
#define READ(a,w,p,v) TRY(read_value(r,(a),(w),(p),&(v)))
#define WRITE(a,w,v,p) TRY(write_value(r,(a),(w),(v),(p)))
static void stop(Run* r,uint32_t pc,uint32_t address){r->out->stopped_pc=pc;r->out->stopped_address=address;}
static int signed_less(uint32_t a,uint32_t b){return(a^0x80000000u)<(b^0x80000000u);}
static int begin(Run* r,Nba97CourtPacketStartupContext* c,Nba97CourtPacketStartupEvent* journal,size_t capacity,Nba97CourtPacketStartupProgress* out){
    size_t i,j;if(!c||!out||(!journal&&capacity)||(!c->memory.region&&c->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<c->memory.count;++i){const Nba97GameTextRegion* a=&c->memory.region[i];
        if(!a->data||!a->size||(uint64_t)a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&c->memory.region[j];if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}}
    memset(out,0,sizeof *out);r->context=c;r->journal=journal;r->capacity=capacity;r->out=out;return NBA97_TEXT_COMPLETE;
}
static int access(Run* r,uint32_t address,unsigned width,uint32_t pc,uint8_t** data,uint8_t** known){
    size_t i,j;stop(r,pc,address);if(r->out->accesses>=r->context->access_budget)return NBA97_TEXT_LIMIT;
    ++r->out->accesses;if((width==4u&&(address&3u))||(width==2u&&(address&1u)))return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){Nba97GameTextRegion* m=&r->context->memory.region[i];uint64_t offset=(uint64_t)address-m->base;
        if(address<m->base||offset>m->size||width>m->size-(size_t)offset)continue;
        *data=m->data+(size_t)offset;*known=m->known?m->known+(size_t)offset:0;
        if(*known)for(j=0;j<width;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;}return NBA97_TEXT_RESOURCE;
}
static int read_value(Run* r,uint32_t address,unsigned width,uint32_t pc,uint32_t* value){
    uint8_t *data,*known;unsigned i;uint32_t word=0;TRY(access(r,address,width,pc,&data,&known));
    if(known)for(i=0;i<width;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<width;++i)word|=(uint32_t)data[i]<<(i*8);
    *value=word;return NBA97_TEXT_COMPLETE;
}
static int write_value(Run* r,uint32_t address,unsigned width,uint32_t value,uint32_t pc){
    uint8_t *data,*known;unsigned i;Nba97CourtPacketStartupEvent* event;stop(r,pc,address);
    if(r->out->events>=r->capacity)return NBA97_TEXT_LIMIT;
    TRY(access(r,address,width,pc,&data,&known));
    event=&r->journal[r->out->events++];memset(event,0,sizeof *event);event->pc=pc;event->address=address;event->value=width==1u?value&255u:width==2u?value&65535u:value;event->width=(uint8_t)width;event->kind=0;event->completed=1;
    for(i=0;i<width;++i){data[i]=(uint8_t)(value>>(i*8));if(known)known[i]=1;}++r->out->stores;return NBA97_TEXT_COMPLETE;
}
static int service(Run* r,uint32_t pc,uint8_t kind,uint32_t entry,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t* returned){
    Nba97CourtPacketStartupEvent* event;Nba97CourtPacketStartupValue value={0,0};stop(r,pc,0);
    if(r->out->events>=r->capacity)return NBA97_TEXT_LIMIT;
    event=&r->journal[r->out->events++];memset(event,0,sizeof *event);event->pc=pc;event->entry=entry;event->kind=kind;event->argument_count=kind==NBA97_COURT_PACKET_STARTUP_SYNC?1u:4u;
    event->argument[0]=a;event->argument[1]=b;event->argument[2]=c;event->argument[3]=d;
    if(!r->context->io||r->context->io(r->context->user,&r->context->memory,event,&value)!=1)return NBA97_TEXT_IO_REFUSED;
    if(value.known>1||(!value.known&&value.word))return NBA97_TEXT_ARGUMENT;
    if(returned&&!value.known)return NBA97_TEXT_UNKNOWN;
    event->returned=value;event->completed=1;++r->out->services_completed;if(returned)*returned=value.word;return NBA97_TEXT_COMPLETE;
}
static int page(Run* r,uint32_t pc,uint32_t* value){return service(r,pc,NBA97_COURT_PACKET_STARTUP_PAGE,0x8009bf98u,2,0,0x200,0x100,value);}
static int patch_pair(Run* r,uint32_t source,uint32_t destination,uint32_t u,uint32_t v,uint32_t pc_base,int bc4){
    uint32_t value,page_value;TRY(page(r,pc_base,&page_value));
    WRITE(destination+0x16u,2,page_value,pc_base+0x08u);
    WRITE(source+0x0cu,1,0,pc_base+0x0cu);READ(source+0x0cu,1,pc_base+0x10u,value);
    WRITE(source+0x16u,2,page_value,pc_base+(bc4?0x14u:0x18u));WRITE(source+0x0du,1,u,pc_base+0x1cu);
    WRITE(source+0x14u,1,0x10,pc_base+0x20u);WRITE(source+0x15u,1,u,pc_base+0x24u);
    WRITE(source+0x1cu,1,0,pc_base+0x28u);WRITE(source+0x1du,1,v,pc_base+0x2cu);
    WRITE(destination+0x0cu,1,value,pc_base+0x30u);READ(source+0x0du,1,pc_base+0x34u,value);WRITE(destination+0x0du,1,value,pc_base+0x3cu);
    READ(source+0x14u,1,pc_base+0x40u,value);WRITE(destination+0x14u,1,value,pc_base+0x48u);
    READ(source+0x15u,1,pc_base+0x4cu,value);WRITE(destination+0x15u,1,value,pc_base+0x54u);
    READ(source+0x1cu,1,pc_base+0x58u,value);WRITE(destination+0x1cu,1,value,pc_base+0x60u);
    READ(source+0x1du,1,pc_base+0x64u,value);WRITE(destination+0x1du,1,value,pc_base+0x6cu);
    return NBA97_TEXT_COMPLETE;
}
int nba97_game_court_packet_startup(Nba97CourtPacketStartupContext* c,Nba97CourtPacketStartupEvent* journal,size_t capacity,Nba97CourtPacketStartupProgress* out){
    Run run,*r=&run;uint32_t player,offset=0,u=0x5f,v=0x6f,mask,base,context,group,descriptor,count,source,destination,index,bc4;
    TRY(begin(r,c,journal,capacity,out));TRY(service(r,0x800484b8u,NBA97_COURT_PACKET_STARTUP_SYNC,0x800994f4u,0,0,0,0,0));
    for(player=0;player<10u;++player,offset+=0xbccu,u+=0x10u,v+=0x10u){
        READ(0x1f80000cu,4,0x80055f0cu,mask);if(!(mask&(1u<<player)))continue;
        READ(0x800f0ed8u,4,0x80048500u,base);context=base+offset;WRITE(0x800f0ed4u,4,context,0x80048520u);++out->players_selected;
        for(group=0;group<20u;++group){++out->body_groups_scanned;READ(context+0xb0u+group*0x94u,4,0x80048524u,descriptor);
            READ(descriptor,4,0x8004852cu,count);READ(descriptor+8u,4,0x80048530u,source);READ(descriptor+0xcu,4,0x80048534u,destination);
            if(!count||!signed_less(0,count))continue;
            for(index=0;signed_less(index,count);++index){TRY(patch_pair(r,source+index*0x20u,destination+index*0x20u,u,v,0x8004855cu,0));++out->body_packets_patched;
                READ(context+0xb0u+group*0x94u,4,0x800485ccu,descriptor);READ(descriptor,4,0x800485d4u,count);}
        }
        READ(0x800f0ed4u,4,0x80048600u,context);READ(context+0xbc4u,4,0x80048608u,bc4);
        READ(bc4,4,0x80048610u,count);READ(bc4+0x28u,4,0x80048614u,source);READ(bc4+0x2cu,4,0x80048618u,destination);
        if(count&&signed_less(0,count))for(index=0;signed_less(index,count);++index){TRY(patch_pair(r,source+index*0x20u,destination+index*0x20u,u,v-1u,0x80048640u,1));++out->bc4_packets_patched;
            READ(0x800f0ed4u,4,0x800486b0u,context);READ(context+0xbc4u,4,0x800486bcu,bc4);READ(bc4,4,0x800486c4u,count);}
        WRITE(0x8010b270u,4,2,0x800486e4u);WRITE(0x800fda04u,4,0x20,0x800486f8u);WRITE(0x800fda08u,4,0x20,0x80048700u);
    }
    out->completed=1;stop(r,0x80048744u,0);return NBA97_TEXT_COMPLETE;
}
