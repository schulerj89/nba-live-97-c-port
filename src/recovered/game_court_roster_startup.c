#include "game_court_roster_startup.h"
#include <string.h>
typedef struct Run {Nba97GameCourtRosterContext* context;Nba97GameCourtRosterEvent* journal;size_t capacity;Nba97GameCourtRosterProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_TEXT_COMPLETE)return status_;}while(0)
#define READ(a,w,p,v) TRY(read_value(r,(a),(w),(p),&(v)))
#define WRITE(a,w,v,p) TRY(write_value(r,(a),(w),(v),(p)))
static void stop(Run* r,uint32_t pc,uint32_t address){r->out->stopped_pc=pc;r->out->stopped_address=address;}
static int less(uint32_t a,uint32_t b){return(a^0x80000000u)<(b^0x80000000u);}
static int begin(Run* r,Nba97GameCourtRosterContext* context,Nba97GameCourtRosterEvent* journal,size_t capacity,Nba97GameCourtRosterProgress* out){
    size_t i,j;
    if(!context||!out||(!journal&&capacity)||(!context->memory.region&&context->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<context->memory.count;++i){const Nba97GameTextRegion* a=&context->memory.region[i];
        if(!a->data||!a->size||(uint64_t)a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&context->memory.region[j];
            if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}
    }
    memset(out,0,sizeof *out);r->context=context;r->journal=journal;r->capacity=capacity;r->out=out;return NBA97_TEXT_COMPLETE;
}
static int access(Run* r,uint32_t a,unsigned width,uint32_t pc,uint8_t** data,uint8_t** known){
    size_t i,j;stop(r,pc,a);if(r->out->accesses>=r->context->access_budget)return NBA97_TEXT_LIMIT;
    ++r->out->accesses;if(a&(width-1u))return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){Nba97GameTextRegion* region=&r->context->memory.region[i];uint64_t offset=(uint64_t)a-region->base;
        if(a<region->base||offset>region->size||width>region->size-(size_t)offset)continue;
        *data=region->data+(size_t)offset;*known=region->known?region->known+(size_t)offset:0;
        if(*known)for(j=0;j<width;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;
    }
    return NBA97_TEXT_RESOURCE;
}
static int read_value(Run* r,uint32_t a,unsigned width,uint32_t pc,uint32_t* value){
    uint8_t *data,*known;unsigned i;uint32_t v=0;TRY(access(r,a,width,pc,&data,&known));
    if(known)for(i=0;i<width;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<width;++i)v|=(uint32_t)data[i]<<(i*8);
    *value=v;return NBA97_TEXT_COMPLETE;
}
static int write_value(Run* r,uint32_t a,unsigned width,uint32_t value,uint32_t pc){
    uint8_t *data,*known;unsigned i;Nba97GameCourtRosterEvent* e;stop(r,pc,a);
    if(r->out->events>=r->capacity)return NBA97_TEXT_LIMIT;
    TRY(access(r,a,width,pc,&data,&known));e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->pc=pc;e->address=a;e->width=(uint8_t)width;e->value=width==1?value&255u:width==2?value&65535u:value;e->completed=1;
    for(i=0;i<width;++i){data[i]=(uint8_t)(value>>(i*8));if(known)known[i]=1;}
    ++r->out->stores;return NBA97_TEXT_COMPLETE;
}
static int compare(Run* r,uint32_t pc,uint32_t left,uint32_t right,uint32_t* different){
    Nba97GameCourtRosterEvent* e;uint32_t a,b;stop(r,pc,left);
    if(r->out->events>=r->capacity)return NBA97_TEXT_LIMIT;
    e=&r->journal[r->out->events++];memset(e,0,sizeof *e);e->pc=pc;e->kind=NBA97_COURT_ROSTER_STRCMP_A0_17;e->argument[0]=left;e->argument[1]=right;
    /* BIOSA0/17 equality contract, not ROM read-instruction reproduction. */
    do{READ(left,1,0x000000a0u,a);READ(right,1,0x000000a0u,b);++left;++right;}while(a==b&&a);
    *different=a!=b;e->value=*different;e->completed=1;++r->out->comparisons;return NBA97_TEXT_COMPLETE;
}
static int scratch(Run* r){
    uint32_t a;for(a=0x1f800030u;a<0x1f800040u;a+=4)WRITE(a,4,a,0x80055f00u);
    return NBA97_TEXT_COMPLETE;
}
static int match(Run* r,uint32_t index,uint32_t* result){
    uint32_t record,left1=0x800b7254u,left2=0x800b726cu,left3=0x800b7284u,right1,right2,v,different;
    READ(0x800fc664u+(index<<2),4,0x80047870u,record);
    READ(left1,1,0x80047878u,v);++left1;right1=record+0x29u;
    if(v)do{READ(left1,1,0x80047894u,v);++left1;}while(v);
    do{READ(left2,1,0x800478a4u,v);++left2;}while(v);
    do{READ(left3,1,0x800478b4u,v);++left3;}while(v);
    right2=right1;do{READ(right2,1,0x800478c4u,v);++right2;}while(v);
    TRY(compare(r,0x800478d8u,0x800b7254u,right1,&different));
    if(!different){TRY(compare(r,0x800478ecu,left1,right2,&different));if(!different){*result=1;goto done;}}
    TRY(compare(r,0x80047904u,0x800b726cu,right1,&different));
    if(!different){TRY(compare(r,0x80047918u,left2,right2,&different));if(!different){*result=2;goto done;}}
    TRY(compare(r,0x80047930u,0x800b7284u,right1,&different));
    if(!different){TRY(compare(r,0x80047944u,left3,right2,&different));if(!different){*result=0;goto done;}}
    /* Fallback rereads the roster pointer after comparisons. Only exact signed
     * ID01C0 produces3; it does not classify every mismatching name as special. */
    READ(0x800fc664u+(index<<2),4,0x80047964u,record);READ(record,2,0x8004796cu,v);
    *result=v==0x1c0u?3u:0u;
done:
    r->out->match_result=*result;++r->out->matches_completed;return NBA97_TEXT_COMPLETE;
}
int nba97_game_court_roster_scratch_init(Nba97GameCourtRosterContext* c,Nba97GameCourtRosterEvent* journal,size_t capacity,Nba97GameCourtRosterProgress* out){
    Run run,*r=&run;TRY(begin(r,c,journal,capacity,out));TRY(scratch(r));out->completed=1;stop(r,0x80053708u,0);return NBA97_TEXT_COMPLETE;
}
int nba97_game_court_roster_match(Nba97GameCourtRosterContext* c,uint32_t index,Nba97GameCourtRosterEvent* journal,size_t capacity,Nba97GameCourtRosterProgress* out){
    Run run,*r=&run;uint32_t value;TRY(begin(r,c,journal,capacity,out));TRY(match(r,index,&value));out->completed=1;stop(r,0x800479b8u,0);return NBA97_TEXT_COMPLETE;
}
int nba97_game_court_roster_startup(Nba97GameCourtRosterContext* c,Nba97GameCourtRosterEvent* journal,size_t capacity,Nba97GameCourtRosterProgress* out){
    Run run,*r=&run;uint32_t i,v,tag,record,record_slot,p,end,selected;
    TRY(begin(r,c,journal,capacity,out));WRITE(0x800dcf10u,4,0,0x800479e8u);TRY(scratch(r));
    WRITE(0x1f80000cu,4,0,0x80055f00u);READ(0x1f800014u,4,0x80055f0cu,v);
    if(v&1u){READ(0x1f800014u,4,0x80055f0cu,v);WRITE(0x1f800004u,4,v^1u,0x80055f00u);}
    for(i=0;i<24;++i){
        tag=0x80102c8cu+i*32u;record_slot=0x800fc664u+i*4u;
        WRITE(tag,1,255,0x80047a54u);TRY(match(r,i,&selected));
        if(selected){
            out->special_roster_seen=1;READ(0x1f80000cu,4,0x80055f0cu,v);WRITE(0x1f80000cu,4,v|(1u<<i),0x80055f00u);
            READ(record_slot,4,0x80047a88u,record);WRITE(0x1f800028u,4,record,0x80055f00u);
            READ(0x1f800028u,4,0x80055f0cu,record);TRY(match(r,i,&selected));
            p=record+14u;end=record+31u;WRITE(tag,1,selected,0x80047ab8u);
            do{WRITE(p,1,99,0x80047abcu);++p;}while(less(p,end));
        }
    }
    if(out->special_roster_seen){
        for(i=0;i<24;++i){
            record_slot=0x800fc664u+i*4u;
            READ(record_slot,4,0x80047b08u,record);READ(record+28,1,0x80047b10u,v);
            if(v>96){WRITE(record+28,1,96,0x80047b24u);READ(record_slot,4,0x80047b28u,record);}
            READ(record+21,1,0x80047b30u,v);if(v>96)WRITE(record+21,1,96,0x80047b44u);
            READ(record_slot,4,0x80047b48u,record);READ(record+18,1,0x80047b50u,v);
            if(v>99){WRITE(record+18,1,99,0x80047b64u);READ(record_slot,4,0x80047b68u,record);}
            READ(record+19,1,0x80047b70u,v);if(v>99)WRITE(record+19,1,99,0x80047b84u);
            READ(record_slot,4,0x80047b88u,record);READ(record+26,1,0x80047b90u,v);
            if(v>99){WRITE(record+26,1,99,0x80047ba4u);READ(record_slot,4,0x80047ba8u,record);}
            READ(record+22,1,0x80047bb0u,v);if(v>99)WRITE(record+22,1,99,0x80047bc4u);
            READ(record_slot,4,0x80047bc8u,record);READ(record+23,1,0x80047bd0u,v);
            if(v>99){WRITE(record+23,1,99,0x80047be4u);READ(record_slot,4,0x80047be8u,record);}
            READ(record+32,1,0x80047bf0u,v);if(v>23)WRITE(record+32,1,23,0x80047c04u);
            READ(0x80102c8cu+i*32u,1,0x80047c10u,v);
            if(v!=255){
                READ(record_slot,4,0x80047c20u,record);WRITE(record+28,1,99,0x80047c28u);
                READ(record_slot,4,0x80047c2cu,record);WRITE(record+21,1,99,0x80047c34u);
                READ(record_slot,4,0x80047c38u,record);WRITE(record+18,1,255,0x80047c40u);
                READ(record_slot,4,0x80047c44u,record);WRITE(record+19,1,255,0x80047c4cu);
                READ(record_slot,4,0x80047c50u,record);WRITE(record+26,1,255,0x80047c58u);
                READ(record_slot,4,0x80047c5cu,record);WRITE(record+22,1,255,0x80047c64u);
                READ(record_slot,4,0x80047c68u,record);WRITE(record+23,1,255,0x80047c70u);
                READ(record_slot,4,0x80047c74u,record);WRITE(record+32,1,23,0x80047c7cu);
            }
        }
        WRITE(0x800b840au,2,0x40e,0x80047c9cu);WRITE(0x800b846au,2,0x2dd,0x80047ca8u);WRITE(0x800b84cau,2,0x42c,0x80047cb4u);
    }
    out->completed=1;stop(r,0x80047cb8u,0);return NBA97_TEXT_COMPLETE;
}
