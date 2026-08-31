#include "game_net_transform.h"
#include <string.h>

typedef struct Run {Nba97PlayerFrameContext* in;Nba97PlayerFrameProgress* out;} Run;
#define TRY(x) do{int status_=(x);if(status_!=NBA97_BODY_OK)return status_;}while(0)
static uint32_t width_mask(unsigned width){return width==4?UINT32_MAX:(1u<<(width*8))-1u;}
static unsigned known_mask(unsigned width){return (1u<<width)-1u;}
static int access(Run* r,uint32_t pc,uint32_t address,unsigned width,unsigned kind,Nba97PlayerFrameValue* value){
    unsigned i;int status;r->out->stopped_pc=pc;r->out->stopped_address=address;
    if(r->out->operations>=r->in->operation_budget)return NBA97_BODY_JOURNAL_LIMIT;
    ++r->out->operations;
    if((width==4&&address&3u)||(width==2&&address&1u))return NBA97_BODY_ALIGNMENT_TRAP;
    status=r->in->access(r->in->user,pc,address,width,kind,value);if(status!=NBA97_BODY_OK)return status;
    if(value->is_reference>1||value->reference.known>1||
       (!value->reference.known&&(value->reference.allocation||value->reference.offset))||
       (!value->is_reference&&value->reference.known)||
       (value->is_reference&&(width!=4||
        (value->reference.known?value->known_mask!=known_mask(width):(value->known_mask||value->word))))||
       (value->known_mask&~known_mask(width))||(value->word&~width_mask(width)))return NBA97_BODY_ARGUMENT;
    for(i=0;i<width;++i)if(!(value->known_mask&(1u<<i))&&(value->word&(255u<<(8*i))))return NBA97_BODY_ARGUMENT;
    if(kind==NBA97_FRAME_READ)++r->out->reads;else ++r->out->stores;
    return NBA97_BODY_OK;
}
static int read(Run* r,uint32_t pc,uint32_t address,uint32_t* word){
    Nba97PlayerFrameValue value;memset(&value,0,sizeof value);
    TRY(access(r,pc,address,4,NBA97_FRAME_READ,&value));
    if(value.known_mask!=15)return NBA97_BODY_UNKNOWN;
    *word=value.word;return NBA97_BODY_OK;
}
static int store_half(Run* r,uint32_t pc,uint32_t address,uint32_t word){
    Nba97PlayerFrameValue value;memset(&value,0,sizeof value);
    value.word=word&65535u;value.known_mask=3;
    return access(r,pc,address,2,NBA97_FRAME_WRITE,&value);
}
static uint32_t multiply_low(uint32_t left,uint32_t right){return (uint32_t)((uint64_t)left*right);}
static uint32_t arithmetic_shift3(uint32_t word){return (word>>3)|((word&0x80000000u)?0xe0000000u:0);}
static uint32_t position(uint32_t source,uint32_t scale){
    uint32_t product=multiply_low(source,scale);
    /* Original rounds a negative low32 product toward zero by adding7 before
     * SRA3, then negates. MULT's high word is intentionally irrelevant. */
    if(product&0x80000000u)product+=7u;
    return 0u-arithmetic_shift3(product);
}
static int run(Run* r){
    uint32_t gate,x,scale,z,y;
    TRY(read(r,0x8002dc8c,0x800fdba0,&gate));if(gate)return NBA97_BODY_OK;
    TRY(read(r,0x8002dca0,0x800fc9a0,&x));TRY(read(r,0x8002dca8,0x800b2044,&scale));
    TRY(read(r,0x8002dcb0,0x800fc9b0,&z));TRY(read(r,0x8002dcb8,0x800fc9ac,&y));
    TRY(store_half(r,0x8002dcc4,0x800fa63c,0));TRY(store_half(r,0x8002dccc,0x800fa630,0));
    TRY(store_half(r,0x8002dcd4,0x800fa632,0));TRY(store_half(r,0x8002dcdc,0x800fa634,0));
    TRY(store_half(r,0x8002dcf0,0x800fa638,0x800u-z));
    TRY(store_half(r,0x8002dcf8,0x800fa63a,0x800u-y));
    TRY(read(r,0x8002dd10,0x800fc9a8,&z));
    TRY(store_half(r,0x8002dd28,0x800fab98,position(x,scale)));
    TRY(read(r,0x8002dd40,0x800fc9a4,&y));
    TRY(store_half(r,0x8002dd58,0x800fab9a,position(z,scale)));
    TRY(store_half(r,0x8002dd78,0x800fab9c,position(y,scale)));
    return NBA97_BODY_OK;
}
int nba97_game_net_transform(Nba97PlayerFrameContext* in,Nba97PlayerFrameProgress* out){
    Run r;int status;if(!out)return NBA97_BODY_ARGUMENT;memset(out,0,sizeof *out);
    if(!in||!in->access)return NBA97_BODY_ARGUMENT;
    r.in=in;r.out=out;status=run(&r);
    if(status==NBA97_BODY_OK){out->completed=1;out->stopped_pc=out->stopped_address=0;}return status;
}
