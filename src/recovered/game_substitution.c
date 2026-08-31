#include "game_substitution.h"

static int32_t signed_half(uint16_t value) {
    return value<0x8000u ? (int32_t)value:(int32_t)value-65536;
}
static Nba97GameSubstitutionResult call_owner(Nba97GameSubstitutionState* state,
    Nba97GameSubstitutionBoundary boundary,void* context,Nba97GameSubstitutionOwner owner,
    uint8_t count,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t* result) {
    Nba97GameSubstitutionCall call;
    Nba97GameSubstitutionReply reply={0,0};
    if(!boundary)return NBA97_SUBSTITUTION_CALLBACK_REQUIRED;
    call.owner=owner;call.argument_count=count;
    call.argument[0]=a0;call.argument[1]=a1;call.argument[2]=a2;
    if(boundary(context,state,&call,&reply)!=1)return NBA97_SUBSTITUTION_CALLBACK_FAILED;
    if(result) {
        if(reply.value_known!=1)return NBA97_SUBSTITUTION_RETURN_UNKNOWN;
        *result=reply.value;
    }
    return NBA97_SUBSTITUTION_OK;
}
#define CALL(owner,count,a0,a1,a2,result) do { \
    Nba97GameSubstitutionResult status=call_owner(state,boundary,context,owner,count,a0,a1,a2,result); \
    if(status!=NBA97_SUBSTITUTION_OK)return status; \
} while(0)
#define CALL0(owner) CALL(owner,0,0,0,0,0)

Nba97GameSubstitutionResult nba97_game_substitute(
    Nba97GameSubstitutionState* state,unsigned side,int32_t active_slot,
    int32_t bench_slot,int32_t reason,uint32_t first,
    Nba97GameSubstitutionBoundary boundary,void* context) {
    Nba97GameSubstitutionTeam* team;
    uint16_t saved92,saved6c,swap;
    if(!state || side>=2)return NBA97_SUBSTITUTION_ARGUMENT;
    team=&state->team[side];saved92=state->flag92;saved6c=state->saved6c;
    state->flag92=1;team->fieldc0=0x708;
    if(state->duration58!=state->remaining60) {
        team->fieldc2=(uint16_t)(team->fieldc2-1u);
        /* Original rechecks both clock words after the decrement. */
        if(state->duration58!=state->remaining60) {
            int announce=reason<0 || signed_half(state->marker8e)<0 || (state->marker8e&0x10)!=0;
            if(!announce) {
                uint32_t query;
                /* LH/SLTI: a phase with bit15 set is below128, not a large
                 * unsigned phase. Preserve the original signed branch. */
                if(signed_half(state->phase90)<128) announce=1;
                else {
                    CALL(NBA97_SUB_31CB8,0,0,0,0,&query);
                    announce=(query&255)!=0;
                }
                if(announce)CALL(NBA97_SUB_29258,1,12,0,0,0);
            }
            if(announce) { CALL0(NBA97_SUB_64914);CALL0(NBA97_SUB_64964); }
            /* The second64914 is deliberate, not deduplicated. */
            CALL0(NBA97_SUB_64914);
            if(reason<0) {
                state->message8bc=(uint16_t)((uint32_t)reason+20u);
                state->player8c8=(uint16_t)((uint32_t)team->side14+(uint32_t)active_slot);
                CALL0(NBA97_SUB_62BFC);CALL0(NBA97_SUB_64964);
            }
        }
    }
    team->fielda2=0;
    /* Source loads bench first, then active; retain prefix on a native guard. */
    if(bench_slot<0 || bench_slot>=12)return NBA97_SUBSTITUTION_OUTSIDE_STORAGE;
    swap=team->lineup[bench_slot];
    if(active_slot<0 || active_slot>=12)return NBA97_SUBSTITUTION_OUTSIDE_STORAGE;
    { uint16_t old=team->lineup[active_slot];team->lineup[active_slot]=swap;team->lineup[bench_slot]=old; }
    if(active_slot<5) {
        uint32_t slot=(uint32_t)active_slot+team->side14;
        unsigned entity;
        if(slot>=10)return NBA97_SUBSTITUTION_OUTSIDE_STORAGE;
        entity=state->entity_table[slot];
        if(entity>=10)return NBA97_SUBSTITUTION_OUTSIDE_STORAGE;
        state->entity[entity].fielddf=0;state->entity[entity].fieldde=0;
    }
    if(state->duration58!=state->remaining60) {
        if(reason>=0) {
            if((state->marker8e&0x10)==0) {
                CALL(NBA97_SUB_35378,1,(uint32_t)signed_half(team->side14),0,0,0);
                CALL(NBA97_SUB_29258,1,12,0,0,0);CALL0(NBA97_SUB_64964);
                state->phase90=128;team->field34=(uint8_t)(team->field34-1u);
                state->delaya8=300;state->marker8e=(uint16_t)(state->marker8e|0x10);
            }
            if(team->field77==0 || state->duration58==state->remaining60)goto after_announcement;
        }
        if(state->flag86==0) {
            CALL(NBA97_SUB_29258,1,14,0,0,0);state->flag86=1;
        }
        CALL(NBA97_SUB_353A0,3,(uint32_t)signed_half(team->side14),
            (uint32_t)signed_half(team->lineup[active_slot]),
            (uint32_t)signed_half(team->lineup[bench_slot]),0);
        /* Re-read changed lineup/side after353A0. Unlike that call, these
         * source a0 loads are LHU, retaining0..65535 rather than sign-extending. */
        CALL(first ? NBA97_SUB_7F84C:NBA97_SUB_7F914,2,team->side14,
            (uint32_t)signed_half(team->lineup[active_slot]),0,0);
        CALL0(NBA97_SUB_64964);
    }
after_announcement:
    if(reason<0) {
        int32_t cursor;
        for(cursor=11;cursor!=bench_slot;--cursor) {
            int32_t player=signed_half(team->lineup[cursor]);
            if(player>=0) {
                unsigned status=(unsigned)player+(team->side14 ? 12u:0u);
                /* Source home references12..23 can cross into the away bank;
                 * retain any reached read still inside owned24-record storage. */
                if(status>=24)return NBA97_SUBSTITUTION_OUTSIDE_STORAGE;
                if(signed_half(state->status20[status])>=0) {
                    uint16_t old=team->lineup[cursor];swap=team->lineup[bench_slot];
                    team->lineup[cursor]=swap;team->lineup[bench_slot]=old;break;
                }
            }
        }
    }
    CALL0(NBA97_SUB_646A8);
    if(state->lock54==0)CALL0(NBA97_SUB_63EDC);
    CALL0(NBA97_SUB_A584C);
    state->flag92=saved92;state->saved6c=saved6c;
    return NBA97_SUBSTITUTION_OK;
}
#undef CALL0
#undef CALL
