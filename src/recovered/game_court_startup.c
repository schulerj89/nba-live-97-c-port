#include "game_court_startup.h"
#include <string.h>
typedef struct Run {
    Nba97GameCourtStartupContext* context;
    Nba97GameCourtStartupEvent* journal;
    Nba97GameCourtStartupProgress* out;
    size_t budget,capacity;
} Run;
#define TRY(x) do {int status_=(x);if(status_!=NBA97_TEXT_COMPLETE)return status_;} while(0)
#define READ(a,p,v) TRY(read_value(r,(a),4,(p),&(v)))
#define WRITE(a,w,v,p) TRY(write_value(r,(a),(w),(v),(p)))
static void stop(Run* r,uint32_t pc,uint32_t address){r->out->stopped_pc=pc;r->out->stopped_address=address;}
static int begin(Run* r,Nba97GameCourtStartupContext* context,size_t budget,
    Nba97GameCourtStartupEvent* journal,size_t capacity,Nba97GameCourtStartupProgress* out){
    size_t i,j;
    if(!context||!out||(!journal&&capacity)||(!context->memory.region&&context->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<context->memory.count;++i){const Nba97GameTextRegion* a=&context->memory.region[i];
        if(!a->data||!a->size||(uint64_t)a->size>UINT64_C(0x100000000)||
            (uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&context->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&
                (uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}
    }
    memset(out,0,sizeof *out);r->context=context;r->journal=journal;r->out=out;r->budget=budget;r->capacity=capacity;
    return NBA97_TEXT_COMPLETE;
}
static int access(Run* r,uint32_t address,unsigned width,uint32_t pc,uint8_t** bytes,uint8_t** known){
    size_t i,j;stop(r,pc,address);
    if(r->out->accesses>=r->budget)return NBA97_TEXT_LIMIT;
    ++r->out->accesses;
    if(address&(width-1u))return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){Nba97GameTextRegion* a=&r->context->memory.region[i];uint64_t offset=(uint64_t)address-a->base;
        if(address<a->base||offset>a->size||width>a->size-(size_t)offset)continue;
        *bytes=a->data+(size_t)offset;*known=a->known?a->known+(size_t)offset:0;
        if(*known)for(j=0;j<width;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read_value(Run* r,uint32_t address,unsigned width,uint32_t pc,uint32_t* value){
    uint8_t *bytes,*known;unsigned i;uint32_t word=0;TRY(access(r,address,width,pc,&bytes,&known));
    if(known)for(i=0;i<width;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<width;++i)word|=(uint32_t)bytes[i]<<(i*8);
    *value=word;return NBA97_TEXT_COMPLETE;
}
static int write_value(Run* r,uint32_t address,unsigned width,uint32_t word,uint32_t pc){
    uint8_t *bytes,*known;unsigned i;Nba97GameCourtStartupEvent* event;stop(r,pc,address);
    if(r->out->events>=r->capacity)return NBA97_TEXT_LIMIT;
    TRY(access(r,address,width,pc,&bytes,&known));
    event=&r->journal[r->out->events++];memset(event,0,sizeof *event);
    event->pc=pc;event->address=address;event->value=width==1?word&255u:width==2?word&65535u:word;
    event->width=(uint8_t)width;event->completed=1;
    for(i=0;i<width;++i){bytes[i]=(uint8_t)(word>>(i*8));if(known)known[i]=1;}
    ++r->out->stores;return NBA97_TEXT_COMPLETE;
}
static int service(Run* r,uint32_t pc,unsigned kind,uint32_t argument,uint32_t* result){
    Nba97GameCourtStartupEvent* event;uint32_t returned=0;stop(r,pc,argument);
    if(r->out->events>=r->capacity)return NBA97_TEXT_LIMIT;
    event=&r->journal[r->out->events++];memset(event,0,sizeof *event);
    event->pc=pc;event->kind=(uint8_t)kind;event->argument[0]=argument;
    if(!r->context->io||r->context->io(r->context->user,&r->context->memory,event,&returned)!=1)return NBA97_TEXT_IO_REFUSED;
    /* Complete29BFC cannot returnNULL:941C8's NULL outcome retries. Reject
     * this inconsistent service acknowledgement; do not invent a resource. */
    if(kind==NBA97_COURT_STARTUP_LOAD_29BFC&&!returned)return NBA97_TEXT_IO_REFUSED;
    event->returned=returned;event->completed=1;++r->out->callbacks_completed;
    if(result)*result=returned;
    return NBA97_TEXT_COMPLETE;
}
int nba97_game_court_startup_select_texture(Nba97GameCourtStartupContext* context,size_t access_budget,
    Nba97GameCourtStartupEvent* journal,size_t capacity,Nba97GameCourtStartupProgress* out){
    Run run,*r=&run;uint32_t special,neutral,team,filename;
    TRY(begin(r,context,access_budget,journal,capacity,out));
    READ(0x800dcf10u,0x80048748u,special);
    WRITE(0x80103508u,4,UINT32_MAX,0x80048754u);WRITE(0x800fcc54u,4,UINT32_MAX,0x8004875cu);
    if(special)filename=0x800260a0u;
    else {
        READ(0x8001ec94u,0x8004877cu,neutral);
        if(neutral)team=31;
        else READ(0x80021d74u,0x80048794u,team);
        READ(0x800b76c8u+(team<<2),0x800487acu,filename);
    }
    out->filename=filename;
    TRY(service(r,0x800487b0u,NBA97_COURT_STARTUP_LOAD_29BFC,filename,&out->loaded_resource));
    out->completed=1;stop(r,0x800487b8u,0);return NBA97_TEXT_COMPLETE;
}
int nba97_game_court_startup_select_geometry(Nba97GameCourtStartupContext* context,uint32_t loaded_texture,
    size_t access_budget,Nba97GameCourtStartupEvent* journal,size_t capacity,Nba97GameCourtStartupProgress* out){
    Run run,*r=&run;uint32_t special,neutral,team,filename,bank,quad,packet,v;
    TRY(begin(r,context,access_budget,journal,capacity,out));
    TRY(service(r,0x80048894u,NBA97_COURT_STARTUP_SYNC_994F4,0,0));
    TRY(service(r,0x8004889cu,NBA97_COURT_STARTUP_FREE_90698,loaded_texture,0));
    READ(0x800dcf10u,0x800488a8u,special);
    if(special){
        for(bank=0;bank<2;++bank)for(quad=0;quad<2;++quad){
            packet=0x801041a4u+bank*72u+quad*36u;
            /* Complete9C33C; preserve low24 tag links and padding. */
            WRITE(packet+3,1,8,0x8009c340u);WRITE(packet+7,1,0x38,0x8009c34cu);
            if(!quad){
                WRITE(packet+4,1,0,0x80048914u);WRITE(packet+5,1,0,0x80048918u);WRITE(packet+6,1,128,0x8004891cu);
                WRITE(packet+12,1,0,0x80048920u);WRITE(packet+13,1,0,0x80048924u);WRITE(packet+14,1,128,0x80048928u);
                WRITE(packet+20,1,128,0x8004892cu);WRITE(packet+21,1,0,0x80048930u);WRITE(packet+22,1,100,0x80048934u);
                WRITE(packet+28,1,128,0x80048938u);WRITE(packet+29,1,0,0x8004893cu);WRITE(packet+30,1,100,0x80048944u);
            }else{
                WRITE(packet+4,1,128,0x80048948u);WRITE(packet+5,1,0,0x8004894cu);WRITE(packet+6,1,100,0x80048950u);
                WRITE(packet+12,1,128,0x80048954u);WRITE(packet+13,1,0,0x80048958u);WRITE(packet+14,1,100,0x8004895cu);
                WRITE(packet+20,1,0,0x80048960u);WRITE(packet+21,1,0,0x80048964u);WRITE(packet+22,1,0,0x80048968u);
                WRITE(packet+28,1,0,0x8004896cu);WRITE(packet+29,1,0,0x80048970u);WRITE(packet+30,1,0,0x80048974u);
            }
            /* Reached9C274(a1=0) reads command before its delay-slot store. */
            TRY(read_value(r,packet+7,1,0x8009c288u,&v));WRITE(packet+7,1,v&0xfdu,0x8009c298u);
            WRITE(packet+26,2,(quad+1u)*48u,0x800489a4u);WRITE(packet+34,2,(quad+1u)*48u,0x800489a8u);
            WRITE(packet+10,2,quad*48u,0x800489b0u);WRITE(packet+18,2,quad*48u,0x800489b4u);
            WRITE(packet+16,2,511,0x800489bcu);WRITE(packet+32,2,511,0x800489c0u);
            WRITE(packet+8,2,0,0x800489c8u);WRITE(packet+24,2,0,0x800489ccu);
        }
        filename=0x800260b0u;
    }else{
        READ(0x8001ec94u,0x80048a10u,neutral);
        if(neutral)team=31;
        else READ(0x80021d74u,0x80048a28u,team);
        READ(0x800b763cu+(team<<2),0x80048a40u,filename);
    }
    out->filename=filename;
    TRY(service(r,0x80048a44u,NBA97_COURT_STARTUP_LOAD_29BFC,filename,&out->loaded_resource));
    out->completed=1;stop(r,0x80048a4cu,0);return NBA97_TEXT_COMPLETE;
}
