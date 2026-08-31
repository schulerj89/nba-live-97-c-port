#include "game_body_names.h"
#include <string.h>
typedef Nba97GameBodyReference Ref;
typedef struct Run {Nba97GameBodyNamesState* state;Nba97GameBodyNameWrite* journal;size_t capacity;Nba97GameBodyNamesProgress* out;} Run;
#define TRY(expr) do{int result_=(expr);if(result_!=NBA97_BODY_OK)return result_;}while(0)
static int valid(Ref r){return r.known<=1&&(r.known||(!r.allocation&&!r.offset));}
static Ref plus(Ref r,uint32_t offset){if(r.known)r.offset+=offset;return r;}
static void stop(Run* r,uint32_t pc,unsigned kind,unsigned index,Ref ref){
    r->out->stopped_pc=pc;r->out->stopped_kind=(uint8_t)kind;r->out->index=(uint8_t)index;r->out->stopped_reference=ref;
}
static int memory(Run* r,Ref ref,unsigned width,uint32_t pc,unsigned kind,Nba97GameBodyBuffer** buffer,Nba97GameBodyCell** cell){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;uint64_t index;unsigned i;
    stop(r,pc,kind,r->out->index,ref);
    if(!valid(ref))return NBA97_BODY_ARGUMENT;
    if(!ref.known)return NBA97_BODY_UNKNOWN;
    if(ref.allocation>=r->state->buffer_count)return NBA97_BODY_BOUNDS;
    b=&r->state->buffers[ref.allocation];
    if(!b->bytes||ref.offset>b->size||width>b->size-ref.offset)return NBA97_BODY_BOUNDS;
    if(b->address_mod4_known>1||b->address_mod4>3)return NBA97_BODY_ARGUMENT;
    if(!b->address_mod4_known)return NBA97_BODY_ALIGNMENT_UNKNOWN;
    if(width==4&&((ref.offset+b->address_mod4)&3u))return NBA97_BODY_ALIGNMENT_TRAP;
    index=((uint64_t)ref.offset+b->address_mod4)/4;
    if(!b->cells||index>=b->cell_count)return NBA97_BODY_BOUNDS;
    c=&b->cells[(size_t)index];
    if(c->is_reference>1||!valid(c->reference)||(!c->is_reference&&c->reference.known))return NBA97_BODY_ARGUMENT;
    if(b->known)for(i=0;i<width;++i)if(b->known[ref.offset+i]>1)return NBA97_BODY_ARGUMENT;
    *buffer=b;*cell=c;return NBA97_BODY_OK;
}
static int read_ref(Run* r,Ref ref,uint32_t pc,Ref* value){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;unsigned i;
    TRY(memory(r,ref,4,pc,NBA97_BODY_NAME_MEMORY_REFERENCE,&b,&c));
    if(c->is_reference){*value=c->reference;return NBA97_BODY_OK;}
    if(b->known)for(i=0;i<4;++i)if(!b->known[ref.offset+i])return NBA97_BODY_UNKNOWN;
    return NBA97_BODY_REFERENCE_REQUIRED;
}
static int read_byte(Run* r,Ref ref,uint32_t pc,uint32_t* value){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;
    TRY(memory(r,ref,1,pc,NBA97_BODY_NAME_BYTE,&b,&c));
    if(c->is_reference)return c->reference.known?NBA97_BODY_ADDRESS_REQUIRED:NBA97_BODY_UNKNOWN;
    if(b->known&&!b->known[ref.offset])return NBA97_BODY_UNKNOWN;
    *value=b->bytes[ref.offset];return NBA97_BODY_OK;
}
static int center(Run* r,unsigned index,uint32_t pc,uint32_t* value){
    Nba97GamePeriodValue v=r->state->name_center[r->out->player][index];Ref empty={0,0,0};
    stop(r,pc,NBA97_BODY_NAME_CENTER,index,empty);
    if(v.known>1)return NBA97_BODY_ARGUMENT;
    if(!v.known)return NBA97_BODY_UNKNOWN;*value=v.word;return NBA97_BODY_OK;
}
static int polygon(Run* r,unsigned index,uint32_t pc,Ref* value){
    Ref v=r->state->name_polygon[r->out->player][index],empty={0,0,0};
    stop(r,pc,NBA97_BODY_NAME_POLYGON,index,empty);
    if(!valid(v))return NBA97_BODY_ARGUMENT;*value=v;return NBA97_BODY_OK;
}
static int reserve(Run* r,uint32_t pc,unsigned kind,unsigned index,Ref destination){
    stop(r,pc,kind,index,destination);
    return r->out->writes<r->capacity?NBA97_BODY_OK:NBA97_BODY_JOURNAL_LIMIT;
}
static void event(Run* r,unsigned kind,unsigned index,uint32_t pc,Ref destination,Ref value,uint32_t word){
    Nba97GameBodyNameWrite* e=&r->journal[r->out->writes++];memset(e,0,sizeof *e);
    e->kind=(uint8_t)kind;e->player=r->out->player;e->index=(uint8_t)index;e->pc=pc;e->destination=destination;e->reference=value;e->word=word;
}
static int store_polygon(Run* r,unsigned index,uint32_t pc,Ref value){
    Ref empty={0,0,0};TRY(reserve(r,pc,NBA97_BODY_NAME_POLYGON,index,empty));
    if(!valid(value)||!valid(r->state->name_polygon[r->out->player][index]))return NBA97_BODY_ARGUMENT;
    r->state->name_polygon[r->out->player][index]=value;event(r,NBA97_BODY_NAME_POLYGON,index,pc,empty,value,0);return NBA97_BODY_OK;
}
static int store_center(Run* r,unsigned index,uint32_t pc,uint32_t value){
    Nba97GamePeriodValue* v=&r->state->name_center[r->out->player][index];Ref empty={0,0,0};
    TRY(reserve(r,pc,NBA97_BODY_NAME_CENTER,index,empty));if(v->known>1)return NBA97_BODY_ARGUMENT;
    v->word=value;v->known=1;event(r,NBA97_BODY_NAME_CENTER,index,pc,empty,empty,value);return NBA97_BODY_OK;
}
static int store_byte(Run* r,unsigned index,Ref ref,uint32_t pc,uint32_t value){
    Nba97GameBodyBuffer* b;Nba97GameBodyCell* c;Ref empty={0,0,0};
    TRY(reserve(r,pc,NBA97_BODY_NAME_BYTE,index,ref));TRY(memory(r,ref,1,pc,NBA97_BODY_NAME_BYTE,&b,&c));
    /* A partial store into a native pointer cell cannot materialize its other
     * original address bytes. Do not turn zero representation metadata into
     * an invented numeric source pointer. This is a native-domain refusal. */
    if(c->is_reference)return c->reference.known?NBA97_BODY_ADDRESS_REQUIRED:NBA97_BODY_UNKNOWN;
    b->bytes[ref.offset]=(uint8_t)value;if(b->known)b->known[ref.offset]=1;
    event(r,NBA97_BODY_NAME_BYTE,index,pc,ref,empty,value&255u);return NBA97_BODY_OK;
}
int nba97_game_body_names(Nba97GameBodyNamesState* state,Nba97GameBodyNameWrite* journal,size_t capacity,Nba97GameBodyNamesProgress* out){
    Run r;unsigned player,bank,index;Ref context,header,base,packet,p0,p1;uint32_t u0,u2,old,mid0,mid1,current;
    if(!state||!out||(!journal&&capacity)||(!state->buffers&&state->buffer_count))return NBA97_BODY_ARGUMENT;
    memset(out,0,sizeof *out);r.state=state;r.journal=journal;r.capacity=capacity;r.out=out;
    for(player=0;player<10;++player){out->player=(uint8_t)player;
        for(bank=0;bank<2;++bank){index=bank*2;out->index=(uint8_t)index;
            /* Reload the whole F0ED8 context root each bank. Header+5E4 is live,
             * including the second read after publishing the first packet. */
            context=plus(state->contexts_f0ed8,player*0xbccu);
            TRY(read_ref(&r,plus(context,0x5e4),0x800505bc,&header));
            TRY(read_ref(&r,plus(header,8+bank*4),0x800505c8,&base));
            TRY(store_polygon(&r,index,0x800505e4,plus(base,0x40)));
            TRY(read_ref(&r,plus(context,0x5e4),0x800505e8,&header));
            TRY(read_ref(&r,plus(header,8+bank*4),0x800505f4,&base));
            TRY(polygon(&r,index,0x80050600,&p0));
            TRY(store_polygon(&r,index+1,0x80050610,plus(base,0x80)));
            out->index=(uint8_t)index;
            TRY(read_byte(&r,plus(p0,0x1c),0x80050614,&u2));
            TRY(read_byte(&r,plus(p0,0xc),0x80050618,&u0));
            /* Original bug/quirk: capture only the OLD FIRST center once. All
             * six later U expressions reuse it, including packet1. Incoming
             * second-center content is never read. Do not substitute either
             * new midpoint or a guessed width when this input is unknown. */
            TRY(center(&r,index,0x80050624,&old));TRY(polygon(&r,index+1,0x80050630,&p1));
            /* lbu endpoints + signed arithmetic shift: floor, not truncating
             * signed division for a negative odd difference. */
            mid0=(u0+u2)>>1;TRY(store_center(&r,index,0x80050648,mid0));
            out->index=(uint8_t)(index+1);
            TRY(read_byte(&r,plus(p1,0x1c),0x8005064c,&u2));
            TRY(read_byte(&r,plus(p1,0xc),0x80050650,&u0));
            TRY(polygon(&r,index,0x8005065c,&packet));TRY(center(&r,index,0x80050668,&current));
            mid1=(u0+u2)>>1;TRY(store_center(&r,index+1,0x80050684,mid1));
            TRY(store_byte(&r,index,plus(packet,0xc),0x80050688,current-old));
            TRY(center(&r,index,0x80050694,&current));TRY(polygon(&r,index,0x800506a0,&packet));
            TRY(store_byte(&r,index,plus(packet,0x14),0x800506a8,current-old));
            TRY(center(&r,index,0x800506b4,&current));TRY(polygon(&r,index,0x800506c0,&packet));
            TRY(store_byte(&r,index,plus(packet,0x1c),0x800506cc,current+old-1u));
            TRY(center(&r,index+1,0x800506d8,&current));TRY(polygon(&r,index+1,0x800506e4,&packet));
            TRY(store_byte(&r,index+1,plus(packet,0xc),0x800506f0,current+old-1u));
            TRY(center(&r,index+1,0x800506fc,&current));TRY(polygon(&r,index+1,0x80050708,&packet));
            TRY(store_byte(&r,index+1,plus(packet,0x14),0x80050714,current+old-1u));
            TRY(center(&r,index+1,0x80050720,&current));TRY(polygon(&r,index+1,0x8005072c,&packet));
            TRY(store_byte(&r,index+1,plus(packet,0x1c),0x80050738,current-old));++out->banks_completed;
        }
        ++out->players_completed;
    }
    out->completed=1;out->stopped_pc=0;out->stopped_kind=0;out->index=0;out->player=0;memset(&out->stopped_reference,0,sizeof out->stopped_reference);
    return NBA97_BODY_OK;
}
