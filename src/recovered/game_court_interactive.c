#include "game_court_interactive.h"
#include <string.h>
typedef struct Run {Nba97CourtInteractiveContext* context;Nba97CourtInteractiveEvent* journal;size_t capacity;Nba97CourtInteractiveProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_TEXT_COMPLETE)return status_;}while(0)
#define READ(a,w,p,v) TRY(read_value(r,(a),(w),(p),&(v)))
#define WRITE(a,w,v,p) TRY(write_value(r,(a),(w),(v),(p)))
static void stop(Run* r,uint32_t pc,uint32_t a){r->out->stopped_pc=pc;r->out->stopped_address=a;}
static int less(uint32_t a,uint32_t b){return(a^0x80000000u)<(b^0x80000000u);}
static uint32_t signed16(uint32_t v){return(v&0x8000u)?v|0xffff0000u:v;}
static int begin(Run* r,Nba97CourtInteractiveContext* c,Nba97CourtInteractiveEvent* journal,size_t capacity,Nba97CourtInteractiveProgress* out){
    size_t i,j;if(!c||!out||(!journal&&capacity)||(!c->memory.region&&c->memory.count))return NBA97_TEXT_ARGUMENT;
    for(i=0;i<c->memory.count;++i){const Nba97GameTextRegion* a=&c->memory.region[i];
        if(!a->data||!a->size||(uint64_t)a->size>UINT64_C(0x100000000)||(uint64_t)a->base+a->size>UINT64_C(0x100000000))return NBA97_TEXT_ARGUMENT;
        for(j=0;j<i;++j){const Nba97GameTextRegion* b=&c->memory.region[j];if((uint64_t)a->base<(uint64_t)b->base+b->size&&(uint64_t)b->base<(uint64_t)a->base+a->size)return NBA97_TEXT_ARGUMENT;}}
    memset(out,0,sizeof *out);r->context=c;r->journal=journal;r->capacity=capacity;r->out=out;return NBA97_TEXT_COMPLETE;
}
static int access(Run* r,uint32_t a,unsigned width,uint32_t pc,uint8_t** data,uint8_t** known){
    size_t i,j;stop(r,pc,a);if(r->out->accesses>=r->context->access_budget)return NBA97_TEXT_LIMIT;
    ++r->out->accesses;if(a&(width-1u))return NBA97_TEXT_ALIGNMENT_TRAP;
    for(i=0;i<r->context->memory.count;++i){Nba97GameTextRegion* m=&r->context->memory.region[i];uint64_t offset=(uint64_t)a-m->base;
        if(a<m->base||offset>m->size||width>m->size-(size_t)offset)continue;
        *data=m->data+(size_t)offset;*known=m->known?m->known+(size_t)offset:0;
        if(*known)for(j=0;j<width;++j)if((*known)[j]>1)return NBA97_TEXT_ARGUMENT;
        return NBA97_TEXT_COMPLETE;}return NBA97_TEXT_RESOURCE;
}
static int read_value(Run* r,uint32_t a,unsigned width,uint32_t pc,uint32_t* value){
    uint8_t *data,*known;unsigned i;uint32_t v=0;TRY(access(r,a,width,pc,&data,&known));
    if(known)for(i=0;i<width;++i)if(!known[i])return NBA97_TEXT_UNKNOWN;
    for(i=0;i<width;++i)v|=(uint32_t)data[i]<<(i*8);
    *value=v;return NBA97_TEXT_COMPLETE;
}
static int write_value(Run* r,uint32_t a,unsigned width,uint32_t value,uint32_t pc){
    uint8_t *data,*known;unsigned i;Nba97CourtInteractiveEvent* e;stop(r,pc,a);if(r->out->events>=r->capacity)return NBA97_TEXT_LIMIT;
    TRY(access(r,a,width,pc,&data,&known));e=&r->journal[r->out->events++];memset(e,0,sizeof *e);
    e->pc=pc;e->address=a;e->width=(uint8_t)width;e->value=width==1?value&255u:width==2?value&65535u:value;e->completed=1;
    for(i=0;i<width;++i){data[i]=(uint8_t)(value>>(i*8));if(known)known[i]=1;}++r->out->stores;return NBA97_TEXT_COMPLETE;
}
static int call(Run* r,uint32_t pc,uint32_t entry,unsigned argc,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t* returned){
    Nba97CourtInteractiveEvent* event;Nba97CourtInteractiveValue value={0,0};stop(r,pc,a);
    if(r->out->events>=r->capacity)return NBA97_TEXT_LIMIT;
    event=&r->journal[r->out->events++];memset(event,0,sizeof *event);event->pc=pc;event->entry=entry;event->kind=1;event->argument_count=(uint8_t)argc;
    event->argument[0]=a;event->argument[1]=b;event->argument[2]=c;event->argument[3]=d;event->argument[4]=e;
    if(!r->context->io||r->context->io(r->context->user,&r->context->memory,event,&value)!=1)return NBA97_TEXT_IO_REFUSED;
    event->returned=value;if(value.known>1)return NBA97_TEXT_ARGUMENT;
    if(returned&&!value.known)return NBA97_TEXT_UNKNOWN;
    if(entry==0x80029bfcu&&!value.word)return NBA97_TEXT_IO_REFUSED;
    event->completed=1;++r->out->services_completed;if(returned)*returned=value.word;return NBA97_TEXT_COMPLETE;
}
static int simple(Run* r,uint32_t pc,uint32_t entry,uint32_t a){return call(r,pc,entry,1,a,0,0,0,0,0);}
static int input(Run* r,uint32_t pc,uint32_t player,uint32_t* value){return call(r,pc,0x8008f224u,1,player,0,0,0,0,value);}
static int upload(Run* r,uint32_t pc,uint32_t image,uint32_t x,uint32_t y){return call(r,pc,0x800946b8u,5,image,x,y,0,0,0);}
static int shpp(Run* r,uint32_t resource,uint32_t index,int is_entry,uint32_t* value){
    Nba97GameFontContext c;Nba97GameFontProgress* p=&r->out->shpp;int status;
    c.memory=r->context->memory;c.step_budget=r->context->access_budget-r->out->accesses;c.io=0;c.user=0;
    status=is_entry?nba97_game_font_shpp_entry(&c,resource,index,value,p):nba97_game_font_shpp_count(&c,resource,value,p);
    r->out->accesses+=p->steps;
    stop(r,is_entry?((p->steps>=2||(status==NBA97_FONT_LIMIT&&p->steps>=1))?0x800a4000u:0x800a3fecu):0x800a3fe0u,p->stopped_address);
    return status;
}
static int digit(Run* r,uint32_t resource,uint32_t number,int ones,uint32_t* image){
    uint32_t target,index,base=ones?0x8004810cu:0x80048044u;
    READ((ones?0x8002610cu:0x800260e4u)+(number<<2),4,ones?0x800480fcu:0x80048034u,target);
    if(target<base||(target-base)%12u||(target-base)/12u>=10u){stop(r,ones?0x80048104u:0x8004803cu,target);return NBA97_COURT_INTERACTIVE_CONTROL_TARGET;}
    index=(target-base)/12u;index=index==0?21u:index<=5?index-1u:index+11u;
    return shpp(r,resource,index,1,image);
}
static int player_record(Run* r,uint32_t i,uint32_t first_pc,uint32_t second_pc,uint32_t third_pc,uint32_t* record){
    uint32_t p;READ(0x800fc650u,4,first_pc,p);READ(p+(i<<2),4,second_pc,p);READ(p+32,4,third_pc,*record);return NBA97_TEXT_COMPLETE;
}
int nba97_game_court_interactive(Nba97CourtInteractiveContext* c,Nba97CourtInteractiveEvent* journal,size_t capacity,Nba97CourtInteractiveProgress* out){
    Run run,*r=&run;uint32_t v,count,resource,index,image,swap,i,buttons,record,raw,tens,ones,x,y,bit;
    TRY(begin(r,c,journal,capacity,out));READ(0x1f800018u,4,0x80055f0cu,v);
    if(v!=0x20u)goto ordinary;
    TRY(input(r,0x80047cd0u,0,&v));if(v!=0xe75u)goto ordinary;
    out->interactive_entered=1;
    TRY(call(r,0x80047cecu,0x80029bfcu,2,0x80026094u,0,0,0,0,&resource));out->loaded_resource=resource;
    index=0;
    for(;;){TRY(shpp(r,resource,0,0,&count));if(!less(index,count))break;
        TRY(shpp(r,resource,index,1,&image));READ(image,1,0x80047d20u,v);++index;WRITE(image,4,v,0x80047d2cu);}
    WRITE(0x1f800004u,4,0xff0000ffu,0x80055f00u);
    READ(0x8001ede8u,4,0x80047d44u,v);swap=!v;WRITE(0x8001ede8u,4,swap,0x80047d64u);
    TRY(simple(r,0x80047d68u,0x80099ca4u,0x8002205cu+swap*20u));
    READ(0x8001ede8u,4,0x80047d74u,v);TRY(simple(r,0x80047d94u,0x80099accu,0x80021eecu+v*92u));
    READ(0x8001ede8u,4,0x80047da0u,v);swap=!v;WRITE(0x8001ede8u,4,swap,0x80047dbcu);
    TRY(simple(r,0x80047dc0u,0x80099ca4u,0x8002205cu+swap*20u));
    READ(0x8001ede8u,4,0x80047dccu,v);TRY(simple(r,0x80047de8u,0x80099accu,0x80021eecu+v*92u));
    WRITE(0x8001ede8u,4,1,0x80047df8u);TRY(simple(r,0x80047dfcu,0x80099ca4u,0x80022070u));
    READ(0x8001ede8u,4,0x80047e08u,v);TRY(simple(r,0x80047e2cu,0x80099accu,0x80021eecu+v*92u));
    for(;;){
        TRY(input(r,0x80047e34u,0,&v));if(v==0x820u)break;
        TRY(simple(r,0x80047e48u,0x800994f4u,0));TRY(simple(r,0x80047e50u,0x80029bdcu,0));
        TRY(call(r,0x80047e68u,0x800aa0bcu,5,0,0,512,256,0,0));
        READ(0x800dcf10u,4,0x80047e74u,v);if(v){TRY(shpp(r,resource,8,1,&image));TRY(upload(r,0x80047e9cu,image,32,32));}
        for(i=0;i<8;++i){
            y=68u+i*20u;TRY(input(r,0x80047eb4u,i,&buttons));
            if(buttons==0x3e1au)WRITE(0x800faba4u+(i<<2),4,2,0x80047ed8u);
            TRY(shpp(r,resource,i+9u,1,&image));TRY(upload(r,0x80047ef8u,image,16,y));
            TRY(player_record(r,i,0x80047f04u,0x80047f10u,0x80047f18u,&record));READ(record+9,1,0x80047f20u,raw);
            /* Source magic-divide sequence has an unsigned-byte input, so its
             * nonnegative /10 and /100 reductions are exact. It draws only the
             * fixed '1' glyph for EVERY nonzero hundreds digit, including2. */
            v=raw>=100u?raw-100u:raw;tens=(v/10u)%10u;ones=v%10u;x=196;
            if(raw>=100u){TRY(shpp(r,resource,0,1,&image));TRY(upload(r,0x80048014u,image,196,y));x=207;}
            TRY(digit(r,resource,tens,0,&image));TRY(upload(r,0x800480d4u,image,x,y));
            READ(image+4,2,0x800480dcu,v);x+=signed16(v);
            TRY(digit(r,resource,ones,1,&image));TRY(upload(r,0x8004819cu,image,x,y));
            READ(0x1f80000cu,4,0x80055f0cu,v);bit=1u<<i;
            if(v&bit){TRY(shpp(r,resource,7,1,&image));TRY(upload(r,0x800481dcu,image,256,y));}
            if(buttons==1u){READ(0x1f80000cu,4,0x80055f0cu,v);WRITE(0x1f80000cu,4,v|bit,0x80055f00u);}
            if(buttons==2u){READ(0x1f80000cu,4,0x80055f0cu,v);if(v&bit){READ(0x1f80000cu,4,0x80055f0cu,v);WRITE(0x1f80000cu,4,v^bit,0x80055f00u);}}
            if(buttons==0x200u){TRY(player_record(r,i,0x80048258u,0x80048264u,0x8004826cu,&record));READ(record+9,1,0x80048274u,v);WRITE(record+9,1,v+1u,0x80048280u);}
            if(buttons==0x1000u){TRY(player_record(r,i,0x80048294u,0x800482a0u,0x800482a8u,&record));READ(record+9,1,0x800482b0u,v);WRITE(record+9,1,v-1u,0x800482bcu);}
            TRY(player_record(r,i,0x800482c4u,0x800482d0u,0x800482d8u,&record));READ(record+9,1,0x800482e0u,v);if(v>144u)WRITE(record+9,1,144,0x800482f4u);
            TRY(player_record(r,i,0x800482fcu,0x80048308u,0x80048310u,&record));READ(record+9,1,0x80048318u,v);if(v<18u)WRITE(record+9,1,18,0x8004832cu);
            if(i==0){
                if(buttons==0x80u)WRITE(0x800dcf10u,4,1,0x8004834cu);
                if(buttons==0x100u)WRITE(0x800dcf10u,4,0,0x8004835cu);
                if(buttons==0x10u){READ(0x1f800004u,4,0x80055f0cu,v);WRITE(0x1f800004u,4,v|0x800u,0x80055f00u);}
                if(buttons==0x40u){READ(0x1f800004u,4,0x80055f0cu,v);if(v&0x800u){READ(0x1f800004u,4,0x80055f0cu,v);WRITE(0x1f800004u,4,v^0x800u,0x80055f00u);}}
            }
            if(i==4){
                if(buttons==0x1f20u){READ(0x1f800014u,4,0x80055f0cu,v);WRITE(0x1f800014u,4,v|1u,0x80055f00u);}
                if(buttons==0x3b20u){READ(0x1f800014u,4,0x80055f0cu,v);if(v&1u){READ(0x1f800014u,4,0x80055f0cu,v);WRITE(0x1f800014u,4,v^1u,0x80055f00u);}}
            }
            ++out->players_completed;
        }
        ++out->frames_completed;
    }
    TRY(simple(r,0x8004844cu,0x80090698u,resource));goto complete;
ordinary:
    WRITE(0x1f800004u,4,0xfffffdffu,0x80055f00u);READ(0x1f800004u,4,0x80055f0cu,v);
    WRITE(0x1f800004u,4,v|0x800u,0x80055f00u);WRITE(0x1f800010u,4,0,0x80055f00u);
    WRITE(0x1f800014u,4,0,0x80055f00u);WRITE(0x1f800018u,4,0,0x80055f00u);
complete:
    out->completed=1;stop(r,0x800484b8u,0);return NBA97_TEXT_COMPLETE;
}
