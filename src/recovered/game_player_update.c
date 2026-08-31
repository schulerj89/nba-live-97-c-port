#include "game_player_update.h"
#include <string.h>
static const uint8_t offsets[6]={0xa6,0xa8,0xa2,0xea,0x1a,0x9a};
unsigned nba97_game_player_update_offset(unsigned field){return field<6?offsets[field]:0;}
unsigned nba97_game_player_update_width(unsigned field){return field==4?1:field<6?2:0;}
static int valid_value(Nba97GamePeriodValue v,unsigned width){return v.known<=1&&(v.known||!v.word)&&v.word<(1u<<(width*8));}
static int valid_ref(Nba97GamePeriodReference v){return v.known<=1&&(v.known||!v.record);}
static int valid_state(const Nba97GamePlayerUpdateState* s){
    unsigned i,f;
    for(i=0;i<11;++i){
        if(!valid_ref(s->entity_table[i]))return 0;
        for(f=0;f<6;++f)if(!valid_value(s->entity[i][f],nba97_game_player_update_width(f)))return 0;
    }
    return valid_ref(s->current_fdc3c)&&valid_ref(s->team_fdc40)&&valid_value(s->flags_fe8c4,2);
}
static void write_ref(Nba97GamePlayerUpdateState* s,Nba97GamePlayerUpdateReceipt* receipt,
                      unsigned kind,Nba97GamePeriodReference ref){
    Nba97GamePlayerUpdateEvent* e=&receipt->event[receipt->count++];e->kind=(uint8_t)kind;e->reference=ref;
    if(kind==NBA97_UPDATE_CURRENT_REFERENCE)s->current_fdc3c=ref;else s->team_fdc40=ref;
}
static void write_value(Nba97GamePlayerUpdateState* s,Nba97GamePlayerUpdateReceipt* receipt,
                        unsigned entity,unsigned field,Nba97GamePeriodValue value){
    Nba97GamePlayerUpdateEvent* e=&receipt->event[receipt->count++];
    value.word&=65535;e->kind=NBA97_UPDATE_ENTITY_WRITE;e->entity=(uint8_t)entity;e->field=(uint8_t)field;e->value=value;
    s->entity[entity][field]=value;
}
static void assign(Nba97GamePlayerUpdateState* s,Nba97GamePlayerUpdateReceipt* receipt,
                   unsigned entity,unsigned field,uint32_t word){
    Nba97GamePeriodValue value={word,1};write_value(s,receipt,entity,field,value);
}
static int read_value(Nba97GamePeriodValue v,uint32_t* value){if(!v.known)return NBA97_PLAYER_UPDATE_UNRESOLVED;*value=v.word;return NBA97_PLAYER_UPDATE_OK;}
#define TRY(op) do{int result_=(op);if(result_!=NBA97_PLAYER_UPDATE_OK)return result_;}while(0)
static int boundary(Nba97GamePlayerUpdateState* s,Nba97GamePlayerUpdateReceipt* receipt,
                    Nba97GamePlayerUpdateCallback callback,void* context,unsigned owner,unsigned slot,unsigned entity){
    Nba97GamePlayerUpdateEvent* e=&receipt->event[receipt->count++];int result;
    e->kind=NBA97_UPDATE_CALLBACK;e->call.owner=(uint8_t)owner;e->call.slot=(uint8_t)slot;e->call.entity=(uint8_t)entity;
    e->call.callsite=owner?0x80068104:0x800680fc;
    if(!callback)return NBA97_PLAYER_UPDATE_PENDING;
    result=callback(context,s,&e->call);
    if(result!=1)return result==0?NBA97_PLAYER_UPDATE_PENDING:NBA97_PLAYER_UPDATE_CALLBACK_FAILED;
    if(!valid_state(s))return NBA97_PLAYER_UPDATE_ARGUMENT;
    e->completed=1;return NBA97_PLAYER_UPDATE_OK;
}
int nba97_game_player_update(Nba97GamePlayerUpdateState* s,Nba97GamePlayerUpdateCallback callback,
                            void* context,Nba97GamePlayerUpdateReceipt* receipt){
    unsigned slot,f;Nba97GamePeriodReference ref={0,1};
    if(!s||!receipt||!valid_state(s))return NBA97_PLAYER_UPDATE_ARGUMENT;
    memset(receipt,0,sizeof(*receipt));write_ref(s,receipt,NBA97_UPDATE_TEAM_REFERENCE,ref);
    for(slot=0;slot<10;++slot){
        uint32_t target,current,delta,actor,status;unsigned entity;
        ref=s->entity_table[slot];
        write_ref(s,receipt,NBA97_UPDATE_CURRENT_REFERENCE,ref);
        if(slot==5){Nba97GamePeriodReference away={1,1};write_ref(s,receipt,NBA97_UPDATE_TEAM_REFERENCE,away);}
        if(!ref.known)return NBA97_PLAYER_UPDATE_UNRESOLVED;
        if(ref.record>=11)return NBA97_PLAYER_UPDATE_REFERENCE;
        entity=ref.record;
        TRY(read_value(s->entity[entity][NBA97_UPDATE_A6],&target));
        TRY(read_value(s->entity[entity][NBA97_UPDATE_A8],&current));
        delta=(target-current)&1023;
        if(delta<512){
            if(delta<35)assign(s,receipt,entity,NBA97_UPDATE_A8,target);
            else{assign(s,receipt,entity,NBA97_UPDATE_A8,current+35);delta=35;}
        }else if(delta<990){
            assign(s,receipt,entity,NBA97_UPDATE_A8,current-35);delta=0u-35;
        }else{
            /* Source bug: negative-wrap snap keeps unsigned modulardelta
             *990..1023 forEA, rather than the short negative displacement. */
            assign(s,receipt,entity,NBA97_UPDATE_A8,target);
        }
        TRY(read_value(s->entity[entity][NBA97_UPDATE_1A],&actor));
        if(actor!=20){
            TRY(read_value(s->entity[entity][NBA97_UPDATE_9A],&status));
            assign(s,receipt,entity,NBA97_UPDATE_EA,delta*4);
            if(status&2)assign(s,receipt,entity,NBA97_UPDATE_EA,0u-delta*4);
        }
        TRY(boundary(s,receipt,callback,context,NBA97_UPDATE_CALL_579FC,slot,entity));
        TRY(boundary(s,receipt,callback,context,NBA97_UPDATE_CALL_6CFE0,slot,entity));
        /* Both calls can mutate angles and globals/table refs. Source still
         * normalizes this capturedS0 entity, then rereads the next table slot. */
        for(f=0;f<3;++f){
            static const uint8_t order[]={NBA97_UPDATE_A8,NBA97_UPDATE_A6,NBA97_UPDATE_A2};
            Nba97GamePeriodValue value=s->entity[entity][order[f]];value.word&=1023;
            write_value(s,receipt,entity,order[f],value);
        }
    }
    {
        Nba97GamePlayerUpdateEvent* e=&receipt->event[receipt->count++];
        s->flags_fe8c4.word&=65533;e->kind=NBA97_UPDATE_FLAGS_WRITE;e->value=s->flags_fe8c4;
    }
    receipt->completed=1;return NBA97_PLAYER_UPDATE_OK;
}
