#include "game_period.h"
#include <string.h>

unsigned nba97_game_period_scalar_width(unsigned f)
{
    if(f>=NBA97_PERIOD_SCALAR_COUNT) return 0;
    if(f==NBA97_PERIOD_21D73) return 1;
    switch(f) {
    case NBA97_PERIOD_FA034: case NBA97_PERIOD_FE898: case NBA97_PERIOD_FDBA4:
    case NBA97_PERIOD_FDB60: case NBA97_PERIOD_FDB58: case NBA97_PERIOD_FDB5C:
    case NBA97_PERIOD_FDB64: return 4;
    default: return 2;
    }
}
unsigned nba97_game_period_entity_width(unsigned f)
{
    if(f>=NBA97_PERIOD_ENTITY_FIELD_COUNT) return 0;
    if(f==NBA97_PERIOD_ENTITY_D9) return 1;
    switch(f) {
    case NBA97_PERIOD_ENTITY_00: case NBA97_PERIOD_ENTITY_08:
    case NBA97_PERIOD_ENTITY_0C: case NBA97_PERIOD_ENTITY_10:
    case NBA97_PERIOD_ENTITY_AC: return 4;
    default: return 2;
    }
}
static int value_valid(Nba97GamePeriodValue v,unsigned width)
{
    if(v.known>1 || (!v.known && v.word)) return 0;
    return width==4 || v.word<(1u<<(width*8));
}
static int reference_valid(Nba97GamePeriodReference r)
{ return r.known<=1 && (r.known || !r.record); }
static int state_valid(const Nba97GamePeriodState* s)
{
    unsigned i,j;
    for(i=0;i<NBA97_PERIOD_SCALAR_COUNT;++i)
        if(!value_valid(s->scalar[i],nba97_game_period_scalar_width(i))) return 0;
    for(i=0;i<2;++i) for(j=0;j<NBA97_PERIOD_TEAM_FIELD_COUNT;++j)
        if(!value_valid(s->team[i][j],j==NBA97_PERIOD_TEAM_10?4:1)) return 0;
    for(i=0;i<11;++i) {
        for(j=0;j<NBA97_PERIOD_ENTITY_FIELD_COUNT;++j)
            if(!value_valid(s->entity[i][j],nba97_game_period_entity_width(j))) return 0;
        if(!reference_valid(s->entity_table[i]) || !reference_valid(s->render_table[i])) return 0;
    }
    for(i=0;i<8;++i)
        if(!value_valid(s->controller22[i],2) || !reference_valid(s->controller_table[i])) return 0;
    return reference_valid(s->ball_fdc48) && reference_valid(s->reference_fdc34) &&
           value_valid(s->incoming_s6,4);
}
static int32_t signed32(uint32_t v)
{ return v<=0x7fffffffu?(int32_t)v:-1-(int32_t)(~v); }
static int32_t signed16(uint32_t v)
{ return v<0x8000u?(int32_t)v:(int32_t)v-65536; }
static int32_t signed8(uint32_t v)
{ return v<128u?(int32_t)v:(int32_t)v-256; }
typedef struct Run {
    Nba97GamePeriodState* s;
    Nba97GamePeriodReceipt* r;
    Nba97GamePeriodCallback callback;
    void* context;
    Nba97GamePeriodValue incoming_s6;
} Run;
static void write_value(Run* run,unsigned target,unsigned record,unsigned field,uint32_t value)
{
    Nba97GamePeriodEvent* e=&run->r->event[run->r->count++];
    Nba97GamePeriodValue* v=0;
    Nba97GamePeriodReference* ref=0;
    unsigned width=4;
    switch(target) {
    case NBA97_PERIOD_TARGET_SCALAR: v=&run->s->scalar[field]; width=nba97_game_period_scalar_width(field); break;
    case NBA97_PERIOD_TARGET_TEAM: v=&run->s->team[record][field]; width=field==NBA97_PERIOD_TEAM_10?4:1; break;
    case NBA97_PERIOD_TARGET_ENTITY: v=&run->s->entity[record][field]; width=nba97_game_period_entity_width(field); break;
    case NBA97_PERIOD_TARGET_CONTROLLER22: v=&run->s->controller22[record]; width=2; break;
    case NBA97_PERIOD_TARGET_ENTITY_TABLE: ref=&run->s->entity_table[record]; break;
    case NBA97_PERIOD_TARGET_RENDER_TABLE: ref=&run->s->render_table[record]; break;
    case NBA97_PERIOD_TARGET_BALL_FDC48: ref=&run->s->ball_fdc48; break;
    case NBA97_PERIOD_TARGET_REFERENCE_FDC34: ref=&run->s->reference_fdc34; break;
    }
    if(width<4) value&=(1u<<(width*8))-1;
    e->kind=NBA97_PERIOD_EVENT_WRITE; e->target=(uint8_t)target;
    e->record=(uint8_t)record; e->field=(uint8_t)field; e->width=(uint8_t)width; e->value=value;
    if(v) { v->word=value; v->known=1; }
    if(ref) { ref->record=(uint8_t)value; ref->known=1; }
}
static int call_owner(Run* run,unsigned owner,uint32_t pc,int32_t arg,
                      unsigned side,unsigned formation,unsigned entity)
{
    Nba97GamePeriodEvent* e=&run->r->event[run->r->count++];
    int result;
    e->kind=NBA97_PERIOD_EVENT_CALL; e->call.owner=(uint8_t)owner;
    e->call.callsite=pc; e->call.argument=arg; e->call.side=(uint8_t)side;
    e->call.formation=(uint8_t)formation; e->call.entity=(uint8_t)entity;
    if(owner==NBA97_PERIOD_CALL_653E8) e->call.incoming_s6=run->incoming_s6;
    if(!run->callback) return NBA97_PERIOD_CALLBACK_PENDING;
    result=run->callback(run->context,run->s,&e->call);
    if(result==0) return NBA97_PERIOD_CALLBACK_PENDING;
    if(result!=1) return NBA97_PERIOD_CALLBACK_FAILED;
    e->call_completed=1; ++run->r->completed_calls;
    return state_valid(run->s)?NBA97_PERIOD_COMPLETE:NBA97_PERIOD_ARGUMENT;
}
static int resolve(Nba97GamePeriodReference ref,unsigned limit,unsigned* record)
{
    if(!ref.known) return NBA97_PERIOD_UNRESOLVED;
    if(ref.record>=limit) return NBA97_PERIOD_REFERENCE;
    *record=ref.record; return NBA97_PERIOD_COMPLETE;
}
#define TRY(operation) do { int result_=(operation); if(result_!=NBA97_PERIOD_COMPLETE) return result_; } while(0)
#define READ(destination,source) do { Nba97GamePeriodValue read_=(source); if(!read_.known) return NBA97_PERIOD_UNRESOLVED; (destination)=read_.word; } while(0)
#define SC(field,value) write_value(&run,NBA97_PERIOD_TARGET_SCALAR,0,NBA97_PERIOD_##field,(uint32_t)(value))
#define TM(side,field,value) write_value(&run,NBA97_PERIOD_TARGET_TEAM,side,NBA97_PERIOD_TEAM_##field,(uint32_t)(value))
#define EN(field,value) write_value(&run,NBA97_PERIOD_TARGET_ENTITY,10,NBA97_PERIOD_ENTITY_##field,(uint32_t)(value))
#define CALL(owner,pc,arg,side,formation,entity) TRY(call_owner(&run,NBA97_PERIOD_CALL_##owner,pc,arg,side,formation,entity))

int nba97_game_period_initialize(Nba97GamePeriodState* s,const Nba97GamePeriodDurations* durations,
                                 Nba97GamePeriodCallback callback,void* context,Nba97GamePeriodReceipt* receipt)
{
    Run run;
    uint32_t quarter_word,option,launch,duration,home,away,x,y,side_word;
    int32_t quarter,divisor,special=-1;
    unsigned i,record,formation=NBA97_PERIOD_FORMATION_B891C;
    if(!s || !durations || !receipt || !state_valid(s)) return NBA97_PERIOD_ARGUMENT;
    memset(receipt,0,sizeof(*receipt));
    run.s=s; run.r=receipt; run.callback=callback; run.context=context; run.incoming_s6=s->incoming_s6;
    CALL(646A8,0x80065dc4u,0,0,0,0);
    READ(quarter_word,s->scalar[NBA97_PERIOD_FDB68]); quarter=signed16(quarter_word);
    receipt->captured_quarter=(int16_t)quarter; receipt->captured_quarter_known=1;
    /* Original only sets this marker in quarter4; other quarters retain it. */
    if(quarter==4) SC(1EDF2,1);
    for(i=NBA97_PERIOD_FE90C;i<=NBA97_PERIOD_FDBB4;++i)
        write_value(&run,NBA97_PERIOD_TARGET_SCALAR,0,i,0);
    READ(option,s->scalar[NBA97_PERIOD_21D73]);
    for(i=NBA97_PERIOD_FDBB2;i<=NBA97_PERIOD_FDB7C;++i)
        write_value(&run,NBA97_PERIOD_TARGET_SCALAR,0,i,0);
    READ(launch,s->scalar[NBA97_PERIOD_1EDEC]);
    SC(FA034,0xffffffffu); SC(FE898,0);
    for(i=NBA97_PERIOD_FE87E;i<=NBA97_PERIOD_FE8E4;++i)
        write_value(&run,NBA97_PERIOD_TARGET_SCALAR,0,i,0xffff);
    /* Unchecked original byte-index lookup deliberately includes adjacent data. */
    duration=durations->value[quarter==4?1:0][option];
    if(launch) duration=0x1518;
    SC(FDBA4,0x5a0); SC(FE86E,3); SC(FDB60,duration); SC(FDB58,duration); SC(FDB76,30);
    divisor=signed32(duration)/1800;
    if(divisor<10) divisor=10;
    SC(FDB5C,3600); SC(FDB64,divisor); SC(FDB7E,805/divisor);
    for(i=0;i<8;++i) {
        READ(duration,s->scalar[NBA97_PERIOD_FDB58]);
        TRY(resolve(s->controller_table[i],8,&record));
        write_value(&run,NBA97_PERIOD_TARGET_CONTROLLER22,record,0,(uint32_t)(signed32(duration)/3600));
    }
    CALL(65140,0x800660a8u,120,0,0,0);
    if(quarter==4) {
        TM(1,34,3); TM(0,34,3); SC(FDB76,30); SC(FDB5C,7200);
    } else if(quarter!=0) {
        formation=NBA97_PERIOD_FORMATION_B893C; special=0;
        if(quarter==3) {
            SC(FDB76,30); SC(FDB5C,7200);
            READ(home,s->team[0][NBA97_PERIOD_TEAM_34]);
            READ(away,s->team[1][NBA97_PERIOD_TEAM_34]);
            /* Signed LB preserves negative byte values; do not normalize them. */
            TM(0,34,signed8(home)>=5?4:home); TM(1,34,signed8(away)>=5?4:away);
        }
        if(quarter==2) {
            CALL(65140,0x80066184u,600,0,0,0);
            READ(home,s->team[0][NBA97_PERIOD_TEAM_10]);
            READ(away,s->team[1][NBA97_PERIOD_TEAM_10]);
            TM(1,35,1); TM(0,35,1);
            /* NEGU wraps; INT_MIN intentionally remains INT_MIN. */
            TM(0,10,0u-home); TM(1,10,0u-away);
        }
    }
    CALL(65B18,0x800661d8u,special,0,formation,0);
    CALL(65B18,0x800661e8u,special,5,formation,0);
    write_value(&run,NBA97_PERIOD_TARGET_BALL_FDC48,0,0,10);
    write_value(&run,NBA97_PERIOD_TARGET_ENTITY_TABLE,10,0,10);
    EN(06,10);
    write_value(&run,NBA97_PERIOD_TARGET_REFERENCE_FDC34,0,0,10);
    write_value(&run,NBA97_PERIOD_TARGET_RENDER_TABLE,10,0,10);
    EN(10,0x5c00); EN(18,0x2d0); SC(FE8AA,1); EN(D9,0xff); EN(00,10);
    EN(B4,0); EN(AC,0); EN(16,0); EN(0C,0); EN(14,0); EN(08,0);
    CALL(653E8,0x80066274u,0,0,0,0);
    CALL(646A8,0x8006627cu,0,0,0,0);
    CALL(60EF8,0x80066284u,0,0,0,0);
    CALL(5828C,0x8006628cu,0,0,0,0);
    SC(FDB96,0xffff); SC(FDB94,0xffff); SC(FDBAA,600);
    if(special) {
        SC(FDB90,0x81); SC(FDB7C,120); EN(BA,360);
        TRY(resolve(s->entity_table[0],11,&record));
        CALL(56B78,0x800662e0u,0x27,0,0,record);
        /* Second reference is read after the first setter has completed. */
        TRY(resolve(s->entity_table[5],11,&record));
        CALL(56B78,0x800662f0u,0x27,0,0,record);
    } else {
        READ(side_word,s->scalar[NBA97_PERIOD_FDB72]);
        side_word=(uint32_t)signed16(side_word);
        if(quarter!=3) side_word^=5;
        /* Source treats EVERY nonzero signed/XOR result as away, yet stores the
         * unnormalized low16 result in the side fields. Preserve this quirk. */
        TRY(resolve(s->entity_table[side_word?5:0],11,&record));
        SC(FDB96,side_word); SC(FDB94,side_word); SC(FE880,side_word); SC(FE882,0); SC(FDB90,0x82);
        READ(x,s->entity[record][NBA97_PERIOD_ENTITY_08]); EN(08,x);
        READ(y,s->entity[record][NBA97_PERIOD_ENTITY_0C]);
        /* s0 remains the physical ball10 even if a callback changed FDC48. */
        EN(10,0x400); EN(18,0); EN(0C,y);
    }
    receipt->complete=1;
    return NBA97_PERIOD_COMPLETE;
}
